// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "redir/redir_client.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using namespace meshcommander::redir;

class TestRedirClient : public QObject
{
    Q_OBJECT
private slots:
    void connectAndOpenSession();
    void busyReplyFailsClient();
    void unexpectedFirstByteFails();
    void fullDigestHandshakeReachesAuthenticated();
    void wrongPasswordFails();
    void noSupportedAuthFails();
};

namespace {

class MockRedirServer : public QObject
{
    Q_OBJECT
public:
    MockRedirServer(QByteArray reply, QObject *parent = nullptr)
        : QObject(parent), m_reply(std::move(reply)), m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *s = m_server->nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                m_received.append(s->readAll());
                if (m_received.size() >= 8) {
                    s->write(m_reply);
                }
            });
            connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
        });
    }
    [[nodiscard]] bool listen() { return m_server->listen(QHostAddress::LocalHost); }
    [[nodiscard]] quint16 port() const { return m_server->serverPort(); }
    [[nodiscard]] QByteArray received() const { return m_received; }

private:
    QByteArray m_reply;
    QByteArray m_received;
    QTcpServer *m_server;
};

QByteArray successReply()
{
    QByteArray b(13, '\0');
    b[0] = 0x11;
    b[1] = 0x00; // Success
    return b;
}

QByteArray busyReply()
{
    QByteArray b(13, '\0');
    b[0] = 0x11;
    b[1] = 0x02;
    return b;
}

template <typename Predicate>
bool waitFor(int timeoutMs, Predicate p)
{
    QElapsedTimer t;
    t.start();
    while (!p()) {
        if (t.elapsed() > timeoutMs) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return true;
}

} // namespace

void TestRedirClient::connectAndOpenSession()
{
    MockRedirServer server(successReply());
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(Protocol::Sol);
    QSignalSpy openedSpy(&client, &RedirectionClient::sessionOpened);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return openedSpy.count() == 1; }));
    QCOMPARE(client.state(), RedirectionClient::State::SessionOpened);
    QCOMPARE(server.received().toHex(), QByteArray("10000000534f4c20"));
}

void TestRedirClient::busyReplyFailsClient()
{
    MockRedirServer server(busyReply());
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(Protocol::Sol);
    QSignalSpy failedSpy(&client, &RedirectionClient::failed);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return failedSpy.count() == 1; }));
    QCOMPARE(client.state(), RedirectionClient::State::Failed);
    QVERIFY(client.lastError().contains(QStringLiteral("busy")));
}

void TestRedirClient::unexpectedFirstByteFails()
{
    QByteArray garbage(13, '\0');
    garbage[0] = 0x42;
    MockRedirServer server(garbage);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(Protocol::Sol);
    QSignalSpy failedSpy(&client, &RedirectionClient::failed);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return failedSpy.count() == 1; }));
    QCOMPARE(client.state(), RedirectionClient::State::Failed);
}

namespace {

/// Scriptable mock that drives the full multi-step handshake. Subsequent
/// client writes get successive replies from `script`. After a write the
/// mock records it under `received[step]` so tests can inspect what the
/// client sent at each phase.
class ScriptedRedirServer : public QObject
{
    Q_OBJECT
public:
    explicit ScriptedRedirServer(QList<QByteArray> script, QObject *parent = nullptr)
        : QObject(parent), m_script(std::move(script)), m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *s = m_server->nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                const QByteArray chunk = s->readAll();
                m_received.append(chunk);
                if (m_step < m_script.size()) {
                    s->write(m_script.at(m_step++));
                }
            });
            connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
        });
    }
    [[nodiscard]] bool listen() { return m_server->listen(QHostAddress::LocalHost); }
    [[nodiscard]] quint16 port() const { return m_server->serverPort(); }
    [[nodiscard]] const QByteArray &received() const { return m_received; }

private:
    QList<QByteArray> m_script;
    QByteArray m_received;
    int m_step = 0;
    QTcpServer *m_server;
};

QByteArray frame14(quint8 status, quint8 authType, const QByteArray &body)
{
    QByteArray b;
    b.append(char(0x14));
    b.append(char(status));
    b.append(char(0x00));
    b.append(char(0x00));
    b.append(char(authType));
    const quint32 n = static_cast<quint32>(body.size());
    b.append(char(n & 0xFF));
    b.append(char((n >> 8) & 0xFF));
    b.append(char((n >> 16) & 0xFF));
    b.append(char((n >> 24) & 0xFF));
    b.append(body);
    return b;
}

QByteArray authCapsBody(QList<quint8> caps)
{
    QByteArray b;
    for (quint8 c : caps) b.append(char(c));
    return b;
}

QByteArray challengeBody(const QByteArray &realm, const QByteArray &nonce, const QByteArray &qop)
{
    QByteArray b;
    b.append(char(realm.size())); b.append(realm);
    b.append(char(nonce.size())); b.append(nonce);
    b.append(char(qop.size())); b.append(qop);
    return b;
}

} // namespace

void TestRedirClient::fullDigestHandshakeReachesAuthenticated()
{
    QList<QByteArray> script{
        successReply(),
        frame14(0, 0, authCapsBody({4})),                          // step 2: caps = DigestWithQop only
        frame14(1, 4, challengeBody("Digest:Intel(R) AMT",
                                    "abc123nonce", "auth")),       // step 4: challenge
        frame14(0, 4, {}),                                         // step 6: success
    };
    ScriptedRedirServer server(script);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(Protocol::Sol);
    client.setCredentials(QStringLiteral("admin"), QStringLiteral("Passw0rd!"));
    QSignalSpy authSpy(&client, &RedirectionClient::authenticated);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return authSpy.count() == 1; }));
    QCOMPARE(client.state(), RedirectionClient::State::Authenticated);
}

void TestRedirClient::wrongPasswordFails()
{
    QList<QByteArray> script{
        successReply(),
        frame14(0, 0, authCapsBody({4})),
        frame14(1, 4, challengeBody("Digest:Intel(R) AMT", "abc", "auth")),
        frame14(2, 4, {}),                                         // status != 0 means rejected
    };
    ScriptedRedirServer server(script);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(Protocol::Sol);
    client.setCredentials(QStringLiteral("admin"), QStringLiteral("wrong"));
    QSignalSpy failedSpy(&client, &RedirectionClient::failed);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return failedSpy.count() == 1; }));
    QCOMPARE(client.state(), RedirectionClient::State::Failed);
    QVERIFY(client.lastError().contains(QStringLiteral("authentication failed")));
}

void TestRedirClient::noSupportedAuthFails()
{
    QList<QByteArray> script{
        successReply(),
        frame14(0, 0, authCapsBody({2})),                          // only Kerberos
    };
    ScriptedRedirServer server(script);
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(Protocol::Sol);
    client.setCredentials(QStringLiteral("admin"), QStringLiteral("p"));
    QSignalSpy failedSpy(&client, &RedirectionClient::failed);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() { return failedSpy.count() == 1; }));
    QCOMPARE(client.state(), RedirectionClient::State::Failed);
    QVERIFY(client.lastError().contains(QStringLiteral("DigestWithQop")));
}

QTEST_GUILESS_MAIN(TestRedirClient)
#include "test_redir_client.moc"
