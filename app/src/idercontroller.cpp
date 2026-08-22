// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "idercontroller.h"

#include <QFileInfo>
#include <QLoggingCategory>

#include "ider/ider_session.h"
#include "redir/redir_client.h"
#include "redir/redir_codec.h"
#include "ssh/ssh_session.h"
#include "ssh_tunnel_opener.h"
#include "sshtunnelhost.h"

// QtInfoMsg, not the two-arg default: see redir/src/redir_client.cpp.
Q_LOGGING_CATEGORY(lcIderController, "qumesh.app.ider", QtInfoMsg)

namespace qumesh::app {

using qumesh::ider::IderSession;
using qumesh::redir::RedirectionClient;

IderController::IderController(QObject *parent) : QObject(parent) {}
IderController::~IderController() { teardown(); }

void IderController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
}

void IderController::setUser(const QString &v)
{
    if (v == m_user) return;
    m_user = v;
    emit userChanged();
}

void IderController::setPassword(const QString &v)
{
    if (v == m_password) return;
    m_password = v;
    emit passwordChanged();
}

void IderController::setIsoPath(const QString &v)
{
    if (v == m_isoPath) return;
    m_isoPath = v;
    emit isoPathChanged();
}

void IderController::setStartOption(StartOption v)
{
    if (v == m_startOption) return;
    m_startOption = v;
    emit startOptionChanged();
}

void IderController::setTls(bool v)
{
    if (v == m_tls) return;
    m_tls = v;
    emit tlsChanged();
}

void IderController::setTrustedFingerprints(QStringList v)
{
    if (v == m_trustedFingerprints) return;
    m_trustedFingerprints = std::move(v);
    emit trustedFingerprintsChanged();
}

void IderController::setState(State s)
{
    if (s == m_state) return;
    const bool wasAwaiting = (m_state == State::AwaitingTrust);
    m_state = s;
    emit stateChanged();
    if (wasAwaiting != (s == State::AwaitingTrust)) emit awaitingTrustChanged();
}

void IderController::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void IderController::setSshConfig(const QVariantMap &cfg)
{
    if (m_sshHost == nullptr) {
        m_sshHost = new SshTunnelHost(this);
        QObject::connect(m_sshHost, &SshTunnelHost::trustedHostKeyAdded, this,
                         &IderController::trustedSshHostKeyAdded);
        QObject::connect(m_sshHost, &SshTunnelHost::hostKeyPromptRequired, this,
                [this](const QString &fp, const QString &keyType) {
            // Mirror the prompt up so QML's SshHostKeyTrustDialog can
            // bind to `awaitingSshHostKeyTrust` on us; we proxy the
            // user's accept back down to the host via
            // `trustPendingSshHostKey`. See #270.
            m_pendingSshHostKey = fp;
            m_pendingSshHostKeyType = keyType;
            m_awaitingSshHostKeyTrust = true;
            emit sshHostKeyPromptChanged();
            emit sshHostKeyPromptRequired(fp, keyType);
        });
        QObject::connect(m_sshHost, &SshTunnelHost::connectedChanged, this, [this]() {
            if (m_openDeferred && m_sshHost != nullptr && m_sshHost->isConnected()) {
                m_openDeferred = false;
                open();
            }
        });
    }
    if (m_awaitingSshHostKeyTrust
        && cfg.value(QStringLiteral("enabled")).toBool() == false) {
        m_pendingSshHostKey.clear();
        m_pendingSshHostKeyType.clear();
        m_awaitingSshHostKeyTrust = false;
        emit sshHostKeyPromptChanged();
    }
    m_sshHost->setConfig(cfg);
    m_sshSession = m_sshHost->session();
}

void IderController::trustPendingSshHostKey(bool persist)
{
    if (!m_awaitingSshHostKeyTrust || m_sshHost == nullptr) return;
    m_pendingSshHostKey.clear();
    m_pendingSshHostKeyType.clear();
    m_awaitingSshHostKeyTrust = false;
    emit sshHostKeyPromptChanged();
    m_sshHost->trustPendingHostKey(persist);
}

