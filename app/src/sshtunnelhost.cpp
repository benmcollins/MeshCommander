// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "sshtunnelhost.h"

#include "ssh/ssh_session.h"

namespace qumesh::app {

SshTunnelHost::SshTunnelHost(QObject *parent) : QObject(parent) {}
SshTunnelHost::~SshTunnelHost() = default;

bool SshTunnelHost::isConnected() const
{
    return m_session != nullptr
        && m_session->state() == qumesh::ssh::SshSession::Connected;
}

void SshTunnelHost::setConfig(const QVariantMap &cfg)
{
    const bool enabled = cfg.value(QStringLiteral("enabled")).toBool();
    if (!enabled) {
        m_enabled = false;
        if (m_session != nullptr) {
            m_session->close();
            m_session->deleteLater();
            m_session = nullptr;
        }
        if (!m_status.isEmpty()) {
            m_status.clear();
            emit statusChanged();
        }
        emit connectedChanged();
        return;
    }

    m_enabled = true;
    m_jumpHost = cfg.value(QStringLiteral("host")).toString();
    if (m_session == nullptr) {
        m_session = new qumesh::ssh::SshSession(this);
        QObject::connect(m_session, &qumesh::ssh::SshSession::stateChanged, this,
                [this](qumesh::ssh::SshSession::State s) {
            // Status strings carry the jump host name when known so the
            // UI can show "via jump.example.com" rather than a bare
            // "via SSH" badge. Issue #245.
            const QString h = m_jumpHost.isEmpty()
                ? QStringLiteral("SSH")
                : m_jumpHost;
            switch (s) {
            case qumesh::ssh::SshSession::Connecting:
                m_status = QStringLiteral("Connecting to %1…").arg(h); break;
            case qumesh::ssh::SshSession::Authenticating:
                m_status = QStringLiteral("Authenticating to %1…").arg(h); break;
            case qumesh::ssh::SshSession::NeedsHostKeyTrust:
                m_status = QStringLiteral("Awaiting host-key trust"); break;
            case qumesh::ssh::SshSession::Connected:
                m_status = QStringLiteral("via %1").arg(h); break;
            case qumesh::ssh::SshSession::Failed:
                m_status = QStringLiteral("SSH (%1) failed: %2")
                               .arg(h, m_session->lastError());
                break;
            case qumesh::ssh::SshSession::Disconnected:
                m_status.clear(); break;
            }
            emit statusChanged();
            emit connectedChanged();
        });
        QObject::connect(m_session, &qumesh::ssh::SshSession::hostKeyPromptRequired, this,
                [this](const QString &fp, const QString & /*keyType*/) {
            // TOFU: auto-trust on first connect, then emit so the
            // controller can persist the fingerprint into the
            // ComputerModel for verification on subsequent connects.
            m_session->trustPendingHostKey();
            emit trustedHostKeyAdded(fp);
        });
    }

    qumesh::ssh::SshSession::Params p;
    p.host = cfg.value(QStringLiteral("host")).toString();
    p.port = static_cast<quint16>(cfg.value(QStringLiteral("port"), 22).toInt());
    p.user = cfg.value(QStringLiteral("user")).toString();
    p.authMode = cfg.value(QStringLiteral("authMode")).toInt()
                     == int(qumesh::ssh::SshSession::AuthKey)
                     ? qumesh::ssh::SshSession::AuthKey
                     : qumesh::ssh::SshSession::AuthPassword;
    p.password = cfg.value(QStringLiteral("password")).toString();
    p.privateKeyPath = cfg.value(QStringLiteral("keyPath")).toString();
    p.privateKeyPassphrase = cfg.value(QStringLiteral("keyPassphrase")).toString();
    p.compression = cfg.value(QStringLiteral("compression")).toBool();
    const QVariantList fps = cfg.value(QStringLiteral("trustedHostKeyFingerprints"))
                                  .toList();
    for (const QVariant &v : fps) p.trustedHostKeyFingerprints.append(v.toString());

    m_session->open(p);
}

} // namespace qumesh::app
