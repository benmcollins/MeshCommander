// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvmcontroller.h"

#include "kvm/kvm_codec.h"
#include "kvm/kvm_session.h"
#include "kvmframebuffer.h"
#include "mjpeg_avi_recorder.h"
#include "redir/redir_client.h"
#include "redir/redir_codec.h"
#include "ssh/ssh_session.h"
#include "ssh_tunnel_opener.h"
#include "sshtunnelhost.h"

namespace qumesh::app {

using qumesh::kvm::KvmSession;
using qumesh::redir::RedirectionClient;

KvmController::KvmController(QObject *parent)
    : QObject(parent), m_framebuffer(new KvmFramebuffer(this))
{
    m_recordTimer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&m_recordTimer, &QTimer::timeout, this, [this]() {
        if (!m_recorder || !m_recorder->isRecording()) return;
        if (!m_framebufferDirty) return;
        if (m_framebuffer->image().isNull()) return;
        m_recorder->pushFrame(m_framebuffer->image());
        m_framebufferDirty = false;
    });
    QObject::connect(m_framebuffer, &KvmFramebuffer::tileApplied, this,
                     [this](QRect) { m_framebufferDirty = true; });
}

KvmController::~KvmController() { teardown(); }

void KvmController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
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

void KvmController::setSshConfig(const QVariantMap &cfg)
{
    if (m_sshHost == nullptr) {
        m_sshHost = new SshTunnelHost(this);
        QObject::connect(m_sshHost, &SshTunnelHost::trustedHostKeyAdded, this,
                         &KvmController::trustedSshHostKeyAdded);
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

void KvmController::open()
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
    m_framebuffer->clear();

    m_client = new RedirectionClient(this);
    m_client->setProtocol(qumesh::redir::Protocol::Kvm);
    m_client->setCredentials(m_user, m_password);
    m_client->setTls(m_tls);
    m_client->setTrustedFingerprints(m_trustedFingerprints);
    if (m_sshSession != nullptr) {
        m_client->setTunnelOpener(qumesh::app::makeSshTunnelOpener(m_sshSession));
    }

    m_session = new KvmSession(m_client, this);

    connect(m_client.data(), &RedirectionClient::peerCertVerifiedByPin, this,
            &KvmController::peerCertVerifiedByPin);
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
    // AMT redirection: 16994 plain / 16995 TLS — fixed by the protocol.
    // Tests can override via setPortForTest(); production always derives.
    const quint16 port = m_portOverride != 0 ? m_portOverride
                                              : (m_tls ? 16995 : 16994);
    m_client->connectTo(m_host, port);
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

bool KvmController::saveScreenshot(const QString &path) const
{
    if (path.isEmpty() || m_framebuffer == nullptr) return false;
    const QImage &img = m_framebuffer->image();
    if (img.isNull() || img.width() == 0 || img.height() == 0) return false;
    return img.save(path, "PNG");
}

bool KvmController::isRecording() const
{
    return m_recorder != nullptr && m_recorder->isRecording();
}

bool KvmController::startRecording(const QString &path, int fps)
{
    if (isRecording()) stopRecording();
    if (m_framebuffer == nullptr) return false;
    const QImage &img = m_framebuffer->image();
    if (img.isNull() || img.width() == 0 || img.height() == 0) return false;
    if (fps <= 0 || fps > 60) fps = 5;

    if (m_recorder == nullptr) m_recorder = new MjpegAviRecorder(this);
    if (!m_recorder->start(path, img.width(), img.height(), fps)) return false;

    // Seed the AVI with frame 0 so playback starts on real content
    // rather than a blank frame, even if no tiles arrive in the first
    // sampling interval.
    m_recorder->pushFrame(img);
    m_framebufferDirty = false;

    m_recordTimer.start(1000 / fps);
    emit recordingChanged();
    return true;
}

void KvmController::stopRecording()
{
    if (!isRecording()) return;
    m_recordTimer.stop();
    m_recorder->stop();
    emit recordingChanged();
}

void KvmController::teardown()
{
    if (isRecording()) stopRecording();
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
