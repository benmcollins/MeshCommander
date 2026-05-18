// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

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

class WsmanClient;

/// One in-flight WSMAN request. Mirrors the pieces of `QNetworkReply`
/// the existing `operations.cpp` helpers consume: a `finished()` signal
/// plus accessors for the response body and any error.
class WsmanReply : public QObject
{
    Q_OBJECT
public:
    explicit WsmanReply(QObject *parent = nullptr);
    ~WsmanReply() override;

    [[nodiscard]] QByteArray readAll() const;
    [[nodiscard]] bool hasError() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] int httpStatus() const;

    void abort();

    // Exposed for use by `wsman_client.cpp` internal helpers; not part
    // of the API. `Private` is defined only in the .cpp file.
    struct Private;
    std::unique_ptr<Private> d;

signals:
    void finished();
};

/// Transport layer for WSMAN/SOAP requests against an AMT device.
///
/// Hand-rolled HTTP/1.1 client on top of QSslSocket (used as a plain
/// QTcpSocket for `http://` endpoints and with `startClientEncryption()`
/// for `https://`). The previous QNetworkAccessManager-based implementation
/// would not let us inject a pre-connected socket; this rewrite is what
/// makes routing every byte through an SSH tunnel possible.
///
/// Authentication: HTTP Digest (RFC 7616 with MD5; AMT firmware does not
/// require MD5-sess or SHA-256 yet). Request flow:
///   1. POST without Authorization
///   2. On 401 with `WWW-Authenticate: Digest`, parse the challenge,
///      compute the response, and re-POST with `Authorization: Digest`
///   3. Read the response body via `Content-Length` or chunked transfer
class WsmanClient : public QObject
{
    Q_OBJECT
public:
    explicit WsmanClient(QObject *parent = nullptr);
    ~WsmanClient() override;

    /// Set the device's WSMAN endpoint URL, e.g.
    /// `http://10.0.0.5:16992/wsman` or `https://.../wsman`. The host and
    /// port are used as the `Host` header and the TLS SNI; the path is
    /// used as the request URI.
    void setEndpoint(QUrl endpoint);

    /// Credentials for HTTP Digest.
    void setCredentials(QString user, QString pass);

    /// Pinned peer-cert fingerprints. When TLS is in use, the SHA-256
    /// fingerprint of the presented cert is compared against this list
    /// during the handshake; on match, hostname-mismatch / chain-of-trust
    /// SSL errors are silenced (AMT firmware ships self-signed certs).
    void setTrustedFingerprints(QStringList fingerprints);

    /// Per-request transfer timeout. Default 30 s. Pass 0 to disable.
    void setTransferTimeoutMs(int ms);

    /// Optional async socket factory. When set, each new request takes
    /// its transport socket from this callback (one fresh fd per reply)
    /// instead of opening a fresh TCP connection to the endpoint's
    /// host/port. Used by the per-machine SSH tunnel: the factory
    /// kicks off a new `SshTunnel` open and invokes the completion
    /// callback with the Qt-side fd of its socketpair when ready, or
    /// with `fd < 0` plus an error message on failure. The factory
    /// must invoke the callback exactly once, even on error; the
    /// request's transfer timeout covers the open window too.
    using SocketCallback = std::function<void(qintptr fd, QString error)>;
    using SocketFactory  = std::function<void(SocketCallback)>;
    void setSocketFactory(SocketFactory factory);

    /// Serialize outbound requests so at most one is in flight at a
    /// time. AMT firmware typically only accepts ~2 concurrent HTTPS
    /// WSMAN sessions; through an SSH tunnel the failure mode is
    /// silent stall rather than fast TCP RST, so the
    /// `MachineDetailsController` flips this on while the tunnel is
    /// active. When false (the default) requests run in parallel —
    /// the historical behavior over a direct connection.
    void setSerializeRequests(bool serialize);

    [[nodiscard]] QUrl endpoint() const;

    /// POST the given SOAP envelope to the endpoint. Returned reply is
    /// owned by Qt; the caller should connect to `finished` and call
    /// `deleteLater()` when done.
    WsmanReply *sendEnvelope(const QByteArray &envelope,
                              const char *soapAction = nullptr);

    [[nodiscard]] PeerCertSummary pendingPeerCert() const;
    [[nodiscard]] bool awaitingTrust() const;

    /// Promote the pending cert's fingerprint into the trusted list.
    void trustPendingPeerCert();

    // Exposed for use by `wsman_client.cpp` internal helpers; not part
    // of the API.
    struct Private;
    std::unique_ptr<Private> d;

signals:
    /// Emitted from inside the TLS handshake when the presented peer
    /// cert is not in `m_trustedFingerprints`. The triggering reply
    /// fails with an SSL handshake error; the controller surfaces a
    /// trust prompt and calls `trustPendingPeerCert()` on accept, then
    /// retries the request.
    void trustPromptRequired(const qumesh::wsman::PeerCertSummary &summary);

    /// Emitted when the cert presented during the handshake matches a
    /// pinned fingerprint.
    void peerCertVerifiedByPin(const QString &fingerprint);
};

} // namespace qumesh::wsman
