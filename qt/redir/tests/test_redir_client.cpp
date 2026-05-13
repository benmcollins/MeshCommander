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

QTEST_GUILESS_MAIN(TestRedirClient)
#include "test_redir_client.moc"
