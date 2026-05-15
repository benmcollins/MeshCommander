// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "redir/redir_codec.h"

#include <QByteArray>
#include <QObject>
#include <QSslCertificate>
#include <QSslError>
#include <QString>
#include <QStringList>

#include <functional>

class QAbstractSocket;
class QSslSocket;
class QTcpSocket;

namespace qumesh::redir {

/// Summary of a peer cert shown to the user when an unknown
/// certificate is presented during the TLS handshake. The QML layer
/// reads these via Q_PROPERTY on the controller.
struct PeerCertSummary {
    QString subject;
    QString issuer;
    QString fingerprintSha256; ///< Colon-separated uppercase hex.
    QString notBefore;         ///< ISO-8601 string for QML display.
    QString notAfter;          ///< ISO-8601 string for QML display.
    QByteArray der;            ///< Raw cert, in case the consumer wants to save it.
};

} // namespace qumesh::redir

Q_DECLARE_METATYPE(qumesh::redir::PeerCertSummary)

namespace qumesh::redir {

/// Plaintext (port 16994) transport with the full AMT digest auth
/// handshake. TLS (16995) lands in a separate issue.
///
/// State machine (after `connectTo()`):
///
///     Disconnected → Connecting → SelectorSent → SessionOpened
///         → AuthQuerying       (sent 0x13 authType=0)
///         → AuthChallenging    (sent 0x13 authType=4 with user+uri)
///         → AuthResponding     (sent 0x13 authType=4 with full digest)
///         → Authenticated      (server returned 0x14 status=0)
///         | Failed (any step)
///
/// The auth steps only run when `setCredentials()` was called with a
/// non-empty user. Otherwise the client stops at `SessionOpened` so
/// existing tests that don't care about auth keep working.
class RedirectionClient : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Disconnected,
        Connecting,
        SelectorSent,
        SessionOpened,
        AuthQuerying,
        AuthChallenging,
        AuthResponding,
        // KVM has an extra round-trip between auth and the RFB banner:
        // the client must send 0x40 and wait for an 0x41 reply before
        // the firmware starts pushing application bytes. SOL and IDE-R
        // skip this — they go straight to Authenticated.
        KvmStarting,
        Authenticated,
        Failed,
    };
    Q_ENUM(State)

    explicit RedirectionClient(QObject *parent = nullptr);
    ~RedirectionClient() override;

    void setProtocol(Protocol p) { m_protocol = p; }
    void setCredentials(QString user, QString pass);
    void setAuthUri(QString uri) { m_authUri = std::move(uri); }

    /// Enable TLS for the next `connectTo`. Default is plaintext.
    void setTls(bool enabled) { m_tls = enabled; }

    /// Set the list of SHA-256 fingerprints already trusted for the
    /// peer. Fingerprints must be uppercase, colon-separated hex (see
    /// `PeerCertSummary::fingerprintSha256`). On a TLS handshake, if
    /// the presented cert's fingerprint is in this list, the handshake
    /// completes silently; otherwise `trustPromptRequired` fires.
    void setTrustedFingerprints(QStringList fps) { m_trustedFingerprints = std::move(fps); }

    [[nodiscard]] Protocol protocol() const { return m_protocol; }
    [[nodiscard]] bool tls() const { return m_tls; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] StartSessionStatus startStatus() const { return m_startStatus; }
    [[nodiscard]] QByteArray oemData() const { return m_oemData; }
    [[nodiscard]] PeerCertSummary pendingPeerCert() const { return m_pendingPeerCert; }

    void connectTo(const QString &host, quint16 port);

    /// Adopt an already-connected socket descriptor (e.g. one end of a
    /// `socketpair()` that the SSH tunnel feeds). Bypasses the TCP
    /// `connectToHost` step; if `setTls(true)` was called the client
    /// performs the TLS handshake on the adopted fd. Ownership of `fd`
    /// transfers to the client.
    void connectViaSocketDescriptor(qintptr fd);

    /// Install a callback that returns a pre-connected socket fd given
    /// the AMT host + port. When set, `connectTo()` calls the opener
    /// instead of opening a TCP connection — same code path as
    /// `connectViaSocketDescriptor`. Used by the SSH-tunnel controllers
    /// so they can keep the existing `connectTo(host, port)` API.
    using TunnelOpener = std::function<qintptr(const QString &host, quint16 port)>;
    void setTunnelOpener(TunnelOpener opener) { m_tunnelOpener = std::move(opener); }

    void disconnectFromHost();

    /// Called by the consumer (the controller / QML layer) after the
    /// user has chosen to trust the pending peer cert. The TLS
    /// handshake has already completed (we use VerifyNone and pin by
    /// fingerprint post-handshake); this just unblocks the redirection
    /// selector write so the protocol can proceed.
    void trustPendingPeerCert();

    /// Write raw bytes to the underlying socket. Intended for use by an
    /// application-layer driver (`SolSession`, future IDE-R/KVM) after
    /// `Authenticated`. Returns the number of bytes queued or -1.
    qint64 writeRaw(const QByteArray &bytes);

signals:
    void stateChanged(State state);
    void sessionOpened();
    void authenticated();
    void failed(const QString &error);

    /// Emitted for every chunk of bytes received after `Authenticated`. The
    /// client stops parsing once auth completes; the application protocol
    /// is responsible from there on.
    void rawBytes(const QByteArray &chunk);

    /// Emitted when the TLS peer presents a cert whose fingerprint is
    /// not in the trusted list. The handshake is paused; the consumer
    /// must call either `trustPendingPeerCert()` (continue) or
    /// `disconnectFromHost()` (abort).
    void trustPromptRequired(const qumesh::redir::PeerCertSummary &summary);

    /// Emitted when the TLS peer's cert fingerprint is found in the
    /// trusted list — i.e. a silent reconnect with no prompt. The
    /// QML side uses this to flash a small "verified" badge so users
    /// who never see the trust prompt again still get a positive
    /// security signal.
    void peerCertVerifiedByPin(const QString &fingerprint);

private:
    void handleConnected();
    void handleEncrypted();
    void handleReadyRead();
    void handleSocketError();

    /// Pull frames out of `m_inbox`; called whenever new bytes arrive.
    void drainInbox();

    void setState(State s);
    void fail(QString error);

    /// Build a `PeerCertSummary` from a Qt cert object.
    static PeerCertSummary summarize(const QSslCertificate &cert);

    Protocol m_protocol = Protocol::Sol;
    bool m_tls = false;
    TunnelOpener m_tunnelOpener;
    QString m_user;
    QString m_pass;
    QString m_authUri = QStringLiteral("/RedirectionService");
    QAbstractSocket *m_socket = nullptr;
    QSslSocket *m_sslSocket = nullptr;          ///< non-owning alias when TLS
    QByteArray m_inbox;
    State m_state = State::Disconnected;
    QString m_lastError;
    StartSessionStatus m_startStatus = StartSessionStatus::UnknownError;
    QByteArray m_oemData;

    QStringList m_trustedFingerprints;
    PeerCertSummary m_pendingPeerCert;
    bool m_awaitingTrust = false;
};

} // namespace qumesh::redir
