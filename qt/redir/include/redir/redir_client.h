// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "redir/redir_codec.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;

namespace meshcommander::redir {

/// Plaintext (port 16994) transport with the full AMT digest auth
/// handshake. TLS (16995) lands in a separate issue.
///
/// State machine (after `connectTo()`):
///
///     Disconnected → Connecting → SelectorSent → SessionOpened
///         → AuthQuerying       (sent 0x13 authType=0)
///         → AuthChallenging    (sent 0x13 authType=4 with user+uri)
///         → AuthResponding     (sent 0x13 authType=4 with full digest)
///         → Authenticated      (server returned 0x14 status=0)
///         | Failed (any step)
///
/// The auth steps only run when `setCredentials()` was called with a
/// non-empty user. Otherwise the client stops at `SessionOpened` so
/// existing tests that don't care about auth keep working.
class RedirectionClient : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Disconnected,
        Connecting,
        SelectorSent,
        SessionOpened,
        AuthQuerying,
        AuthChallenging,
        AuthResponding,
        Authenticated,
        Failed,
    };
    Q_ENUM(State)

    explicit RedirectionClient(QObject *parent = nullptr);
    ~RedirectionClient() override;

    void setProtocol(Protocol p) { m_protocol = p; }
    void setCredentials(QString user, QString pass);
    void setAuthUri(QString uri) { m_authUri = std::move(uri); }

    [[nodiscard]] Protocol protocol() const { return m_protocol; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] StartSessionStatus startStatus() const { return m_startStatus; }
    [[nodiscard]] QByteArray oemData() const { return m_oemData; }

    void connectTo(const QString &host, quint16 port);
    void disconnectFromHost();

signals:
    void stateChanged(State state);
    void sessionOpened();
    void authenticated();
    void failed(const QString &error);

private:
    void handleConnected();
    void handleReadyRead();
    void handleSocketError();

    /// Pull frames out of `m_inbox`; called whenever new bytes arrive.
    void drainInbox();

    void setState(State s);
    void fail(QString error);

    Protocol m_protocol = Protocol::Sol;
    QString m_user;
    QString m_pass;
    QString m_authUri = QStringLiteral("/RedirectionService");
    QTcpSocket *m_socket = nullptr;
    QByteArray m_inbox;
    State m_state = State::Disconnected;
    QString m_lastError;
    StartSessionStatus m_startStatus = StartSessionStatus::UnknownError;
    QByteArray m_oemData;
};

} // namespace meshcommander::redir
