// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "rmcp/pong.h"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>

class QTimer;
class QUdpSocket;

namespace qumesh::rmcp {

/// Asynchronously sweeps a list of IPv4 hosts with ASF presence pings on
/// UDP/623 and emits one signal per host that responds with a valid
/// Intel pong. Owns a single ephemeral UDP socket bound to 0.0.0.0; the
/// caller is responsible for keeping the `Scanner` alive until
/// `finished()` fires (or calling `stop()`).
///
/// Send-pacing: writes go out in small batches, then the scanner waits
/// for a quiet window before declaring the scan done. Replies arriving
/// after `finished()` are dropped.
class Scanner : public QObject
{
    Q_OBJECT
public:
    explicit Scanner(QObject *parent = nullptr);
    ~Scanner() override;

    /// Begin a scan. Cancels any in-flight scan first. Returns false on
    /// socket-bind failure (`errorString()` carries the reason).
    bool start(const QList<QHostAddress> &targets);

    /// Cancel an in-flight scan. Emits `finished()` synchronously.
    void stop();

    [[nodiscard]] QString errorString() const { return m_lastError; }

signals:
    /// One per validated pong. `reverseDns` is best-effort and may be
    /// empty — the result row should fall back to the IP string.
    void pongReceived(const qumesh::rmcp::PongMessage &pong,
                      const QHostAddress &source,
                      const QString &reverseDns);

    /// Fires once when every probe has been sent AND the post-send
    /// quiet window has elapsed (or `stop()` was called).
    void finished();

private slots:
    void onReadyRead();
    void onSendBurst();
    void onWaitElapsed();

private:
    void teardownTimers();
    void requestReverseDns(const QHostAddress &addr);

    QUdpSocket *m_socket = nullptr;
    QTimer *m_sendTimer = nullptr;
    QTimer *m_waitTimer = nullptr;
    QList<QHostAddress> m_pending;
    int m_nextIndex = 0;
    quint8 m_tag = 0;
    QString m_lastError;
};

} // namespace qumesh::rmcp
