// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

// winsock2.h must come before any Qt header on Windows — QtCore
// internally pulls in windows.h, which auto-includes the legacy
// winsock.h and breaks the winsock2.h include order if it lands
// second.
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

#include "ssh/ssh_session.h"
#include "ssh/ssh_tunnel.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QtTest>

#include <libssh/libssh.h>
#include <libssh/server.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <atomic>
#include <memory>

#ifndef _WIN32
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#endif

using namespace qumesh::ssh;

namespace {

/// Generate an ephemeral RSA-2048 host key for the mock SSH server,
/// as a PEM blob suitable for `SSH_BIND_OPTIONS_IMPORT_KEY_STR`.
/// Borrowed shape from `redir/tests/test_redir_tls.cpp` (the
/// self-signed cert helper) — the test-only crypto cost is the same.
QByteArray generateHostKeyPem()
{
    EVP_PKEY *pkey = EVP_RSA_gen(2048);
    Q_ASSERT(pkey != nullptr);
    BIO *kb = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(kb, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    char *kp = nullptr;
    const long kn = BIO_get_mem_data(kb, &kp);
    QByteArray pem(kp, static_cast<int>(kn));
    BIO_free(kb);
    EVP_PKEY_free(pkey);
    return pem;
}

/// Minimal libssh-backed mock SSH server. Accepts one client at a
/// time, does the handshake, takes whatever the client offers for
/// auth, accepts the first direct-tcpip channel, then loops reading
/// from the channel and discarding the bytes. Two atomic counters
/// expose progress so the test can verify (a) the channel reached
/// open, and (b) the server saw at least one byte of pumped data
/// before being torn down.
///
/// Lives on its own QThread so destruction doesn't race the libssh
/// poll loop — the test calls `stop()` (sets a stop flag), then
/// `wait()` on the QThread before the test exits.
class MockSshServer : public QThread
{
    Q_OBJECT
public:
    MockSshServer()
        : m_bind(ssh_bind_new())
    {
        Q_ASSERT(m_bind != nullptr);
        m_hostKeyPem = generateHostKeyPem();
        // Bind to localhost on an ephemeral port. We grab the port
        // back via getsockname() after listen().
        const char *addr = "127.0.0.1";
        const int port = 0;
        const int verbosity = SSH_LOG_NOLOG;
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_BINDADDR, addr);
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_BINDPORT, &port);
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &verbosity);
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_IMPORT_KEY_STR,
                              m_hostKeyPem.constData());
    }
    ~MockSshServer() override
    {
        stop();
        if (isRunning()) wait();
        ssh_bind_free(m_bind);
    }

    bool listen()
    {
        if (ssh_bind_listen(m_bind) != SSH_OK) return false;
        // libssh's ssh_bind allocates the socket internally; grab
        // the OS-assigned port back.
        const int fd = ssh_bind_get_fd(m_bind);
        sockaddr_storage ss{};
        socklen_t len = sizeof(ss);
        if (::getsockname(fd, reinterpret_cast<sockaddr *>(&ss), &len) != 0)
            return false;
        if (ss.ss_family == AF_INET) {
            m_port = ntohs(reinterpret_cast<sockaddr_in *>(&ss)->sin_port);
        } else {
            m_port = ntohs(reinterpret_cast<sockaddr_in6 *>(&ss)->sin6_port);
        }
        return true;
    }
    quint16 port() const { return m_port; }
    bool channelOpened() const { return m_channelOpened.loadRelaxed() != 0; }
    int bytesPumped() const { return m_bytesPumped.loadRelaxed(); }
    void stop() { m_stop.store(1, std::memory_order_relaxed); }

