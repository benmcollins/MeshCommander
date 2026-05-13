// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/sol_session.h"

#include "redir/redir_client.h"
#include "redir/redir_codec.h"

#include <QTimer>

namespace meshcommander::redir {

namespace {

constexpr int kKeepaliveIntervalMs = 2000;

}

SolSession::SolSession(RedirectionClient *client, QObject *parent)
    : QObject(parent), m_client(client), m_keepalive(new QTimer(this))
{
    Q_ASSERT(client != nullptr);
    m_keepalive->setInterval(kKeepaliveIntervalMs);
    connect(m_keepalive, &QTimer::timeout, this, &SolSession::sendKeepalive);
    connect(m_client, &RedirectionClient::failed, this, &SolSession::onClientFailed);
    connect(m_client, &RedirectionClient::rawBytes, this, &SolSession::onRawBytes);
}

SolSession::~SolSession() = default;

void SolSession::start()
{
    if (m_client->state() != RedirectionClient::State::Authenticated) {
        fail(QStringLiteral("client is not authenticated"));
        return;
    }
    const QByteArray open = buildSolOpen(m_sequence++);
    if (m_client->writeRaw(open) != open.size()) {
        fail(QStringLiteral("could not write SOL Open: %1").arg(m_client->lastError()));
        return;
    }
}

void SolSession::sendInput(const QByteArray &input)
{
    if (!m_open) return;
    constexpr int kChunk = 0xFFFF;
    for (int off = 0; off < input.size(); off += kChunk) {
        const int n = qMin(kChunk, input.size() - off);
        const QByteArray frame = buildSolDataToHost(QByteArrayView(input.data() + off, n));
        m_client->writeRaw(frame);
    }
}

void SolSession::onClientFailed(const QString &reason)
{
    fail(reason);
}

void SolSession::onRawBytes(const QByteArray &chunk)
{
    m_inbox.append(chunk);
    drainInbox();
}

void SolSession::drainInbox()
{
    while (!m_inbox.isEmpty()) {
        const unsigned char tag = static_cast<unsigned char>(m_inbox.at(0));

        if (tag == 0x21) {
            SolOpenReply reply;
            int consumed = 0;
            if (!tryParseSolOpenReply(m_inbox, &reply, &consumed)) return;
            m_inbox.remove(0, consumed);
            if (reply.status != 0) {
                fail(QStringLiteral("SOL OpenReply status %1").arg(reply.status));
                return;
            }
            if (!m_settingsSent) {
                m_client->writeRaw(buildSolSettings(m_sequence++));
                m_settingsSent = true;
                m_open = true;
                m_keepalive->start();
                emit sessionOpened();
            }
            continue;
        }

        if (tag == 0x2A) {
            QByteArray payload;
            int consumed = 0;
            if (!tryParseSolDisplayData(m_inbox, &payload, &consumed)) return;
            m_inbox.remove(0, consumed);
            if (!payload.isEmpty()) emit data(payload);
            continue;
        }

        if (tag == 0x2B) {
            if (m_inbox.size() < 8) return;
            m_inbox.remove(0, 8);
            continue;
        }

        if (tag == 0x29) {
            // Serial settings echo — 10 bytes, no payload to surface.
            if (m_inbox.size() < 10) return;
            m_inbox.remove(0, 10);
            continue;
        }

        fail(QStringLiteral("unexpected SOL frame 0x%1")
                 .arg(tag, 2, 16, QLatin1Char('0')));
        return;
    }
}

void SolSession::sendKeepalive()
{
    m_client->writeRaw(buildSolKeepalive());
}

void SolSession::fail(QString reason)
{
    m_lastError = std::move(reason);
    m_open = false;
    m_keepalive->stop();
    emit closed(m_lastError);
}

} // namespace meshcommander::redir
