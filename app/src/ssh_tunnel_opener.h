// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "redir/redir_client.h"
#include "wsman/wsman_client.h"

namespace qumesh::ssh { class SshSession; }

namespace qumesh::app {

/// Build a `RedirectionClient::TunnelOpener` that opens a fresh
/// `SshTunnel` over the given session for every connect attempt. The
/// returned callback captures `session` by raw pointer; the caller
/// must keep `session` alive while the underlying RedirectionClient
/// may reconnect.
qumesh::redir::RedirectionClient::TunnelOpener
makeSshTunnelOpener(qumesh::ssh::SshSession *session);

/// Same idea but for `WsmanClient::SocketFactory` (one channel per
/// request, short-lived). `amtHost` / `amtPort` capture the WSMAN
/// endpoint at controller setup time so the lambda doesn't need to
/// look up the URL again.
qumesh::wsman::WsmanClient::SocketFactory
makeSshSocketFactory(qumesh::ssh::SshSession *session, QString amtHost, quint16 amtPort);

} // namespace qumesh::app
