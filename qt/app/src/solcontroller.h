// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "terminal/terminalscreen.h"

namespace meshcommander::redir {
class RedirectionClient;
class SolSession;
} // namespace meshcommander::redir

namespace meshcommander::app {

/// QML-facing controller for one SOL session. Owns the redirection
/// client, the SOL session driver, and the terminal screen the QML
/// layer renders. Created per SOL window — the host pane instantiates
/// it, sets `host`/`port`/`user`/`password`, and calls `open()`.
class SolController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(meshcommander::terminal::TerminalScreen *screen READ screen CONSTANT)

public:
    enum class State {
        Disconnected,
        Connecting,
        Authenticating,
        Opening,
        Connected,
        Failed,
    };
    Q_ENUM(State)

    explicit SolController(QObject *parent = nullptr);
    ~SolController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] quint16 port() const { return m_port; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] meshcommander::terminal::TerminalScreen *screen() const { return m_screen; }

    void setHost(const QString &v);
    void setPort(quint16 v);
    void setUser(const QString &v);
    void setPassword(const QString &v);

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void sendBytes(const QByteArray &bytes);

signals:
    void hostChanged();
    void portChanged();
    void userChanged();
    void passwordChanged();
    void stateChanged();
    void lastErrorChanged();

private:
    void setState(State s);
    void setLastError(const QString &e);
    void teardown();

    QString m_host;
    quint16 m_port = 16994;
    QString m_user;
    QString m_password;
    State m_state = State::Disconnected;
    QString m_lastError;

    meshcommander::terminal::TerminalScreen *m_screen;
    QPointer<meshcommander::redir::RedirectionClient> m_client;
    QPointer<meshcommander::redir::SolSession> m_session;
};

} // namespace meshcommander::app
