// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QAuthenticator;

namespace meshcommander::wsman {

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

    /// Per-request transfer timeout. Default 30 s. Pass 0 to disable.
    void setTransferTimeoutMs(int ms);

    [[nodiscard]] QUrl endpoint() const { return m_endpoint; }

    /// POST the given SOAP envelope to the endpoint. The returned reply is
    /// owned by Qt; the caller should connect to `finished` and call
    /// `deleteLater()` when done. The optional `soapAction` is set as the
    /// `SOAPAction` HTTP header.
    QNetworkReply *sendEnvelope(const QByteArray &envelope,
                                const char *soapAction = nullptr);

private:
    void handleAuthenticationRequired(QNetworkReply *reply, QAuthenticator *auth);

    QNetworkAccessManager *m_nam = nullptr;
    QUrl m_endpoint;
    QString m_user;
    QString m_pass;
    int m_transferTimeoutMs = 30000;
};

} // namespace meshcommander::wsman
