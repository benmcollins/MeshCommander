// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvmcontroller.h"

#include "kvm/kvm_codec.h"
#include "kvm/kvm_session.h"
#include "kvmframebuffer.h"
#include "redir/redir_client.h"
#include "redir/redir_codec.h"

namespace meshcommander::app {

using meshcommander::kvm::KvmSession;
using meshcommander::redir::RedirectionClient;

KvmController::KvmController(QObject *parent)
    : QObject(parent), m_framebuffer(new KvmFramebuffer(this))
{
}

KvmController::~KvmController() { teardown(); }

void KvmController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
}

void KvmController::setPort(quint16 v)
{
    if (v == m_port) return;
    m_port = v;
    emit portChanged();
}

void KvmController::setUser(const QString &v)
{
    if (v == m_user) return;
    m_user = v;
    emit userChanged();
}

void KvmController::setPassword(const QString &v)
{
    if (v == m_password) return;
    m_password = v;
    emit passwordChanged();
}

void KvmController::setTls(bool v)
{
    if (v == m_tls) return;
    m_tls = v;
    emit tlsChanged();
}

void KvmController::setTrustedFingerprints(QStringList v)
{
    if (v == m_trustedFingerprints) return;
    m_trustedFingerprints = std::move(v);
    emit trustedFingerprintsChanged();
}

void KvmController::setState(State s)
{
    if (s == m_state) return;
    const bool wasAwaiting = (m_state == State::AwaitingTrust);
    m_state = s;
    emit stateChanged();
    if (wasAwaiting != (s == State::AwaitingTrust)) emit awaitingTrustChanged();
}

void KvmController::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void KvmController::open()
{
    teardown();
    if (m_host.isEmpty()) {
        setLastError(tr("host is empty"));
        setState(State::Failed);
        return;
    }
    m_framebuffer->clear();

    m_client = new RedirectionClient(this);
    m_client->setProtocol(meshcommander::redir::Protocol::Kvm);
    m_client->setCredentials(m_user, m_password);
    m_client->setTls(m_tls);
    m_client->setTrustedFingerprints(m_trustedFingerprints);

    m_session = new KvmSession(m_client, this);

    connect(m_client.data(), &RedirectionClient::trustPromptRequired, this,
            [this](const meshcommander::redir::PeerCertSummary &summary) {
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
            setState(State::Negotiating);
            break;
        case RedirectionClient::State::Failed:
            setState(State::Failed);
            break;
        }
    });

    connect(m_client.data(), &RedirectionClient::authenticated,
            m_session.data(), &KvmSession::start);

    connect(m_client.data(), &RedirectionClient::failed, this,
            [this](const QString &reason) {
                setLastError(reason);
                setState(State::Failed);
            });

    connect(m_session.data(), &KvmSession::stateChanged, this,
            [this](KvmSession::State s) {
                if (s == KvmSession::State::FrameLoop) {
                    setState(State::Connected);
                }
            });
    connect(m_session.data(), &KvmSession::desktopResized, this,
            [this](int w, int h) {
                m_width = w;
                m_height = h;
                m_framebuffer->resize(w, h);
                emit desktopResized();
            });
    connect(m_session.data(), &KvmSession::tileUpdated, this,
            [this](int x, int y, const QImage &tile) {
                m_framebuffer->applyTile(x, y, tile);
            });
    connect(m_session.data(), &KvmSession::closed, this,
            [this](const QString &reason) {
                setLastError(reason);
                setState(State::Failed);
            });

    setLastError({});
    setState(State::Connecting);
    m_client->connectTo(m_host, m_port);
}

void KvmController::close()
{
    teardown();
    setState(State::Disconnected);
}

void KvmController::trustPendingCert(bool persist)
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

void KvmController::sendCtrlAltDel()
{
    if (m_session) m_session->sendCtrlAltDel();
}

void KvmController::sendKeyTap(quint32 keysym)
{
    if (!m_session) return;
    m_session->sendKey(keysym, true);
    m_session->sendKey(keysym, false);
}

void KvmController::sendKey(quint32 keysym, bool down)
{
    if (m_session) m_session->sendKey(keysym, down);
}

void KvmController::sendPointer(int buttonMask, int x, int y)
{
    if (m_session) {
        m_session->sendPointer(static_cast<quint8>(buttonMask & 0xFF),
                                static_cast<quint16>(qBound(0, x, 65535)),
                                static_cast<quint16>(qBound(0, y, 65535)));
    }
}

void KvmController::teardown()
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

} // namespace meshcommander::app