protected:
    void run() override
    {
        // ssh_bind_accept blocks; gate with a select on the listener
        // socket so the stop flag can break the loop without waiting
        // for an inbound TCP connection. select() (rather than poll())
        // because Qt's only cross-platform poll is QSocketNotifier,
        // and the existing SshTunnel uses select() on Windows too.
        const int listenFd = ssh_bind_get_fd(m_bind);
        while (m_stop.load(std::memory_order_relaxed) == 0) {
            fd_set rfds;
            FD_ZERO(&rfds);
#ifdef Q_OS_WIN
            FD_SET(static_cast<SOCKET>(listenFd), &rfds);
#else
            FD_SET(listenFd, &rfds);
#endif
            timeval tv{0, 100 * 1000};
            const int sr = ::select(listenFd + 1, &rfds, nullptr, nullptr, &tv);
            if (sr < 0) break;
            if (sr == 0) continue;

            ssh_session sess = ssh_new();
            if (sess == nullptr) continue;
            if (ssh_bind_accept(m_bind, sess) != SSH_OK) {
                ssh_free(sess);
                continue;
            }
            handleSession(sess);
            ssh_disconnect(sess);
            ssh_free(sess);
        }
    }

private:
    void handleSession(ssh_session sess)
    {
        if (ssh_handle_key_exchange(sess) != SSH_OK) return;
        ssh_set_auth_methods(sess, SSH_AUTH_METHOD_PASSWORD);

        ssh_channel chan = nullptr;
        // Message loop: handle auth + channel-open, capture the
        // first channel, then break out to the pump loop. Bail if
        // anything goes sideways — the test owns the timing budget.
        while (m_stop.load(std::memory_order_relaxed) == 0) {
            ssh_message msg = ssh_message_get(sess);
            if (msg == nullptr) return;
            const int type = ssh_message_type(msg);
            if (type == SSH_REQUEST_AUTH) {
                // Accept whatever password the client offered.
                ssh_message_auth_reply_success(msg, /*partial=*/0);
                ssh_message_free(msg);
                continue;
            }
            if (type == SSH_REQUEST_CHANNEL_OPEN) {
                chan = ssh_message_channel_request_open_reply_accept(msg);
                ssh_message_free(msg);
                if (chan == nullptr) return;
                break;
            }
            ssh_message_reply_default(msg);
            ssh_message_free(msg);
        }

        m_channelOpened.storeRelaxed(1);

        // Pump loop: read whatever the client sends and drop it.
        // The non-blocking read returns SSH_AGAIN immediately when no
        // data is buffered; sleep a beat to keep CPU at zero. Exits
        // when the client tears the channel down or when stop() flips.
        char buf[4096];
        while (m_stop.load(std::memory_order_relaxed) == 0) {
            const int n = ssh_channel_read_nonblocking(chan, buf,
                                                        sizeof(buf), 0);
            if (n == SSH_ERROR) break;
            if (n == SSH_EOF || ssh_channel_is_eof(chan)) break;
            if (n > 0) {
                m_bytesPumped.fetchAndAddRelaxed(n);
            } else {
                QThread::msleep(10);
            }
        }
        ssh_channel_close(chan);
        ssh_channel_free(chan);
    }

    ssh_bind m_bind;
    QByteArray m_hostKeyPem;
    quint16 m_port = 0;
    std::atomic<int> m_stop{0};
    QAtomicInt m_channelOpened{0};
    QAtomicInt m_bytesPumped{0};
};

template <typename Pred>
bool waitFor(int ms, Pred p)
{
    QElapsedTimer t;
    t.start();
    while (!p()) {
        if (t.elapsed() > ms) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return true;
}

/// Background pump that writes a continuous stream of bytes into the
/// tunnel's Qt-side socket fd while the test runs. The whole point is
/// to make sure SshSession destruction interrupts an in-flight pump
/// rather than waiting for it to drain.
class TunnelByteWriter : public QThread
{
    Q_OBJECT
public:
    explicit TunnelByteWriter(qintptr fd) : m_fd(fd) {}
    void stop() { m_stop.store(1, std::memory_order_relaxed); }
    int bytesSent() const { return m_sent.loadRelaxed(); }
protected:
    void run() override
    {
        QTcpSocket s;
        s.setSocketDescriptor(m_fd, QAbstractSocket::ConnectedState,
                              QIODevice::ReadWrite);
        QByteArray chunk(4096, 'x');
        while (m_stop.load(std::memory_order_relaxed) == 0 && s.state() == QAbstractSocket::ConnectedState) {
            const qint64 w = s.write(chunk);
            if (w > 0) {
                m_sent.fetchAndAddRelaxed(int(w));
                if (!s.waitForBytesWritten(100)) {
                    // No headroom — yield and try again. We're not
                    // racing for throughput, just keeping the pump
                    // busy.
                    QThread::msleep(5);
                }
            } else {
                QThread::msleep(5);
            }
        }
        s.disconnectFromHost();
    }
private:
    qintptr m_fd;
    std::atomic<int> m_stop{0};
    QAtomicInt m_sent{0};
};

} // namespace

