#pragma once

#include "redir/redir_codec.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;

namespace meshcommander::redir {

/// Plaintext (port 16994) transport handshake for AMT Redirection. TLS
/// (port 16995) and authentication are not implemented yet — this class is
/// the foundation for those follow-ups.
///
/// Lifecycle:
/// - construct, set protocol, call `connectTo(host, port)`.
/// - state transitions through `stateChanged` signals:
///   Disconnected → Connecting → SelectorSent → SessionOpened (or Failed).
/// - on `SessionOpened` the device is ready for the next phase of the
///   protocol (the auth/query frame `0x13`). This class does not send it.
class RedirectionClient : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Disconnected,
        Connecting,
        SelectorSent,
        SessionOpened,
        Failed,
    };
    Q_ENUM(State)

    explicit RedirectionClient(QObject *parent = nullptr);
    ~RedirectionClient() override;

    void setProtocol(Protocol p) { m_protocol = p; }
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
    void failed(const QString &error);

private:
    void handleConnected();
    void handleReadyRead();
    void handleSocketError();
    void setState(State s);
    void fail(QString error);

    Protocol m_protocol = Protocol::Sol;
    QTcpSocket *m_socket = nullptr;
    QByteArray m_inbox;
    State m_state = State::Disconnected;
    QString m_lastError;
    StartSessionStatus m_startStatus = StartSessionStatus::UnknownError;
    QByteArray m_oemData;
};

} // namespace meshcommander::redir
