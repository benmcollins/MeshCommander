// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QTimer;

namespace qumesh::wsman {
class WsmanClient;
}

namespace qumesh::app {

/// Polls `CIM_AssociatedPowerManagementService.PowerState` via WSMAN
/// on a fixed interval. Owns a `WsmanClient`; the caller sets host /
/// port / credentials and calls `start()`.
///
/// The CIM enum is mapped to the small set the UI cares about: On /
/// Off / Standby / Hibernate / Unknown / Unreachable. AMT's WSMAN
/// returns an integer `PowerState` per CIM_PowerState mapping (2=On,
/// 3=Sleep light, 4=Sleep deep, 6=Off-soft, 7=Hibernate, 8=Off-hard,
/// 9=Power cycle, 13=Sleep, 14=Standby).
class PowerStatePoller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged)

public:
    enum class State : quint8 {
        Unknown = 0,
        On,
        Off,
        Standby,
        Hibernate,
        Unreachable,
    };
    Q_ENUM(State)

    explicit PowerStatePoller(QObject *parent = nullptr);
    ~PowerStatePoller() override;

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] int intervalMs() const { return m_intervalMs; }

    void setHost(const QString &host);
    void setPort(quint16 port);
    void setTls(bool tls);
    void setCredentials(const QString &user, const QString &pass);
    void setIntervalMs(int ms);

    /// Begin polling. Issues an immediate request, then continues at
    /// the configured interval. Safe to call repeatedly; reconfigures
    /// the underlying client and restarts the timer.
    Q_INVOKABLE void start();

    /// Stop polling and reset state to Unknown.
    Q_INVOKABLE void stop();

signals:
    void stateChanged(State state);
    void lastErrorChanged();
    void intervalMsChanged();

private:
    void rebuildEndpoint();
    void poll();
    void applyCimState(int cimPowerState);
    void setState(State s);
    void setLastError(const QString &e);

    qumesh::wsman::WsmanClient *m_client;
    QTimer *m_timer;
    QString m_host;
    quint16 m_port = 16992;
    bool m_tls = false;
    QString m_user;
    QString m_pass;
    int m_intervalMs = 10000;
    State m_state = State::Unknown;
    QString m_lastError;
};

} // namespace qumesh::app
