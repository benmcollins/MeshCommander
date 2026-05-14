// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace qumesh::ssh {

class SshSession;

/// A single direct-tcpip channel over an authenticated `SshSession`,
/// paired with an `AF_UNIX` `socketpair()` so the Qt-side gets a real
/// socket file descriptor. The descriptor is suitable for
/// `QSslSocket::setSocketDescriptor` / `QTcpSocket::setSocketDescriptor`
/// — no port is ever bound on loopback, so nothing else on the local
/// machine can connect into the tunnel.
///
/// Lifecycle:
///   1. Construct with the session + (remote host, remote port).
///   2. Call `open()`. The tunnel emits `opened()` when the SSH channel
///      is up and the local fd is ready, or `failed()` on error.
///   3. Read `takeLocalFd()` once and pass it to the Qt client.
///   4. The pump runs on a dedicated thread until either side closes
///      or `close()` is called.
///
/// The session must reach `Connected` before `open()` is called; on a
/// session that has not authenticated this returns an error.
class SshTunnel : public QObject
{
    Q_OBJECT
public:
    /// Build a tunnel that forwards to `remoteHost:remotePort` over
    /// `session`. The tunnel does not retain ownership of the session
    /// pointer; the caller must keep the session alive for the tunnel's
    /// lifetime.
    SshTunnel(SshSession *session, QString remoteHost, quint16 remotePort,
              QObject *parent = nullptr);
    ~SshTunnel() override;

    /// Open the underlying SSH channel + socketpair pump.
    void open();

    /// Close the tunnel and stop the pump. Safe to call from any state.
    void close();

    /// The Qt-side socket descriptor. -1 until `opened()` has fired.
    /// First call transfers ownership of the fd to the caller; subsequent
    /// calls return -1.
    [[nodiscard]] qintptr takeLocalFd();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString lastError() const;

signals:
    void opened();
    void failed(const QString &error);
    void closed();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace qumesh::ssh
