// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh_tunnel_opener.h"

#include "ssh/ssh_session.h"
#include "ssh/ssh_tunnel.h"

#include <QPointer>

namespace qumesh::app {

namespace {

/// Build a self-cleaning `SshTunnel` and wire its async signals to
/// `done`. Used by both factories — the only difference is whether
/// the host/port come from the caller (TunnelOpener) or are captured
/// at setup (SocketFactory).
void openAsync(qumesh::ssh::SshSession *session,
               const QString &host, quint16 port,
               std::function<void(qintptr, QString)> done)
{
    if (session == nullptr
        || session->state() != qumesh::ssh::SshSession::Connected) {
        done(-1, QStringLiteral("SSH session not connected"));
        return;
    }

    auto *tunnel = new qumesh::ssh::SshTunnel(session, host, port);

    // `opened` and `failed` are mutually exclusive by contract — see
    // SshTunnel: the async open path emits exactly one of them.
    QObject::connect(tunnel, &qumesh::ssh::SshTunnel::opened, tunnel,
                     [tunnel, done](qintptr fd) {
        // Tunnel manages its own teardown once the consumer closes the
        // returned fd; closed() fires when the pump exits.
        QObject::connect(tunnel, &qumesh::ssh::SshTunnel::closed,
                         tunnel, &QObject::deleteLater);
        done(fd, QString());
    });
    QObject::connect(tunnel, &qumesh::ssh::SshTunnel::failed, tunnel,
                     [tunnel, done](const QString &err) {
        tunnel->deleteLater();
        done(-1, err);
    });

    tunnel->open();
}

} // namespace

qumesh::redir::RedirectionClient::TunnelOpener
makeSshTunnelOpener(qumesh::ssh::SshSession *session)
{
    QPointer<qumesh::ssh::SshSession> weak(session);
    return [weak](const QString &host, quint16 port,
                  qumesh::redir::RedirectionClient::TunnelCallback done) {
        openAsync(weak.data(), host, port, std::move(done));
    };
}

qumesh::wsman::WsmanClient::SocketFactory
makeSshSocketFactory(qumesh::ssh::SshSession *session, QString amtHost, quint16 amtPort)
{
    QPointer<qumesh::ssh::SshSession> weak(session);
    return [weak, amtHost = std::move(amtHost), amtPort](
               qumesh::wsman::WsmanClient::SocketCallback done) {
        openAsync(weak.data(), amtHost, amtPort, std::move(done));
    };
}

} // namespace qumesh::app
