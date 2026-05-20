// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQmlIntegration>

#include "redir/redir_client.h"

namespace qumesh::ider {
class IderSession;
}

namespace qumesh::ssh { class SshSession; }

namespace qumesh::app {

class SshTunnelHost;

/// QML-facing controller for one IDE-R session. Owns the redirection
/// client and the IDE-R session driver. The QML layer sets `host`,
/// `port`, `user`, `password`, `isoPath`, and `startOption`, then calls
/// `open()`.
class IderController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString isoPath READ isoPath WRITE setIsoPath NOTIFY isoPathChanged)
    Q_PROPERTY(StartOption startOption READ startOption WRITE setStartOption NOTIFY startOptionChanged)

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(quint64 bytesSentToAmt READ bytesSentToAmt NOTIFY statsChanged)
    Q_PROPERTY(quint64 bytesReceivedFromAmt READ bytesReceivedFromAmt NOTIFY statsChanged)
    Q_PROPERTY(bool deviceEnabled READ deviceEnabled NOTIFY deviceEnabledChanged)
    Q_PROPERTY(bool tls READ tls WRITE setTls NOTIFY tlsChanged)
    Q_PROPERTY(QStringList trustedFingerprints READ trustedFingerprints
                   WRITE setTrustedFingerprints NOTIFY trustedFingerprintsChanged)
    Q_PROPERTY(QString pendingCertSubject READ pendingCertSubject NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertIssuer READ pendingCertIssuer NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertFingerprint READ pendingCertFingerprint NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotBefore READ pendingCertNotBefore NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotAfter READ pendingCertNotAfter NOTIFY pendingCertChanged)
    Q_PROPERTY(bool awaitingTrust READ awaitingTrust NOTIFY awaitingTrustChanged)

    // SSH host-key prompt — see `SshHostKeyTrustDialog.qml`. Bound
    // when the per-machine SSH tunnel encounters an unpinned key.
    Q_PROPERTY(bool awaitingSshHostKeyTrust READ awaitingSshHostKeyTrust
                   NOTIFY sshHostKeyPromptChanged)
    Q_PROPERTY(QString pendingSshHostKeyFingerprint READ pendingSshHostKeyFingerprint
                   NOTIFY sshHostKeyPromptChanged)
    Q_PROPERTY(QString pendingSshHostKeyType READ pendingSshHostKeyType
                   NOTIFY sshHostKeyPromptChanged)

public:
    enum class State {
        Disconnected,
        Connecting,
        Authenticating,
        AwaitingTrust,
        Opening,
        Running,
        Failed,
    };
    Q_ENUM(State)

    enum class StartOption {
        OnReboot   = 0,
        Graceful   = 1,
        Immediate  = 2,
    };
    Q_ENUM(StartOption)

    explicit IderController(QObject *parent = nullptr);
    ~IderController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] QString isoPath() const { return m_isoPath; }
    [[nodiscard]] StartOption startOption() const { return m_startOption; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] quint64 bytesSentToAmt() const { return m_bytesSentToAmt; }
    [[nodiscard]] quint64 bytesReceivedFromAmt() const { return m_bytesReceivedFromAmt; }
    [[nodiscard]] bool deviceEnabled() const { return m_deviceEnabled; }
    [[nodiscard]] bool tls() const { return m_tls; }
    [[nodiscard]] QStringList trustedFingerprints() const { return m_trustedFingerprints; }
    [[nodiscard]] bool awaitingTrust() const { return m_state == State::AwaitingTrust; }
    [[nodiscard]] QString pendingCertSubject() const { return m_pendingCert.subject; }
    [[nodiscard]] QString pendingCertIssuer() const { return m_pendingCert.issuer; }
    [[nodiscard]] QString pendingCertFingerprint() const { return m_pendingCert.fingerprintSha256; }
    [[nodiscard]] QString pendingCertNotBefore() const { return m_pendingCert.notBefore; }
    [[nodiscard]] QString pendingCertNotAfter() const { return m_pendingCert.notAfter; }
    [[nodiscard]] bool awaitingSshHostKeyTrust() const { return m_awaitingSshHostKeyTrust; }
    [[nodiscard]] QString pendingSshHostKeyFingerprint() const { return m_pendingSshHostKey; }
    [[nodiscard]] QString pendingSshHostKeyType() const { return m_pendingSshHostKeyType; }

    void setHost(const QString &v);
    /// Override the redirection port. Tests only — production derives
    /// 16994 / 16995 from `tls`. Not exposed to QML.
    void setPortForTest(quint16 v) { m_portOverride = v; }
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setIsoPath(const QString &v);
    void setStartOption(StartOption v);
    void setTls(bool v);
    void setTrustedFingerprints(QStringList v);
    void setSshSession(qumesh::ssh::SshSession *session) { m_sshSession = session; }
    /// QML-friendly: take the per-machine SSH config (as produced by
    /// `ComputerModel::sshConfigFor`). When enabled, the controller
    /// owns an `SshTunnelHost` that opens the session lazily; `open()`
    /// then waits for the session to reach `Connected` before
    /// dialing the AMT host.
    Q_INVOKABLE void setSshConfig(const QVariantMap &cfg);

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void trustPendingCert(bool persist);
    /// Resolves a pending SSH host-key prompt (see `SshTunnelHost`).
    /// On accept, resumes the paused SSH session; with `persist` also
    /// forwards the fingerprint via `trustedSshHostKeyAdded` so the
    /// QML layer can pin it on the per-machine config.
    Q_INVOKABLE void trustPendingSshHostKey(bool persist);

