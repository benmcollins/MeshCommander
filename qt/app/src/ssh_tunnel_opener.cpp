// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh_tunnel_opener.h"

#include "ssh/ssh_session.h"
#include "ssh/ssh_tunnel.h"

namespace qumesh::app {

qumesh::redir::RedirectionClient::TunnelOpener
makeSshTunnelOpener(qumesh::ssh::SshSession *session)
{
    return [session](const QString &host, quint16 port) -> qintptr {
        if (session == nullptr
            || session->state() != qumesh::ssh::SshSession::Connected)
            return -1;
        auto *tunnel = new qumesh::ssh::SshTunnel(session, host, port);
        tunnel->open();
        if (!tunnel->isOpen()) {
            tunnel->deleteLater();
            return -1;
        }
        const qintptr fd = tunnel->takeLocalFd();
        QObject::connect(tunnel, &qumesh::ssh::SshTunnel::closed,
                         tunnel, &QObject::deleteLater);
        return fd;
    };
}

qumesh::wsman::WsmanClient::SocketFactory
makeSshSocketFactory(qumesh::ssh::SshSession *session, QString amtHost, quint16 amtPort)
{
    return [session, amtHost = std::move(amtHost), amtPort]() -> qintptr {
        if (session == nullptr
            || session->state() != qumesh::ssh::SshSession::Connected)
            return -1;
        auto *tunnel = new qumesh::ssh::SshTunnel(session, amtHost, amtPort);
        tunnel->open();
        if (!tunnel->isOpen()) {
            tunnel->deleteLater();
            return -1;
        }
        const qintptr fd = tunnel->takeLocalFd();
        QObject::connect(tunnel, &qumesh::ssh::SshTunnel::closed,
                         tunnel, &QObject::deleteLater);
        return fd;
    };
}

} // namespace qumesh::app