class TestSshTunnelShutdown : public QObject
{
    Q_OBJECT
private slots:
    void destructorReturnsPromptlyWhilePumpIsBusy();
    void closeBeforeChannelOpenEmitsFailed();
};

void TestSshTunnelShutdown::destructorReturnsPromptlyWhilePumpIsBusy()
{
    // The #233 regression guard in `test_ssh_session.cpp` covers
    // destruction during `ssh_connect`. This test covers the other
    // half: destruction after an SshTunnel is open and actively
    // pumping bytes via its BlockingQueuedConnection-into-the-worker
    // pattern. Pre-fix, the pump's QThread could be parked inside a
    // libssh call when the session went away, blocking ~SshSession
    // for the full connect timeout.

    MockSshServer server;
    QVERIFY(server.listen());

    auto session = std::make_unique<SshSession>();
    SshSession::Params p;
    p.host = QStringLiteral("127.0.0.1");
    p.port = server.port();
    p.user = QStringLiteral("test");
    p.password = QStringLiteral("anything"); // mock accepts any
    p.connectTimeoutMs = 15000;

    server.start();
    QSignalSpy stateSpy(session.get(), &SshSession::stateChanged);
    // The mock server's host key is ephemeral and obviously unknown
    // to the client. Auto-accept the prompt so the session advances
    // past NeedsHostKeyTrust into Authenticating.
    QObject::connect(session.get(), &SshSession::hostKeyPromptRequired,
                     session.get(),
                     [&](const QString &, const QString &) {
                         session->trustPendingHostKey();
                     });
    session->open(p);
    // Walk through Connecting → Handshaking → Authenticated. We
    // accept any intermediate state on the way; just need to land
    // somewhere usable for a channel.
    QVERIFY2(waitFor(8000, [&] {
                 return session->state() == SshSession::Connected;
             }),
             qPrintable(QStringLiteral(
                 "session never reached Connected; last state=%1, lastError='%2'")
                 .arg(int(session->state()))
                 .arg(session->lastError())));

    SshTunnel tunnel(session.get(), QStringLiteral("127.0.0.1"), 12345);
    QSignalSpy openedSpy(&tunnel, &SshTunnel::opened);
    QSignalSpy failedSpy(&tunnel, &SshTunnel::failed);
    tunnel.open();
    QVERIFY(waitFor(8000, [&] {
        return openedSpy.size() == 1 || failedSpy.size() == 1;
    }));
    QCOMPARE(failedSpy.size(), 0);
    QCOMPARE(openedSpy.size(), 1);
    const qintptr fd = openedSpy.first().value(0).value<qintptr>();
    QVERIFY(fd >= 0);
    QVERIFY(waitFor(2000, [&] { return server.channelOpened(); }));

    // Start writing bytes into the tunnel's local fd. The writer
    // owns the fd once handed off; SshTunnel never closes it again.
    TunnelByteWriter writer(fd);
    writer.start();
    QVERIFY(waitFor(2000, [&] { return writer.bytesSent() > 0; }));
    // And confirm the mock saw them — proves the pump is actually
    // in flight when we kill the session.
    QVERIFY(waitFor(2000, [&] { return server.bytesPumped() > 0; }));

    // Now tear the session down with a live tunnel + active pump.
    // The destructor must return well inside the 15 s connect-timeout
    // window — anything in between is the bug we're guarding against.
    QElapsedTimer t;
    t.start();
    session.reset();
    const qint64 elapsed = t.elapsed();
    QVERIFY2(elapsed < 1500,
             qPrintable(QStringLiteral(
                 "~SshSession took %1 ms with a live tunnel — "
                 "expected <1500 ms").arg(elapsed)));

    // Drain the writer thread before letting `server` go out of
    // scope — it owns a QTcpSocket bound to `fd`, and we don't want
    // its destructor racing the test exit.
    writer.stop();
    writer.wait(2000);
}

