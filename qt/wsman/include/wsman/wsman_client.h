// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QList>
#include <QNetworkReply>
#include <QObject>
#include <QSslError>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QAuthenticator;

namespace qumesh::wsman {

/// Snapshot of a peer cert presented during a WSMAN TLS handshake. Used
/// by the controller to drive a trust-on-first-use prompt; mirrors the
/// shape of `qumesh::redir::PeerCertSummary` so the same `CertTrustDialog`
/// can render it.
struct PeerCertSummary
{
    QString subject;
    QString issuer;
    QString fingerprintSha256; ///< Colon-separated uppercase hex.
    QString notBefore;
    QString notAfter;
    QByteArray der;
};

/// Transport layer for WSMAN/SOAP requests against an AMT device.
///
/// Owns a `QNetworkAccessManager` and feeds it credentials when the server
/// challenges (HTTP Digest, which AMT uses by default on the WSMAN port).
/// Each call to `sendEnvelope` returns the raw `QNetworkReply`; callers
/// connect to `QNetworkReply::finished` and read the body themselves.
/// The client deliberately does not consume the body itself so it never
/// races with the caller's handler.
class WsmanClient : public QObject
{
    Q_OBJECT
public:
    explicit WsmanClient(QObject *parent = nullptr);

    /// Set the device's WSMAN endpoint URL, e.g.
    /// `http://10.0.0.5:16992/wsman` or `https://.../wsman`.
    void setEndpoint(QUrl endpoint);

    /// Credentials to satisfy HTTP Digest challenges. Empty user disables
    /// auto-authentication (requests will fail with 401 if the server
    /// requires authentication).
    void setCredentials(QString user, QString pass);

    /// Pinned peer-cert fingerprints. When TLS is in use, the SHA-256
    /// fingerprint of the presented cert is compared against this list
    /// during the handshake; on match, hostname-mismatch / chain-of-trust
    /// SSL errors are silenced (AMT firmware ships self-signed certs
    /// whose CN never matches port-forwarded localhost endpoints). When
    /// empty, only Qt's default validation applies.
    void setTrustedFingerprints(QStringList fingerprints);

    /// Per-request transfer timeout. Default 30 s. Pass 0 to disable.
    void setTransferTimeoutMs(int ms);

    [[nodiscard]] QUrl endpoint() const { return m_endpoint; }

    /// POST the given SOAP envelope to the endpoint. The returned reply is
    /// owned by Qt; the caller should connect to `finished` and call
    /// `deleteLater()` when done. The optional `soapAction` is set as the
    /// `SOAPAction` HTTP header.
    QNetworkReply *sendEnvelope(const QByteArray &envelope,
                                const char *soapAction = nullptr);

    /// The cert captured during the most recent `trustPromptRequired`
    /// emission. Valid until the controller calls `trustPendingPeerCert()`
    /// or clears it.
    [[nodiscard]] PeerCertSummary pendingPeerCert() const { return m_pendingPeerCert; }
    [[nodiscard]] bool awaitingTrust() const { return m_awaitingTrust; }

    /// Promote the pending cert's fingerprint into the trusted list and
    /// clear the pending state. Caller is responsible for retrying the
    /// failed request — this method does not re-send anything.
    void trustPendingPeerCert();

signals:
    /// Emitted from inside the `sslErrors` handler when the presented
    /// peer cert is not in `m_trustedFingerprints`. The reply that
    /// triggered the prompt will fail with `SslHandshakeFailedError`;
    /// the controller is expected to surface a trust prompt to the user
    /// and call `trustPendingPeerCert()` if accepted.
    void trustPromptRequired(const qumesh::wsman::PeerCertSummary &summary);

private:
    void handleAuthenticationRequired(QNetworkReply *reply, QAuthenticator *auth);

    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

    QNetworkAccessManager *m_nam = nullptr;
    QUrl m_endpoint;
    QString m_user;
    QString m_pass;
    QStringList m_trustedFingerprints;
    PeerCertSummary m_pendingPeerCert;
    bool m_awaitingTrust = false;
    int m_transferTimeoutMs = 30000;
};

} // namespace qumesh::wsman
