// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/wsman_client.h"

#include <QAuthenticator>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QUrl>

namespace qumesh::wsman {

namespace {

/// Match the formatting used elsewhere: uppercase hex, colon-separated.
QString fingerprintFor(const QSslCertificate &cert)
{
    const QByteArray hash = QCryptographicHash::hash(cert.toDer(),
                                                      QCryptographicHash::Sha256);
    QString out;
    out.reserve(hash.size() * 3);
    for (int i = 0; i < hash.size(); ++i) {
        if (i > 0) out += QLatin1Char(':');
        out += QString::asprintf("%02X", static_cast<quint8>(hash.at(i)));
    }
    return out;
}

} // namespace

WsmanClient::WsmanClient(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::authenticationRequired, this,
            &WsmanClient::handleAuthenticationRequired);
    connect(m_nam, &QNetworkAccessManager::sslErrors, this,
            &WsmanClient::handleSslErrors);
}

void WsmanClient::setEndpoint(QUrl endpoint)
{
    m_endpoint = std::move(endpoint);
}

void WsmanClient::setCredentials(QString user, QString pass)
{
    m_user = std::move(user);
    m_pass = std::move(pass);
}

void WsmanClient::setTrustedFingerprints(QStringList fingerprints)
{
    m_trustedFingerprints = std::move(fingerprints);
}

void WsmanClient::setTransferTimeoutMs(int ms)
{
    m_transferTimeoutMs = ms;
}

QNetworkReply *WsmanClient::sendEnvelope(const QByteArray &envelope, const char *soapAction)
{
    QNetworkRequest req(m_endpoint);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/soap+xml;charset=UTF-8"));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("QuMesh/0.1"));
    if (soapAction != nullptr) {
        req.setRawHeader("SOAPAction", soapAction);
    }
    req.setTransferTimeout(m_transferTimeoutMs);

    return m_nam->post(req, envelope);
}

void WsmanClient::trustPendingPeerCert()
{
    if (!m_awaitingTrust || m_pendingPeerCert.fingerprintSha256.isEmpty()) return;
    if (!m_trustedFingerprints.contains(m_pendingPeerCert.fingerprintSha256))
        m_trustedFingerprints.append(m_pendingPeerCert.fingerprintSha256);
    m_pendingPeerCert = {};
    m_awaitingTrust = false;
}

void WsmanClient::handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    // Qt only emits sslErrors when default validation found a problem:
    // hostname mismatch, self-signed chain, etc. AMT ships a self-
    // signed cert whose CN never matches a port-forwarded localhost
    // endpoint, so we don't fall through to the default rejection.
    // Two outcomes:
    //
    //   * cert is pinned (fingerprint in m_trustedFingerprints) →
    //     ignoreSslErrors and let the handshake complete;
    //   * not pinned → capture the cert, emit a trust-prompt signal,
    //     do *not* ignore the errors → the reply will fail with
    //     SslHandshakeFailedError, the controller surfaces the dialog,
    //     and on accept it calls trustPendingPeerCert() + retries.
    const QList<QSslCertificate> chain = reply->sslConfiguration().peerCertificateChain();
    if (chain.isEmpty()) return;
    const QSslCertificate &leaf = chain.first();
    const QString fp = fingerprintFor(leaf);

    if (m_trustedFingerprints.contains(fp)) {
        emit peerCertVerifiedByPin(fp);
        reply->ignoreSslErrors(errors);
        return;
    }

    PeerCertSummary s;
    const auto subjects = leaf.subjectInfo(QSslCertificate::CommonName);
    const auto issuers  = leaf.issuerInfo(QSslCertificate::CommonName);
    if (!subjects.isEmpty()) s.subject = subjects.first();
    if (!issuers.isEmpty())  s.issuer  = issuers.first();
    s.fingerprintSha256 = fp;
    s.notBefore = leaf.effectiveDate().toString(Qt::ISODate);
    s.notAfter  = leaf.expiryDate().toString(Qt::ISODate);
    s.der = leaf.toDer();
    m_pendingPeerCert = s;
    m_awaitingTrust = true;
    emit trustPromptRequired(s);
}

void WsmanClient::handleAuthenticationRequired(QNetworkReply *, QAuthenticator *auth)
{
    // Only feed the credentials once per reply. Re-prompting means the server
    // has rejected them, in which case we let QNetworkAccessManager give up
    // (otherwise we loop on a 401).
    if (auth->user().isEmpty() && !m_user.isEmpty()) {
        auth->setUser(m_user);
        auth->setPassword(m_pass);
    }
}

} // namespace qumesh::wsman