void TestSshTunnelShutdown::closeBeforeChannelOpenEmitsFailed()
{
    // Regression guard for #372. SshTunnelOpener wires deleteLater
    // off opened → closed and off failed. Pre-fix, SshTunnel::close()
    // only emitted closed() when d->opened was already true — so if
    // close() landed between open() (which only *posts* work to the
    // session worker) and the worker handing back a channel, neither
    // signal ever fired and the heap tunnel leaked for the lifetime
    // of the process. Reproduce that race here by calling close()
    // synchronously after open(), before the worker thread has had a
    // chance to run.
    //
    // The session has to be Connected so open() takes the async-post
    // branch (rather than the early-return "not connected" branch
    // which already emits failed). Reuse the MockSshServer fixture
    // from the pump-busy test — we don't actually exercise the
    // server's channel-accept path; we just need a Connected session.

    MockSshServer server;
    QVERIFY(server.listen());

    auto session = std::make_unique<SshSession>();
    SshSession::Params p;
    p.host = QStringLiteral("127.0.0.1");
    p.port = server.port();
    p.user = QStringLiteral("test");
    p.password = QStringLiteral("anything");
    p.connectTimeoutMs = 15000;

    server.start();
    QObject::connect(session.get(), &SshSession::hostKeyPromptRequired,
                     session.get(),
                     [&](const QString &, const QString &) {
                         session->trustPendingHostKey();
                     });
    session->open(p);
    QVERIFY2(waitFor(8000, [&] {
                 return session->state() == SshSession::Connected;
             }),
             qPrintable(QStringLiteral(
                 "session never reached Connected; last state=%1, lastError='%2'")
                 .arg(int(session->state()))
                 .arg(session->lastError())));

    // Allocate on the heap to mirror the makeSshTunnelOpener pattern
    // (`new SshTunnel(...)` with deleteLater wired off failed). A
    // QSignalSpy on destroyed() is the sentinel for "the deleteLater
    // chain ran" — i.e. the leak that #372 was about.
    auto *tunnel = new SshTunnel(session.get(), QStringLiteral("127.0.0.1"), 12345);
    QSignalSpy openedSpy(tunnel, &SshTunnel::opened);
    QSignalSpy failedSpy(tunnel, &SshTunnel::failed);
    QSignalSpy closedSpy(tunnel, &SshTunnel::closed);
    QSignalSpy destroyedSpy(tunnel, &QObject::destroyed);

    // Same wiring as makeSshTunnelOpener: deleteLater on failed.
    QObject::connect(tunnel, &SshTunnel::failed, tunnel, &QObject::deleteLater);

    tunnel->open();
    // Race window: close() before the worker thread can finish its
    // queued openForwardChannel + post back. open() itself only posts
    // to the worker, so the close runs first.
    tunnel->close();

    // Synthetic failed must have fired by close() return.
    QCOMPARE(failedSpy.size(), 1);
    QCOMPARE(openedSpy.size(), 0);
    QCOMPARE(closedSpy.size(), 0);

    // deleteLater runs on the next event-loop tick. QTRY waits a
    // bounded interval for the destruction sentinel to fire — that's
    // the actual leak fix.
    QTRY_COMPARE(destroyedSpy.size(), 1);

    // The worker may still queue a stale onChannelOpened back; let the
    // event loop drain so anything left in flight runs (and the
    // abandoned-flag short-circuit fires) before we tear the session
    // down. No assertions here — we just don't want background work
    // outliving the test.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);

    session.reset();
}

QTEST_GUILESS_MAIN(TestSshTunnelShutdown)
#include "test_ssh_tunnel_shutdown.moc"
