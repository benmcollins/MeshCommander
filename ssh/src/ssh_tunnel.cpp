// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh/ssh_tunnel.h"
#include "ssh/ssh_session.h"
#include "ssh_session_worker.h"

#include <QPointer>
#include <QThread>

#include <libssh/libssh.h>

#include <atomic>
#include <memory>

#ifdef Q_OS_WIN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <afunix.h>
#  pragma comment(lib, "ws2_32.lib")
   using socklen_t_compat = int;
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <poll.h>
#  include <errno.h>
   using socklen_t_compat = socklen_t;
#endif

namespace qumesh::ssh {

class SshSessionWorker;

namespace {

/// Build a pair of connected stream sockets via a transient 127.0.0.1
/// listener: bind to an ephemeral port, accept exactly one connection,
/// close the listener. The listening port is open for only the few
/// microseconds between `listen()` and `accept()`, then gone.
///
/// Previously the POSIX path used `socketpair(AF_UNIX, SOCK_STREAM)`
/// which is conceptually cleaner — no port ever bound — but
/// `QSslSocket::setSocketDescriptor` / `QTcpSocket::setSocketDescriptor`
/// have TCP-shaped assumptions and don't drive an AF_UNIX fd reliably
/// (no `readyRead` after writes that should have triggered it, TLS
/// handshake never completing, etc.). Both platforms now share this
/// transient-loopback pattern so the failure mode is the same.
int makeSocketPair(qintptr fds[2])
{
#ifdef Q_OS_WIN
    SOCKET listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0
        || ::listen(listener, 1) != 0) {
        ::closesocket(listener);
        return -1;
    }
    socklen_t_compat alen = sizeof(addr);
    if (::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &alen) != 0) {
        ::closesocket(listener);
        return -1;
    }

    SOCKET client = ::socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET) {
        ::closesocket(listener);
        return -1;
    }
    if (::connect(client, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::closesocket(client);
        ::closesocket(listener);
        return -1;
    }
    SOCKET server = ::accept(listener, nullptr, nullptr);
    ::closesocket(listener);
    if (server == INVALID_SOCKET) {
        ::closesocket(client);
        return -1;
    }
    fds[0] = static_cast<qintptr>(server);
    fds[1] = static_cast<qintptr>(client);
    return 0;
#else
    int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return -1;

    int one = 1;
    ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0
        || ::listen(listener, 1) != 0) {
        ::close(listener);
        return -1;
    }
    socklen_t alen = sizeof(addr);
    if (::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &alen) != 0) {
        ::close(listener);
        return -1;
    }

    int client = ::socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0) {
        ::close(listener);
        return -1;
    }
    if (::connect(client, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(client);
        ::close(listener);
        return -1;
    }
    // Capture the client's source port so we can verify the accept()
    // peer matches — see the post-accept check below.
    sockaddr_in clientAddr{};
    socklen_t clen = sizeof(clientAddr);
    if (::getsockname(client, reinterpret_cast<sockaddr *>(&clientAddr), &clen) != 0) {
        ::close(client);
        ::close(listener);
        return -1;
    }
    sockaddr_in peerAddr{};
    socklen_t plen = sizeof(peerAddr);
    int server = ::accept(listener, reinterpret_cast<sockaddr *>(&peerAddr), &plen);
    ::close(listener);
    if (server < 0) {
        ::close(client);
        return -1;
    }
    // Tiny race window between listen() and accept() lets another
    // local process connect() to our loopback port and impersonate
    // the client. They can't bypass downstream auth, but they can
    // feed garbage into the redir parser. Verify the accepted peer's
    // source port matches the source port `connect()` got back — if
    // someone else raced in their source port differs. Pre-#291 the
    // listener returned the first connect() to land, no matter
    // whose.
    if (peerAddr.sin_port != clientAddr.sin_port
        || peerAddr.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
        ::close(server);
        ::close(client);
        return -1;
    }
    // Pump side: non-blocking so the pump's poll-and-read loop doesn't
    // stall on partial drains.
    int flags = ::fcntl(server, F_GETFL, 0);
    if (flags >= 0) ::fcntl(server, F_SETFL, flags | O_NONBLOCK);
    fds[0] = server;
    fds[1] = client;
    return 0;
#endif
}

