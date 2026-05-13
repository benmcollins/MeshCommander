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

void RedirectionClient::setCredentials(QString user, QString pass)
{
    m_user = std::move(user);
    m_pass = std::move(pass);
}

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
    drainInbox();
}

void RedirectionClient::drainInbox()
{
    while (!m_inbox.isEmpty()) {
        const unsigned char tag = static_cast<unsigned char>(m_inbox.at(0));

        if (tag == 0x11) {
            StartSessionReply reply;
            int consumed = 0;
            if (!tryParseStartSessionReply(m_inbox, &reply, &consumed)) return;
            m_inbox.remove(0, consumed);
            m_startStatus = reply.status;
            m_oemData = reply.oemData;
            if (reply.status != StartSessionStatus::Success) {
                fail(describe(reply.status));
                return;
            }
            emit sessionOpened();
            setState(State::SessionOpened);

            if (!m_user.isEmpty()) {
                if (m_socket->write(buildAuthQuery()) < 0) {
                    fail(m_socket->errorString());
                    return;
                }
                setState(State::AuthQuerying);
            }
            continue;
        }

        if (tag == 0x14) {
            AuthReply reply;
            int consumed = 0;
            if (!tryParseAuthReply(m_inbox, &reply, &consumed)) return;
            m_inbox.remove(0, consumed);

            switch (m_state) {
            case State::AuthQuerying: {
                if (!reply.caps.hasDigestWithQop) {
                    fail(QStringLiteral("device does not offer DigestWithQop auth"));
                    return;
                }
                if (m_socket->write(buildDigestQuery(m_user, m_authUri)) < 0) {
                    fail(m_socket->errorString());
                    return;
                }
                setState(State::AuthChallenging);
                break;
            }
            case State::AuthChallenging: {
                if (reply.status != 1 || reply.realm.isEmpty() || reply.nonce.isEmpty()) {
                    fail(QStringLiteral("authentication rejected by device"));
                    return;
                }
                const QString qop = reply.qop.isEmpty() ? QStringLiteral("auth") : reply.qop;
                const QString nc = QStringLiteral("00000001");
                const QString cnonce = makeClientNonce();
                const QString response = computeDigest(m_user, reply.realm, m_pass,
                                                       reply.nonce, nc, cnonce, qop,
                                                       m_authUri);
                if (m_socket->write(buildDigestResponse(m_user, reply.realm, reply.nonce,
                                                       m_authUri, cnonce, nc, response,
                                                       qop)) < 0) {
                    fail(m_socket->errorString());
                    return;
                }
                setState(State::AuthResponding);
                break;
            }
            case State::AuthResponding: {
                if (reply.status != 0) {
                    fail(QStringLiteral("authentication failed (status %1)").arg(reply.status));
                    return;
                }
                setState(State::Authenticated);
                emit authenticated();
                break;
            }
            default:
                fail(QStringLiteral("unexpected 0x14 in state %1").arg(static_cast<int>(m_state)));
                return;
            }
            continue;
        }

        fail(QStringLiteral("unexpected frame 0x%1 in state %2")
                 .arg(tag, 2, 16, QLatin1Char('0'))
                 .arg(static_cast<int>(m_state)));
        return;
    }
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
