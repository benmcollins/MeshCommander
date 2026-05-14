// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh/ssh_tunnel.h"
#include "ssh/ssh_session.h"
#include "ssh_session_worker.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>

#include <libssh/libssh.h>

#include <atomic>

Q_LOGGING_CATEGORY(qumeshSshTunnel, "qumesh.ssh.tunnel", QtWarningMsg)

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

// Forward-declared in ssh_session.cpp; we only need its raw session
// pointer and mutex here, so reach into it via the public worker()
// handle.
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
    int server = ::accept(listener, nullptr, nullptr);
    ::close(listener);
    if (server < 0) {
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
class SshTunnelPump : public QThread
{
    Q_OBJECT
public:
    SshTunnelPump(SshSession *session, QString remoteHost, quint16 remotePort)
        : m_session(session),
          m_remoteHost(std::move(remoteHost)),
          m_remotePort(remotePort) {}

    ~SshTunnelPump() override
    {
        stop();
        wait();
    }

    /// Synchronously open the channel + socketpair. Returns the Qt-side
    /// fd on success, -1 on failure (and sets `m_error`).
    qintptr openSync()
    {
        // Ask the session's worker to open a channel for us. The worker
        // serializes access to the ssh_session.
        SshSessionWorker *w = m_session->worker();
        if (w == nullptr) {
            m_error = QStringLiteral("SSH session is not connected");
            return -1;
        }
        ssh_channel ch = nullptr;
        bool ok = QMetaObject::invokeMethod(
            w, [w, host = m_remoteHost, port = m_remotePort, &ch]() {
                ch = const_cast<SshSessionWorker *>(w)->openForwardChannel(host, port);
            }, Qt::BlockingQueuedConnection);
        if (!ok || ch == nullptr) {
            m_error = QStringLiteral("Failed to open SSH channel to %1:%2")
                          .arg(m_remoteHost).arg(m_remotePort);
            return -1;
        }
        m_channel = ch;

        qintptr fds[2] = {-1, -1};
        if (makeSocketPair(fds) != 0) {
            // Tear down the channel; we held the session mutex implicitly
            // via the worker call above.
            QMetaObject::invokeMethod(w, [ch]() {
                ssh_channel_close(ch);
                ssh_channel_free(ch);
            }, Qt::BlockingQueuedConnection);
            m_channel = nullptr;
            m_error = QStringLiteral("socketpair() failed");
            return -1;
        }
        m_pumpFd = fds[0];
        qCWarning(qumeshSshTunnel) << "tunnel to" << m_remoteHost << ":"
                                 << m_remotePort << "opened: pump fd"
                                 << m_pumpFd << ", qt fd" << fds[1];
        return fds[1];
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
        qCWarning(qumeshSshTunnel) << "pump thread started for"
                                 << m_remoteHost << ":" << m_remotePort;
        qint64 totalAppToSsh = 0;
        qint64 totalSshToApp = 0;

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
                    totalAppToSsh += written;
                    qCWarning(qumeshSshTunnel) << "app→ssh wrote" << written
                                             << "bytes (total" << totalAppToSsh << ")";
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
            if (polled < 0) {
                m_error = QStringLiteral("ssh_channel_poll failed");
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
                    totalSshToApp += got;
                    qCWarning(qumeshSshTunnel) << "ssh→app wrote" << got
                                             << "bytes (total" << totalSshToApp << ")";
                }
            }

            // 3. Detect EOF / close on the SSH side.
            int eof = 0;
            QMetaObject::invokeMethod(
                w, [this, &eof]() { eof = ssh_channel_is_eof(m_channel); },
                Qt::BlockingQueuedConnection);
            if (eof != 0) {
                qCWarning(qumeshSshTunnel) << "ssh channel EOF — pump exiting";
                break;
            }

            if (!didWork) {
                // Avoid spinning: short sleep when both directions
                // are idle. 5ms keeps interactive paste/keystrokes
                // responsive while bounding CPU at ~0% idle.
                QThread::usleep(5000);
            }
        }
        qCWarning(qumeshSshTunnel) << "pump thread exiting; sent" << totalAppToSsh
                                 << "bytes app→ssh, received" << totalSshToApp
                                 << "bytes ssh→app; m_error=" << m_error;

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
    QString m_remoteHost;
    quint16 m_remotePort = 0;
    ssh_channel m_channel = nullptr;
    qintptr m_pumpFd = -1;
    std::atomic<bool> m_stop{false};
    QString m_error;
};

struct SshTunnel::Private
{
    SshSession *session = nullptr;
    QString remoteHost;
    quint16 remotePort = 0;
    std::unique_ptr<SshTunnelPump> pump;
    qintptr localFd = -1;
    bool opened = false;
    QString lastError;
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
    close();
}

void SshTunnel::open()
{
    if (d->session == nullptr || d->session->state() != SshSession::Connected) {
        d->lastError = QStringLiteral("SSH session not connected");
        emit failed(d->lastError);
        return;
    }
    d->pump = std::make_unique<SshTunnelPump>(d->session, d->remoteHost, d->remotePort);
    const qintptr fd = d->pump->openSync();
    if (fd < 0) {
        d->lastError = d->pump->error();
        d->pump.reset();
        emit failed(d->lastError);
        return;
    }
    d->localFd = fd;
    d->opened = true;
    d->pump->start();
    emit opened();
}

void SshTunnel::close()
{
    if (d->pump != nullptr) {
        d->pump->stop();
        d->pump->wait();
        d->pump.reset();
    }
    if (d->localFd >= 0) {
        // If the caller hasn't taken the fd yet, close it ourselves.
        closeFd(d->localFd);
        d->localFd = -1;
    }
    if (d->opened) {
        d->opened = false;
        emit closed();
    }
}

qintptr SshTunnel::takeLocalFd()
{
    const qintptr fd = d->localFd;
    d->localFd = -1;
    return fd;
}

bool SshTunnel::isOpen() const { return d->opened; }
QString SshTunnel::lastError() const { return d->lastError; }

} // namespace qumesh::ssh

#include "ssh_tunnel.moc"