void IderController::open()
{
    if (m_sshHost != nullptr && m_sshHost->isEnabled() && !m_sshHost->isConnected()) {
        m_openDeferred = true;
        setLastError({});
        setState(State::Connecting);
        return;
    }
    m_openDeferred = false;
    teardown();
    if (m_host.isEmpty()) {
        setLastError(tr("host is empty"));
        setState(State::Failed);
        return;
    }
    if (m_isoPath.isEmpty()) {
        setLastError(tr("ISO path is required"));
        setState(State::Failed);
        return;
    }

    m_bytesSentToAmt = 0;
    m_bytesReceivedFromAmt = 0;
    emit statsChanged();
    m_deviceEnabled = false;
    emit deviceEnabledChanged();

    m_client = new RedirectionClient(this);
    m_client->setProtocol(qumesh::redir::Protocol::Ider);
    m_client->setCredentials(m_user, m_password);
    m_client->setTls(m_tls);
    m_client->setTrustedFingerprints(m_trustedFingerprints);
    if (m_sshSession != nullptr) {
        m_client->setTunnelOpener(qumesh::app::makeSshTunnelOpener(m_sshSession));
    }

    m_session = new IderSession(m_client, this);
    m_session->setIsoPath(m_isoPath);

    connect(m_client.data(), &RedirectionClient::peerCertVerifiedByPin, this,
            &IderController::peerCertVerifiedByPin);
    connect(m_client.data(), &RedirectionClient::trustPromptRequired, this,
            [this](const qumesh::redir::PeerCertSummary &summary) {
                m_pendingCert = summary;
                emit pendingCertChanged();
                setState(State::AwaitingTrust);
            });

    using IderStart = IderSession::StartOption;
    switch (m_startOption) {
    case StartOption::OnReboot:  m_session->setStartOption(IderStart::OnReboot); break;
    case StartOption::Graceful:  m_session->setStartOption(IderStart::Graceful); break;
    case StartOption::Immediate: m_session->setStartOption(IderStart::Immediate); break;
    }

    connect(m_client.data(), &RedirectionClient::stateChanged, this, [this]() {
        if (!m_client) return;
        switch (m_client->state()) {
        case RedirectionClient::State::Disconnected:
            setState(State::Disconnected);
            break;
        case RedirectionClient::State::Connecting:
            setState(State::Connecting);
            break;
        case RedirectionClient::State::SelectorSent:
        case RedirectionClient::State::SessionOpened:
        case RedirectionClient::State::AuthQuerying:
        case RedirectionClient::State::AuthChallenging:
        case RedirectionClient::State::AuthResponding:
            setState(State::Authenticating);
            break;
        case RedirectionClient::State::Authenticated:
            setState(State::Opening);
            break;
        case RedirectionClient::State::Failed:
            setState(State::Failed);
            break;
        }
    });

    connect(m_client.data(), &RedirectionClient::authenticated,
            m_session.data(), &IderSession::start);

    connect(m_client.data(), &RedirectionClient::failed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    connect(m_session.data(), &IderSession::sessionOpened, this,
            [this](const qumesh::ider::SessionInfo &info) {
                // The firmware/buffer numbers are exactly the
                // compatibility data an IDE-R support round-trip asks
                // for, and none of it is sensitive.
                qCInfo(lcIderController) << "IDE-R session open — protocol"
                                          << info.major << "." << info.minor
                                          << "firmware" << info.fwMajor << "." << info.fwMinor
                                          << "readBuffer" << info.readBuffer
                                          << "writeBuffer" << info.writeBuffer
                                          << "iana" << info.iana;
                setState(State::Running);
            });
    connect(m_session.data(), &IderSession::enabledChanged, this, [this](bool en) {
        if (en == m_deviceEnabled) return;
        m_deviceEnabled = en;
        emit deviceEnabledChanged();
    });
    connect(m_session.data(), &IderSession::statsChanged, this,
            [this](quint64 to, quint64 from) {
                m_bytesSentToAmt = to;
                m_bytesReceivedFromAmt = from;
                emit statsChanged();
            });
    connect(m_session.data(), &IderSession::closed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    setLastError({});
    setState(State::Connecting);
    // AMT redirection: 16994 plain / 16995 TLS — fixed by the protocol.
    // Tests can override via setPortForTest(); production always derives.
    const quint16 port = m_portOverride != 0 ? m_portOverride
                                              : (m_tls ? 16995 : 16994);
    m_client->connectTo(m_host, port);
}

void IderController::close()
{
    teardown();
    setState(State::Disconnected);
}

void IderController::trustPendingCert(bool persist)
{
    if (!m_client || m_state != State::AwaitingTrust) return;
    const QString fp = m_pendingCert.fingerprintSha256;
    m_client->trustPendingPeerCert();
    if (!m_trustedFingerprints.contains(fp)) {
        m_trustedFingerprints.append(fp);
        emit trustedFingerprintsChanged();
    }
    setState(State::Authenticating);
    if (persist && !fp.isEmpty()) emit trustedFingerprintAdded(fp);
}

void IderController::teardown()
{
    if (m_session) {
        m_session->disconnect(this);
        m_session->deleteLater();
        m_session = nullptr;
    }
    if (m_client) {
        m_client->disconnect(this);
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (!m_pendingCert.fingerprintSha256.isEmpty()) {
        m_pendingCert = {};
        emit pendingCertChanged();
    }
}

} // namespace qumesh::app
