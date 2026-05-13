// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/wsman_client.h"

#include <QAuthenticator>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace qumesh::wsman {

WsmanClient::WsmanClient(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::authenticationRequired, this,
            &WsmanClient::handleAuthenticationRequired);
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
                  QStringLiteral("MeshCommander-Qt/0.1"));
    if (soapAction != nullptr) {
        req.setRawHeader("SOAPAction", soapAction);
    }
    req.setTransferTimeout(m_transferTimeoutMs);

    return m_nam->post(req, envelope);
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
