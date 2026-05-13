// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

namespace meshcommander::redir {
class RedirectionClient;
}

namespace meshcommander::ider {
class IderSession;
}

namespace meshcommander::app {

/// QML-facing controller for one IDE-R session. Owns the redirection
/// client and the IDE-R session driver. The QML layer sets `host`,
/// `port`, `user`, `password`, `isoPath`, and `startOption`, then calls
/// `open()`.
class IderController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(QString isoPath READ isoPath WRITE setIsoPath NOTIFY isoPathChanged)
    Q_PROPERTY(StartOption startOption READ startOption WRITE setStartOption NOTIFY startOptionChanged)

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(quint64 bytesSentToAmt READ bytesSentToAmt NOTIFY statsChanged)
    Q_PROPERTY(quint64 bytesReceivedFromAmt READ bytesReceivedFromAmt NOTIFY statsChanged)
    Q_PROPERTY(bool deviceEnabled READ deviceEnabled NOTIFY deviceEnabledChanged)

public:
    enum class State {
        Disconnected,
        Connecting,
        Authenticating,
        Opening,
        Running,
        Failed,
    };
    Q_ENUM(State)

    enum class StartOption {
        OnReboot   = 0,
        Graceful   = 1,
        Immediate  = 2,
    };
    Q_ENUM(StartOption)

    explicit IderController(QObject *parent = nullptr);
    ~IderController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] quint16 port() const { return m_port; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] QString isoPath() const { return m_isoPath; }
    [[nodiscard]] StartOption startOption() const { return m_startOption; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] quint64 bytesSentToAmt() const { return m_bytesSentToAmt; }
    [[nodiscard]] quint64 bytesReceivedFromAmt() const { return m_bytesReceivedFromAmt; }
    [[nodiscard]] bool deviceEnabled() const { return m_deviceEnabled; }

    void setHost(const QString &v);
    void setPort(quint16 v);
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setIsoPath(const QString &v);
    void setStartOption(StartOption v);

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();

signals:
    void hostChanged();
    void portChanged();
    void userChanged();
    void passwordChanged();
    void isoPathChanged();
    void startOptionChanged();
    void stateChanged();
    void lastErrorChanged();
    void statsChanged();
    void deviceEnabledChanged();

private:
    void setState(State s);
    void setLastError(const QString &e);
    void teardown();

    QString m_host;
    quint16 m_port = 16994;
    QString m_user;
    QString m_password;
    QString m_isoPath;
    StartOption m_startOption = StartOption::Graceful;

    State m_state = State::Disconnected;
    QString m_lastError;
    quint64 m_bytesSentToAmt = 0;
    quint64 m_bytesReceivedFromAmt = 0;
    bool m_deviceEnabled = false;

    QPointer<meshcommander::redir::RedirectionClient> m_client;
    QPointer<meshcommander::ider::IderSession> m_session;
};

} // namespace meshcommander::app