void closeFd(qintptr fd)
{
#ifdef Q_OS_WIN
    if (fd >= 0) ::closesocket(static_cast<SOCKET>(fd));
#else
    if (fd >= 0) ::close(static_cast<int>(fd));
#endif
}

bool fdReadable(qintptr fd, int timeoutMs)
{
#ifdef Q_OS_WIN
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(static_cast<SOCKET>(fd), &rfds);
    timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    return ::select(0, &rfds, nullptr, nullptr, &tv) > 0;
#else
    pollfd pfd{};
    pfd.fd = static_cast<int>(fd);
    pfd.events = POLLIN;
    return ::poll(&pfd, 1, timeoutMs) > 0;
#endif
}

qint64 fdRead(qintptr fd, char *buf, qint64 len)
{
#ifdef Q_OS_WIN
    return ::recv(static_cast<SOCKET>(fd), buf, static_cast<int>(len), 0);
#else
    return ::read(static_cast<int>(fd), buf, static_cast<size_t>(len));
#endif
}

qint64 fdWrite(qintptr fd, const char *buf, qint64 len)
{
#ifdef Q_OS_WIN
    return ::send(static_cast<SOCKET>(fd), buf, static_cast<int>(len), 0);
#else
    return ::write(static_cast<int>(fd), buf, static_cast<size_t>(len));
#endif
}

} // namespace

/// Pump worker: owns the ssh_channel and the pump-side fd of the
/// socketpair. Spins on its own QThread, takes the session mutex
/// briefly for each libssh call so multiple tunnels on the same
/// session can interleave.
///
/// The pump receives an already-open `ssh_channel` from the SshTunnel
/// — it doesn't drive the channel-open itself. The new async open
/// flow keeps the channel handshake on the SSH worker thread; the
/// pump thread only runs once we have something to pump.
class SshTunnelPump : public QThread
{
    Q_OBJECT
public:
    SshTunnelPump(SshSession *session, ssh_channel channel, qintptr pumpFd)
        : m_session(session),
          m_channel(channel),
          m_pumpFd(pumpFd) {}

    ~SshTunnelPump() override
    {
        stop();
        wait();
    }

    void stop()
    {
        m_stop.store(true, std::memory_order_release);
    }

