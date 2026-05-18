// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace qumesh::ssh {

class SshSession;

/// A single direct-tcpip channel over an authenticated `SshSession`,
/// paired with a transient-loopback socketpair so the Qt-side gets a
/// real socket file descriptor. The descriptor is suitable for
/// `QSslSocket::setSocketDescriptor` / `QTcpSocket::setSocketDescriptor`.
///
/// Lifecycle:
///   1. Construct with the session + (remote host, remote port).
///   2. Call `open()`. The tunnel emits `opened(fd)` with the local
///      socket descriptor when the SSH channel is up, or `failed(err)`
///      on error. Neither signal fires before `open()` returns —
///      `open()` itself is non-blocking and never touches libssh on
///      the caller's thread.
///   3. The pump runs on a dedicated thread until either side closes
///      or `close()` is called; `closed()` fires when the pump exits.
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

    /// Asynchronously open the SSH channel. The channel handshake runs
    /// on the SSH worker thread; this function returns immediately.
    /// Completion is reported via the `opened(fd)` or `failed(error)`
    /// signal. Safe to call only once per tunnel.
    void open();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString lastError() const;

public slots:
    /// Close the tunnel and stop the pump. Safe to call from any state
    /// and to call multiple times. Used as a slot so the underlying
    /// `SshSession` and the pump's `QThread::finished` can both drive
    /// teardown via signal/slot, eliminating dangling-pointer races
    /// between the pump thread and session shutdown.
    void close();

signals:
    /// The SSH channel is up and `fd` is a connected stream socket
    /// suitable for `setSocketDescriptor`. Ownership of `fd` transfers
    /// to the receiver — the tunnel will not close it again.
    void opened(qintptr fd);

    /// The channel could not be opened. The tunnel is in a terminal
    /// state; no further signals will fire.
    void failed(const QString &error);

    /// The pump has exited and the SSH channel has been freed.
    void closed();

private:
    struct Private;
    std::unique_ptr<Private> d;

    // Called on the SshTunnel's thread once the worker hands us a
    // freshly-opened channel. Builds the socketpair, starts the pump,
    // emits opened(fd). `channel` may be null on failure.
    void onChannelOpened(void *channel);
};

} // namespace qumesh::ssh
