// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "kvmframebuffer.h"
#include "redir/redir_client.h"

namespace qumesh::kvm {
class KvmSession;
}

namespace qumesh::app {

/// QML-creatable controller for one KVM session. Owns the redirection
/// client, the KVM session driver, and the framebuffer the QML viewer
/// renders.
class KvmController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int desktopWidth READ desktopWidth NOTIFY desktopResized)
    Q_PROPERTY(int desktopHeight READ desktopHeight NOTIFY desktopResized)
    Q_PROPERTY(KvmFramebuffer *framebuffer READ framebuffer CONSTANT)
    Q_PROPERTY(bool tls READ tls WRITE setTls NOTIFY tlsChanged)
    Q_PROPERTY(QStringList trustedFingerprints READ trustedFingerprints
                   WRITE setTrustedFingerprints NOTIFY trustedFingerprintsChanged)
    Q_PROPERTY(QString pendingCertSubject READ pendingCertSubject NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertIssuer READ pendingCertIssuer NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertFingerprint READ pendingCertFingerprint NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotBefore READ pendingCertNotBefore NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotAfter READ pendingCertNotAfter NOTIFY pendingCertChanged)
    Q_PROPERTY(bool awaitingTrust READ awaitingTrust NOTIFY awaitingTrustChanged)

public:
    enum class State {
        Disconnected,
        Connecting,
        Authenticating,
        AwaitingTrust,
        Negotiating,
        Connected,
        Failed,
    };
    Q_ENUM(State)

    explicit KvmController(QObject *parent = nullptr);
    ~KvmController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] quint16 port() const { return m_port; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] int desktopWidth() const { return m_width; }
    [[nodiscard]] int desktopHeight() const { return m_height; }
    [[nodiscard]] KvmFramebuffer *framebuffer() const { return m_framebuffer; }
    [[nodiscard]] bool tls() const { return m_tls; }
    [[nodiscard]] QStringList trustedFingerprints() const { return m_trustedFingerprints; }
    [[nodiscard]] bool awaitingTrust() const { return m_state == State::AwaitingTrust; }
    [[nodiscard]] QString pendingCertSubject() const { return m_pendingCert.subject; }
    [[nodiscard]] QString pendingCertIssuer() const { return m_pendingCert.issuer; }
    [[nodiscard]] QString pendingCertFingerprint() const { return m_pendingCert.fingerprintSha256; }
    [[nodiscard]] QString pendingCertNotBefore() const { return m_pendingCert.notBefore; }
    [[nodiscard]] QString pendingCertNotAfter() const { return m_pendingCert.notAfter; }

    void setHost(const QString &v);
    void setPort(quint16 v);
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setTls(bool v);
    void setTrustedFingerprints(QStringList v);

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void trustPendingCert(bool persist);
    Q_INVOKABLE void sendCtrlAltDel();
    /// Send a single key tap (down → up) by X11 keysym.
    Q_INVOKABLE void sendKeyTap(quint32 keysym);
    /// Press/release one key. `down=true` for press.
    Q_INVOKABLE void sendKey(quint32 keysym, bool down);
    /// Mouse pointer event in framebuffer coordinates.
    Q_INVOKABLE void sendPointer(int buttonMask, int x, int y);

signals:
    void hostChanged();
    void portChanged();
    void userChanged();
    void passwordChanged();
    void stateChanged();
    void lastErrorChanged();
    void desktopResized();
    void tlsChanged();
    void trustedFingerprintsChanged();
    void pendingCertChanged();
    void awaitingTrustChanged();
    void trustedFingerprintAdded(const QString &fingerprint);

private:
    void setState(State s);
    void setLastError(const QString &e);
    void teardown();

    QString m_host;
    quint16 m_port = 16994;
    QString m_user;
    QString m_password;
    State m_state = State::Disconnected;
    QString m_lastError;
    int m_width = 0;
    int m_height = 0;
    bool m_tls = false;
    QStringList m_trustedFingerprints;
    qumesh::redir::PeerCertSummary m_pendingCert;

    KvmFramebuffer *m_framebuffer;
    QPointer<qumesh::redir::RedirectionClient> m_client;
    QPointer<qumesh::kvm::KvmSession> m_session;
};

} // namespace qumesh::app