    QString error() const { return m_error; }

protected:
    void run() override
    {
        SshSessionWorker *w = m_session->worker();
        constexpr qint64 kBufSize = 32 * 1024;
        QByteArray buf;
        buf.resize(kBufSize);

        while (!m_stop.load(std::memory_order_acquire)) {
            bool didWork = false;

            // 1. App-side → SSH: only if the local pump fd has data.
            if (fdReadable(m_pumpFd, 0)) {
                const qint64 n = fdRead(m_pumpFd, buf.data(), kBufSize);
                if (n > 0) {
                    qint64 written = 0;
                    bool sshErr = false;
                    QMetaObject::invokeMethod(
                        w, [this, &buf, n, &written, &sshErr]() {
                            const int rc = ssh_channel_write(
                                m_channel, buf.constData(), static_cast<int>(n));
                            if (rc < 0) sshErr = true;
                            else written = rc;
                        }, Qt::BlockingQueuedConnection);
                    if (sshErr) { m_error = QStringLiteral("ssh_channel_write failed"); break; }
                    didWork = true;
                    if (written < n) {
                        // libssh occasionally returns short writes; loop
                        // remaining bytes next iteration. We push the
                        // unwritten tail back to the socket buffer using
                        // a small loop instead.
                        qint64 sent = written;
                        while (sent < n && !m_stop.load(std::memory_order_acquire)) {
                            qint64 chunk = 0;
                            QMetaObject::invokeMethod(
                                w, [this, &buf, n, sent, &chunk, &sshErr]() {
                                    const int rc = ssh_channel_write(
                                        m_channel, buf.constData() + sent,
                                        static_cast<int>(n - sent));
                                    if (rc < 0) sshErr = true;
                                    else chunk = rc;
                                }, Qt::BlockingQueuedConnection);
                            if (sshErr) break;
                            sent += chunk;
                        }
                        if (sshErr) { m_error = QStringLiteral("ssh_channel_write failed"); break; }
                    }
                } else if (n == 0) {
                    // App side closed the socket.
                    break;
                }
            }

            // 2. SSH → app side: check for incoming bytes without
            // blocking the session mutex.
            int polled = 0;
            QMetaObject::invokeMethod(
                w, [this, &polled]() {
                    polled = ssh_channel_poll(m_channel, 0);
                }, Qt::BlockingQueuedConnection);
            // SSH_EOF (-127) is libssh's signal that the remote side
            // half-closed the channel — a normal end of stream, not
            // an error. Anything else negative is a real failure.
            if (polled == SSH_EOF) break;
            if (polled < 0) {
                m_error = QStringLiteral("ssh_channel_poll failed (rc=%1)").arg(polled);
                break;
            }
            if (polled > 0) {
                int got = 0;
                bool sshErr = false;
                QMetaObject::invokeMethod(
                    w, [this, &buf, &got, &sshErr]() {
                        const int rc = ssh_channel_read_nonblocking(
                            m_channel, buf.data(),
                            static_cast<uint32_t>(kBufSize), 0);
                        if (rc < 0) sshErr = true;
                        else got = rc;
                    }, Qt::BlockingQueuedConnection);
                if (sshErr) { m_error = QStringLiteral("ssh_channel_read failed"); break; }
                if (got > 0) {
                    qint64 totalOut = 0;
                    while (totalOut < got && !m_stop.load(std::memory_order_acquire)) {
                        const qint64 wrote = fdWrite(m_pumpFd, buf.constData() + totalOut,
                                                     got - totalOut);
                        if (wrote <= 0) {
                            // Local fd closed by the consumer.
                            m_stop.store(true, std::memory_order_release);
                            break;
                        }
                        totalOut += wrote;
                    }
                    didWork = true;
                }
            }

            // 3. Detect EOF / close on the SSH side.
            int eof = 0;
            QMetaObject::invokeMethod(
                w, [this, &eof]() { eof = ssh_channel_is_eof(m_channel); },
                Qt::BlockingQueuedConnection);
            if (eof != 0) break;

            if (!didWork) {
                // Avoid spinning: sleep when both directions are
                // idle. 50ms is plenty for interactive use (matches
                // the SSH session worker's poll cadence in
                // waitForLibssh) and cuts the per-tunnel idle wakeup
                // rate from ~200 Hz to ~20 Hz. See #291. A proper
                // QWaitCondition / select() driven wake — wakes
                // immediately on either fd readable — is a planned
                // follow-up; this is the low-risk piece.
                QThread::usleep(50000);
            }
        }

        // Pump exiting: close the SSH channel.
        if (m_channel != nullptr) {
            QMetaObject::invokeMethod(
                w, [this]() {
                    ssh_channel_close(m_channel);
                    ssh_channel_free(m_channel);
                }, Qt::BlockingQueuedConnection);
            m_channel = nullptr;
        }
        closeFd(m_pumpFd);
        m_pumpFd = -1;
    }

private:
    SshSession *m_session = nullptr;
    ssh_channel m_channel = nullptr;
    qintptr m_pumpFd = -1;
    std::atomic<bool> m_stop{false};
    QString m_error;
};

namespace {

/// Shared state for an in-flight async channel open. Lives at least as
/// long as both the worker-side lambda and the result-arrival lambda,
/// even if the `SshTunnel` is destroyed between them. The `abandoned`
/// flag tells the worker callback to free the channel itself instead
/// of trying to hand it back to a destroyed tunnel.
struct OpenState
{
    QPointer<SshSessionWorker> worker;
    std::atomic<bool> abandoned{false};
};

} // namespace

struct SshTunnel::Private
{
    SshSession *session = nullptr;
    QString remoteHost;
    quint16 remotePort = 0;
    std::unique_ptr<SshTunnelPump> pump;
    bool opened = false;
    QString lastError;
    std::shared_ptr<OpenState> openState;
    bool destroyConnected = false;
};

SshTunnel::SshTunnel(SshSession *session, QString remoteHost, quint16 remotePort,
                     QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->session = session;
    d->remoteHost = std::move(remoteHost);
    d->remotePort = remotePort;
}

SshTunnel::~SshTunnel()
{
    // If an open is still in flight, tell the worker-side lambda to
    // free the channel itself instead of posting back to us.
    if (d->openState) d->openState->abandoned.store(true);
    close();
}

