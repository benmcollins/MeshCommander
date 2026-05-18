// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "rmcp/scanner.h"

#include <QHostInfo>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>
#include <QUdpSocket>

namespace qumesh::rmcp {

namespace {

constexpr quint16 kAmtRmcpPort = 623;
// Spread the probes across small bursts so the UDP socket buffer isn't
// flooded on a /16-sized scan. 32 probes every 10 ms walks a /24 in
// ~80 ms and a /16 in ~20 s.
constexpr int kBurstSize       = 32;
constexpr int kBurstIntervalMs = 10;
// AMT typically replies within a few hundred ms; legacy MeshCommander
// waited 5 s after the last send. Match that.
constexpr int kQuietWindowMs   = 5000;

} // namespace

Scanner::Scanner(QObject *parent)
    : QObject(parent)
{
}

Scanner::~Scanner() = default;

bool Scanner::start(const QList<QHostAddress> &targets)
{
    stop();

    if (targets.isEmpty()) {
        m_lastError = QStringLiteral("No targets to scan.");
        return false;
    }

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress(QHostAddress::AnyIPv4), 0)) {
        m_lastError = m_socket->errorString();
        m_socket->deleteLater();
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &Scanner::onReadyRead);

    m_pending = targets;
    m_nextIndex = 0;
    // Fresh tag per scan so stale replies from a previous run get
    // discarded on the way in.
    m_tag = static_cast<quint8>(QRandomGenerator::global()->bounded(1, 256));
    m_lastError.clear();

    m_sendTimer = new QTimer(this);
    m_sendTimer->setInterval(kBurstIntervalMs);
    connect(m_sendTimer, &QTimer::timeout, this, &Scanner::onSendBurst);
    // Fire the first burst immediately so a single-IP scan doesn't wait
    // 10 ms before going out.
    onSendBurst();
    if (m_nextIndex < m_pending.size())
        m_sendTimer->start();
    return true;
}

void Scanner::stop()
{
    teardownTimers();
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (!m_pending.isEmpty()) {
        m_pending.clear();
        m_nextIndex = 0;
        emit finished();
    }
}

void Scanner::teardownTimers()
{
    if (m_sendTimer) {
        m_sendTimer->stop();
        m_sendTimer->deleteLater();
        m_sendTimer = nullptr;
    }
    if (m_waitTimer) {
        m_waitTimer->stop();
        m_waitTimer->deleteLater();
        m_waitTimer = nullptr;
    }
}

void Scanner::onSendBurst()
{
    if (!m_socket)
        return;

    const QByteArray ping = buildPing(m_tag);
    const int end = qMin(m_nextIndex + kBurstSize, m_pending.size());
    for (int i = m_nextIndex; i < end; ++i)
        m_socket->writeDatagram(ping, m_pending[i], kAmtRmcpPort);
    m_nextIndex = end;

    if (m_nextIndex >= m_pending.size()) {
        if (m_sendTimer) {
            m_sendTimer->stop();
            m_sendTimer->deleteLater();
            m_sendTimer = nullptr;
        }
        // Sends are done — start the quiet window. Late replies after
        // this fires get dropped, which is fine for a one-shot scan.
        m_waitTimer = new QTimer(this);
        m_waitTimer->setSingleShot(true);
        m_waitTimer->setInterval(kQuietWindowMs);
        connect(m_waitTimer, &QTimer::timeout,
                this, &Scanner::onWaitElapsed);
        m_waitTimer->start();
    }
}

void Scanner::onReadyRead()
{
    if (!m_socket)
        return;

    while (m_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        const qint64 n = m_socket->readDatagram(buf.data(), buf.size(),
                                                &sender, &senderPort);
        if (n <= 0)
            continue;
        buf.truncate(static_cast<int>(n));

        const auto parsed = parsePong(buf);
        if (!parsed)
            continue;
        if (parsed->tag != m_tag)
            continue;

        // Kick off a best-effort reverse-DNS lookup. We emit the result
        // with whatever name comes back; the per-host PTR is rarely
        // worth waiting on synchronously, so we fan out and emit on the
        // lookup callback. Capture the parsed pong by value so the
        // callback owns its own copy.
        const PongMessage pong = *parsed;
        const QHostAddress src = sender;
        QPointer<Scanner> self(this);
        QHostInfo::lookupHost(sender.toString(), this,
            [self, pong, src](const QHostInfo &info) {
                if (!self)
                    return;
                QString name;
                if (info.error() == QHostInfo::NoError && !info.hostName().isEmpty()
                    && info.hostName() != src.toString()) {
                    name = info.hostName();
                }
                emit self->pongReceived(pong, src, name);
            });
    }
}

void Scanner::onWaitElapsed()
{
    // End-of-scan: drop sockets/timers and signal completion. The
    // pending DNS callbacks above still resolve after this — their
    // QPointer guard handles the case where the caller has destroyed
    // the scanner in response to finished().
    if (m_waitTimer) {
        m_waitTimer->deleteLater();
        m_waitTimer = nullptr;
    }
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_pending.clear();
    m_nextIndex = 0;
    emit finished();
}

} // namespace qumesh::rmcp
