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
#include "terminal/terminalscreen.h"

namespace qumesh::redir {
class RedirectionClient;
class SolSession;
} // namespace qumesh::redir

namespace qumesh::ssh { class SshSession; }

namespace qumesh::app {

class AsciicastRecorder;
class SshTunnelHost;

/// QML-facing controller for one SOL session. Owns the redirection
/// client, the SOL session driver, and the terminal screen the QML
/// layer renders. Created per SOL window — the host pane instantiates
/// it, sets `host`/`port`/`user`/`password`, and calls `open()`.
class SolController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qumesh::terminal::TerminalScreen *screen READ screen CONSTANT)
    Q_PROPERTY(bool tls READ tls WRITE setTls NOTIFY tlsChanged)
    Q_PROPERTY(QStringList trustedFingerprints READ trustedFingerprints
                   WRITE setTrustedFingerprints NOTIFY trustedFingerprintsChanged)
    Q_PROPERTY(QString pendingCertSubject READ pendingCertSubject NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertIssuer READ pendingCertIssuer NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertFingerprint READ pendingCertFingerprint NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotBefore READ pendingCertNotBefore NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotAfter READ pendingCertNotAfter NOTIFY pendingCertChanged)
    Q_PROPERTY(bool awaitingTrust READ awaitingTrust NOTIFY awaitingTrustChanged)
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)

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
        Connected,
        Failed,
    };
    Q_ENUM(State)

    explicit SolController(QObject *parent = nullptr);
    ~SolController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] bool tls() const { return m_tls; }
    [[nodiscard]] QStringList trustedFingerprints() const { return m_trustedFingerprints; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] bool awaitingTrust() const { return m_state == State::AwaitingTrust; }
    [[nodiscard]] QString pendingCertSubject() const { return m_pendingCert.subject; }
    [[nodiscard]] QString pendingCertIssuer() const { return m_pendingCert.issuer; }
    [[nodiscard]] QString pendingCertFingerprint() const { return m_pendingCert.fingerprintSha256; }
    [[nodiscard]] QString pendingCertNotBefore() const { return m_pendingCert.notBefore; }
    [[nodiscard]] QString pendingCertNotAfter() const { return m_pendingCert.notAfter; }
    [[nodiscard]] qumesh::terminal::TerminalScreen *screen() const { return m_screen; }
    [[nodiscard]] bool isRecording() const;
    [[nodiscard]] bool awaitingSshHostKeyTrust() const { return m_awaitingSshHostKeyTrust; }
    [[nodiscard]] QString pendingSshHostKeyFingerprint() const { return m_pendingSshHostKey; }
    [[nodiscard]] QString pendingSshHostKeyType() const { return m_pendingSshHostKeyType; }

    void setHost(const QString &v);
    /// Override the redirection port. Only exists for tests that need to
    /// dial a mock server on an ephemeral port — production code lets the
    /// controller derive 16994 / 16995 from `tls`. Not exposed to QML.
    void setPortForTest(quint16 v) { m_portOverride = v; }
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setTls(bool v);
    void setTrustedFingerprints(QStringList v);

    /// When non-null, `open()` opens the session via an SSH tunnel
    /// over the supplied session rather than connecting directly. The
    /// pointer is non-owning; the host (e.g. SessionWindow) keeps the
    /// session alive for the controller's lifetime.
    void setSshSession(qumesh::ssh::SshSession *session) { m_sshSession = session; }

    /// QML-friendly: take the per-machine SSH config (as produced by
    /// `ComputerModel::sshConfigFor`). When enabled, the controller
    /// owns an `SshTunnelHost` that opens the session lazily; `open()`
    /// then waits for the session to reach `Connected` before
    /// dialing the AMT host.
    Q_INVOKABLE void setSshConfig(const QVariantMap &cfg);

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void sendBytes(const QByteArray &bytes);

    /// Called by the QML trust prompt when the user accepts the
    /// presented cert. Resumes the TLS handshake and (when `persist`
    /// is true) emits `trustedFingerprintAdded` so the QML layer can
    /// persist it via ComputerModel.
    Q_INVOKABLE void trustPendingCert(bool persist);
    /// Resolves a pending SSH host-key prompt (see `SshTunnelHost`).
    /// On accept, resumes the paused SSH session; with `persist` also
    /// emits `trustedSshHostKeyAdded` so the QML layer can pin the
    /// fingerprint on the per-machine config.
    Q_INVOKABLE void trustPendingSshHostKey(bool persist);

    /// Start streaming the SOL output to `path` as asciicast v2. The
    /// recorded file is replayable by asciinema / asciinema-player.
    /// Returns true if the file was opened and the header written.
    /// Bytes that arrived before `startRecording` are not retroactively
    /// included — the recording starts from this point forward.
    Q_INVOKABLE bool startRecording(const QString &path,
                                    const QString &title = QString());
    /// Stop the in-progress recording. Safe to call when not recording.
    Q_INVOKABLE void stopRecording();

signals:
    void hostChanged();
    void userChanged();
    void passwordChanged();
    void tlsChanged();
    void trustedFingerprintsChanged();
    void stateChanged();
    void lastErrorChanged();
    void pendingCertChanged();
    void awaitingTrustChanged();
    /// Emitted after `trustPendingCert(true)` so the QML layer can
    /// persist the fingerprint into ComputerModel.
    void trustedFingerprintAdded(const QString &fingerprint);
    /// Emitted when the per-machine SSH tunnel encounters an unpinned
    /// host key. QML surfaces `SshHostKeyTrustDialog`.
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
    void recordingChanged();

private:
    void setState(State s);
    void setLastError(const QString &e);
    void teardown();

    QString m_host;
    quint16 m_portOverride = 0;
    QString m_user;
    QString m_password;
/// One-shot latch for the "first data" log line. The SOL byte stream
    /// is never logged — see the note at the `SolSession::data` handler.
    bool m_loggedFirstData = false;
    bool m_tls = false;
    QStringList m_trustedFingerprints;
    State m_state = State::Disconnected;
    QString m_lastError;
    qumesh::redir::PeerCertSummary m_pendingCert;

    qumesh::terminal::TerminalScreen *m_screen;
    QPointer<qumesh::redir::RedirectionClient> m_client;
    QPointer<qumesh::redir::SolSession> m_session;
    qumesh::ssh::SshSession *m_sshSession = nullptr;
    SshTunnelHost *m_sshHost = nullptr;
    /// `true` when the user pressed Open while the SSH session was
    /// still negotiating; the controller resumes the dial once the
    /// host reaches Connected.
    bool m_openDeferred = false;
    AsciicastRecorder *m_recorder = nullptr;

    QString m_pendingSshHostKey;
    QString m_pendingSshHostKeyType;
    bool m_awaitingSshHostKeyTrust = false;
};

} // namespace qumesh::app