signals:
    void hostChanged();
    void userChanged();
    void passwordChanged();
    void isoPathChanged();
    void startOptionChanged();
    void stateChanged();
    void lastErrorChanged();
    void statsChanged();
    void deviceEnabledChanged();
    void tlsChanged();
    void trustedFingerprintsChanged();
    void pendingCertChanged();
    void awaitingTrustChanged();
    void trustedFingerprintAdded(const QString &fingerprint);
    /// Emitted when the per-machine SSH tunnel encounters an unpinned
    /// host key. QML surfaces `SshHostKeyTrustDialog`; the user accepts
    /// via `trustPendingSshHostKey(persist)` or cancels by closing the
    /// session.
    void sshHostKeyPromptRequired(const QString &fingerprint,
                                   const QString &keyType);
    /// NOTIFY for the `awaitingSshHostKeyTrust` / pending properties.
    void sshHostKeyPromptChanged();
    /// Emitted after `trustPendingSshHostKey(true)` so the QML layer
    /// can persist the fingerprint into ComputerModel.
    void trustedSshHostKeyAdded(const QString &fingerprint);
    /// Forwarded from the underlying client whenever a TLS
    /// reconnect quietly matched a pinned fingerprint. The QML side
    /// uses it to flash a small "verified" badge.
    void peerCertVerifiedByPin(const QString &fingerprint);

private:
    void setState(State s);
    void setLastError(const QString &e);
    void teardown();

    QString m_host;
    quint16 m_portOverride = 0;
    QString m_user;
    QString m_password;
    QString m_isoPath;
    StartOption m_startOption = StartOption::Graceful;

    State m_state = State::Disconnected;
    QString m_lastError;
    quint64 m_bytesSentToAmt = 0;
    quint64 m_bytesReceivedFromAmt = 0;
    bool m_deviceEnabled = false;
    bool m_tls = false;
    QStringList m_trustedFingerprints;
    qumesh::redir::PeerCertSummary m_pendingCert;

    QPointer<qumesh::redir::RedirectionClient> m_client;
    QPointer<qumesh::ider::IderSession> m_session;
    qumesh::ssh::SshSession *m_sshSession = nullptr;
    SshTunnelHost *m_sshHost = nullptr;
    bool m_openDeferred = false;

    QString m_pendingSshHostKey;
    QString m_pendingSshHostKeyType;
    bool m_awaitingSshHostKeyTrust = false;
};

} // namespace qumesh::app
