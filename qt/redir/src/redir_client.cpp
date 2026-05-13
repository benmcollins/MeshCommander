#include "redir/redir_client.h"

#include <QTcpSocket>

namespace meshcommander::redir {

namespace {

QString describe(StartSessionStatus s)
{
    switch (s) {
    case StartSessionStatus::Success:
        return QStringLiteral("Success");
    case StartSessionStatus::Busy:
        return QStringLiteral("device is busy with another session");
    case StartSessionStatus::Unsupported:
        return QStringLiteral("device does not support this redirection protocol");
    case StartSessionStatus::UnknownError:
        return QStringLiteral("device reported an unknown error");
    }
    return QStringLiteral("unknown status %1").arg(static_cast<int>(s));
}

} // namespace

RedirectionClient::RedirectionClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &RedirectionClient::handleConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &RedirectionClient::handleReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &RedirectionClient::handleSocketError);
}

RedirectionClient::~RedirectionClient() = default;

void RedirectionClient::connectTo(const QString &host, quint16 port)
{
    m_inbox.clear();
    m_lastError.clear();
    m_startStatus = StartSessionStatus::UnknownError;
    m_oemData.clear();
    setState(State::Connecting);
    m_socket->connectToHost(host, port);
}

void RedirectionClient::disconnectFromHost()
{
    m_socket->disconnectFromHost();
    setState(State::Disconnected);
}

void RedirectionClient::handleConnected()
{
    const QByteArray selector = buildSelector(m_protocol);
    if (m_socket->write(selector) != selector.size()) {
        fail(QStringLiteral("could not write protocol selector: %1").arg(m_socket->errorString()));
        return;
    }
    setState(State::SelectorSent);
}

void RedirectionClient::handleReadyRead()
{
    m_inbox.append(m_socket->readAll());

    StartSessionReply reply;
    int consumed = 0;
    if (!tryParseStartSessionReply(m_inbox, &reply, &consumed)) {
        // not enough bytes yet, or not a 0x11 frame
        if (!m_inbox.isEmpty() && static_cast<unsigned char>(m_inbox.at(0)) != 0x11) {
            fail(QStringLiteral("unexpected first byte 0x%1 (waiting for 0x11)")
                     .arg(static_cast<unsigned char>(m_inbox.at(0)), 2, 16, QLatin1Char('0')));
        }
        return;
    }

    m_inbox.remove(0, consumed);
    m_startStatus = reply.status;
    m_oemData = reply.oemData;

    if (reply.status != StartSessionStatus::Success) {
        fail(describe(reply.status));
        return;
    }
    setState(State::SessionOpened);
    emit sessionOpened();
}

void RedirectionClient::handleSocketError()
{
    fail(m_socket->errorString());
}

void RedirectionClient::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

void RedirectionClient::fail(QString error)
{
    m_lastError = std::move(error);
    setState(State::Failed);
    emit failed(m_lastError);
}

} // namespace meshcommander::redir