void SshTunnel::open()
{
    if (d->session == nullptr || d->session->state() != SshSession::Connected) {
        d->lastError = QStringLiteral("SSH session not connected");
        emit failed(d->lastError);
        return;
    }
    SshSessionWorker *w = d->session->worker();
    if (w == nullptr) {
        d->lastError = QStringLiteral("SSH session worker not available");
        emit failed(d->lastError);
        return;
    }

    // Wire aboutToDestroy → close() once; the connection survives across
    // the async open boundary. Pump exit emits closed() the same way.
    if (!d->destroyConnected) {
        QObject::connect(d->session, &SshSession::aboutToDestroy,
                         this, &SshTunnel::close, Qt::DirectConnection);
        d->destroyConnected = true;
    }

    auto state = std::make_shared<OpenState>();
    state->worker = w;
    d->openState = state;

    QPointer<SshTunnel> self(this);
    const QString host = d->remoteHost;
    const quint16 port = d->remotePort;

    QMetaObject::invokeMethod(w, [state, host, port, self]() {
        SshSessionWorker *worker = state->worker.data();
        if (worker == nullptr) return;
        ssh_channel ch = worker->openForwardChannel(host, port);

        // If the tunnel was destroyed (or close()d) while we were in
        // the libssh handshake, free the channel right here on the
        // worker — never bounce it back.
        if (state->abandoned.load(std::memory_order_acquire)) {
            if (ch != nullptr) {
                ssh_channel_close(ch);
                ssh_channel_free(ch);
            }
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, state, ch]() {
            // Same race on the result-arrival side: if we've been
            // destroyed between the worker posting and us processing,
            // post a cleanup back to the worker. (Channel handles must
            // be freed on the same thread that owns the ssh_session.)
            if (self.isNull() || state->abandoned.load(std::memory_order_acquire)) {
                if (ch != nullptr) {
                    SshSessionWorker *w = state->worker.data();
                    if (w != nullptr) {
                        QMetaObject::invokeMethod(w, [ch]() {
                            ssh_channel_close(ch);
                            ssh_channel_free(ch);
                        }, Qt::QueuedConnection);
                    }
                }
                return;
            }
            self->onChannelOpened(ch);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void SshTunnel::onChannelOpened(void *rawChannel)
{
    auto *ch = static_cast<ssh_channel>(rawChannel);
    if (ch == nullptr) {
        d->lastError = QStringLiteral("Failed to open SSH channel to %1:%2")
                           .arg(d->remoteHost).arg(d->remotePort);
        d->openState.reset();
        emit failed(d->lastError);
        return;
    }

    qintptr fds[2] = { -1, -1 };
    if (makeSocketPair(fds) != 0) {
        // Channel is open but we couldn't build the socketpair; tear
        // down the channel via the worker before failing.
        SshSessionWorker *w = d->openState ? d->openState->worker.data() : nullptr;
        if (w != nullptr) {
            QMetaObject::invokeMethod(w, [ch]() {
                ssh_channel_close(ch);
                ssh_channel_free(ch);
            }, Qt::QueuedConnection);
        }
        d->lastError = QStringLiteral("socketpair() failed");
        d->openState.reset();
        emit failed(d->lastError);
        return;
    }

    d->pump = std::make_unique<SshTunnelPump>(d->session, ch, fds[0]);
    QObject::connect(d->pump.get(), &QThread::finished,
                     this, &SshTunnel::close, Qt::QueuedConnection);
    d->pump->start();
    d->opened = true;
    d->openState.reset();

    // Ownership of fds[1] transfers to the receiver of opened().
    emit opened(fds[1]);
}

void SshTunnel::close()
{
    // Abandon any in-flight open. Already-abandoned is a no-op.
    if (d->openState) d->openState->abandoned.store(true);

    if (d->pump != nullptr) {
        d->pump->stop();
        d->pump->wait();
        d->pump.reset();
    }
    if (d->opened) {
        d->opened = false;
        emit closed();
    }
}

bool SshTunnel::isOpen() const { return d->opened; }
QString SshTunnel::lastError() const { return d->lastError; }

} // namespace qumesh::ssh

#include "ssh_tunnel.moc"
