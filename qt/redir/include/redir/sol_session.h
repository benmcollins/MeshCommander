// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QTimer;

namespace meshcommander::redir {

class RedirectionClient;

/// Drives a Serial-over-LAN session on top of an authenticated
/// `RedirectionClient`. After the client emits `authenticated()`, calling
/// `start()` sends the SOL Open / Settings sequence and from then on:
///
///  - inbound `0x2A IncomingDisplayData` frames surface as `data(QByteArray)`;
///  - `sendInput()` packages bytes into `0x28 SerialDataToHost` frames;
///  - a 2-second `QTimer` emits `0x2B` keepalives;
///  - protocol errors close the session via `closed(reason)`.
///
/// The session does NOT parse VT100 / ANSI; consumers (typically the QML
/// terminal widget) get raw bytes and do their own interpretation.
class SolSession : public QObject
{
    Q_OBJECT
public:
    explicit SolSession(RedirectionClient *client, QObject *parent = nullptr);
    ~SolSession() override;

    /// Send the SOL Open frame. Only valid after the underlying client
    /// has reached `Authenticated`. Subsequent frames are dispatched as
    /// the protocol progresses.
    void start();

    /// Queue user-typed bytes for the AMT serial console.
    void sendInput(const QByteArray &input);

    [[nodiscard]] bool isOpen() const { return m_open; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

signals:
    void sessionOpened();
    void data(const QByteArray &bytes);
    void closed(const QString &reason);

private:
    void onClientFailed(const QString &reason);
    void onRawBytes(const QByteArray &chunk);
    void drainInbox();
    void sendKeepalive();
    void fail(QString reason);

    RedirectionClient *m_client;
    QTimer *m_keepalive = nullptr;
    QByteArray m_inbox;
    quint32 m_sequence = 1;
    bool m_open = false;
    bool m_settingsSent = false;
    QString m_lastError;
};

} // namespace meshcommander::redir
