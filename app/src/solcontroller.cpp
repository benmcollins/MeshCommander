// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "solcontroller.h"

#include "redir/redir_client.h"
#include "redir/redir_codec.h"
#include "redir/sol_session.h"
#include "ssh/ssh_session.h"
#include "ssh_tunnel_opener.h"
#include "sshtunnelhost.h"

namespace qumesh::app {

using qumesh::redir::RedirectionClient;
using qumesh::redir::SolSession;
using qumesh::terminal::TerminalScreen;

SolController::SolController(QObject *parent)
    : QObject(parent), m_screen(new TerminalScreen(this))
{
    // The parser emits this whenever the host queries our terminal
    // size with an XTWINOPS escape (CSI 18/19/14 t). Forward the
    // reply down the SOL channel so `resize(1)` and friends can see
    // it. No-op when no session is open.
    connect(m_screen, &TerminalScreen::respond, this, [this](const QByteArray &bytes) {
        if (m_session) m_session->sendInput(bytes);
    });
}

SolController::~SolController() { teardown(); }

void SolController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
}

void SolController::setUser(const QString &v)
{
    if (v == m_user) return;
    m_user = v;
    emit userChanged();
}

void SolController::setPassword(const QString &v)
{
    if (v == m_password) return;
    m_password = v;
    emit passwordChanged();
}

void SolController::setTls(bool v)
{
    if (v == m_tls) return;
    m_tls = v;
    emit tlsChanged();
}

void SolController::setTrustedFingerprints(QStringList v)
{
    if (v == m_trustedFingerprints) return;
    m_trustedFingerprints = std::move(v);
    emit trustedFingerprintsChanged();
}

void SolController::setState(State s)
{
    if (s == m_state) return;
    const bool wasAwaiting = (m_state == State::AwaitingTrust);
    m_state = s;
    emit stateChanged();
    if (wasAwaiting != (s == State::AwaitingTrust)) emit awaitingTrustChanged();
}

void SolController::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void SolController::setSshConfig(const QVariantMap &cfg)
{
    if (m_sshHost == nullptr) {
        m_sshHost = new SshTunnelHost(this);
        QObject::connect(m_sshHost, &SshTunnelHost::trustedHostKeyAdded, this,
                         &SolController::trustedSshHostKeyAdded);
        QObject::connect(m_sshHost, &SshTunnelHost::connectedChanged, this, [this]() {
            if (m_openDeferred && m_sshHost != nullptr && m_sshHost->isConnected()) {
                m_openDeferred = false;
                open();
            }
        });
    }
    m_sshHost->setConfig(cfg);
    m_sshSession = m_sshHost->session();
}

void SolController::open()
{
    // SSH tunnel mode: if the host is still negotiating, defer
    // the dial until it reaches Connected. The host's
    // connectedChanged slot will re-call open().
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
    m_screen->clear();

    m_client = new RedirectionClient(this);
    m_client->setProtocol(qumesh::redir::Protocol::Sol);
    m_client->setCredentials(m_user, m_password);
    m_client->setTls(m_tls);
    m_client->setTrustedFingerprints(m_trustedFingerprints);
    if (m_sshSession != nullptr) {
        m_client->setTunnelOpener(qumesh::app::makeSshTunnelOpener(m_sshSession));
    }

    m_session = new SolSession(m_client, this);

    connect(m_client.data(), &RedirectionClient::peerCertVerifiedByPin, this,
            &SolController::peerCertVerifiedByPin);
    connect(m_client.data(), &RedirectionClient::trustPromptRequired, this,
            [this](const qumesh::redir::PeerCertSummary &summary) {
                m_pendingCert = summary;
                emit pendingCertChanged();
                setState(State::AwaitingTrust);
            });

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
            m_session.data(), &SolSession::start);

    connect(m_client.data(), &RedirectionClient::failed, this, [this](const QString &reason) {
        setLastError(reason);
        setState(State::Failed);
    });

    connect(m_session.data(), &SolSession::sessionOpened, this, [this]() {
        setState(State::Connected);
    });
    connect(m_session.data(), &SolSession::data, this, [this](const QByteArray &bytes) {
        m_screen->feed(bytes);
    });
    connect(m_session.data(), &SolSession::closed, this, [this](const QString &reason) {
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

void SolController::close()
{
    teardown();
    setState(State::Disconnected);
}

void SolController::trustPendingCert(bool persist)
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

void SolController::sendText(const QString &text)
{
    sendBytes(text.toUtf8());
}

void SolController::sendBytes(const QByteArray &bytes)
{
    if (!m_session) return;
    m_session->sendInput(bytes);
}

void SolController::teardown()
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
