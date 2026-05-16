// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "solcontroller.h"
#include "terminal/terminalscreen.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using qumesh::app::SolController;
using qumesh::terminal::TerminalScreen;

namespace {

template <typename Pred>
bool waitFor(int ms, Pred p)
{
    QElapsedTimer t;
    t.start();
    while (!p()) {
        if (t.elapsed() > ms) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return true;
}

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

QByteArray solOpenReplyOk()
{
    QByteArray b(23, '\0');
    b[0] = 0x21;
    return b;
}

QByteArray solDisplay(const QByteArray &payload)
{
    QByteArray b;
    b.append(char(0x2A));
    b.append(7, '\0');
    b.append(char(payload.size() & 0xFF));
    b.append(char((payload.size() >> 8) & 0xFF));
    b.append(payload);
    return b;
}

/// Minimal AMT redirection mock: replays a successful SOL handshake
/// then emits a single display payload so the controller's data path
/// is exercised end-to-end.
class MockServer : public QObject
{
    Q_OBJECT
public:
    MockServer() : m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *s = m_server->nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s]() { handleData(s); });
        });
    }
    bool listen() { return m_server->listen(QHostAddress::LocalHost); }
    quint16 port() const { return m_server->serverPort(); }
    /// Bytes captured from `0x28 SerialDataToHost` payloads — the
    /// stream the host sees on the serial side. Tests inspect this to
    /// assert the controller's `stty`/`TERM` init landed.
    QByteArray serialInput() const { return m_serialIn; }

private:
    void handleData(QTcpSocket *s)
    {
        m_buf.append(s->readAll());
        for (;;) {
            if (m_buf.isEmpty()) return;
            const unsigned char tag = static_cast<unsigned char>(m_buf.at(0));
            if (tag == 0x10) {
                if (m_buf.size() < 8) return;
                m_buf.remove(0, 8);
                QByteArray reply(13, '\0');
                reply[0] = 0x11;
                s->write(reply);
            } else if (tag == 0x13) {
                if (m_buf.size() < 9) return;
                const int len = static_cast<unsigned char>(m_buf.at(5))
                              | static_cast<unsigned char>(m_buf.at(6)) << 8
                              | static_cast<unsigned char>(m_buf.at(7)) << 16
                              | static_cast<unsigned char>(m_buf.at(8)) << 24;
                if (m_buf.size() < 9 + len) return;
                const unsigned char authType = static_cast<unsigned char>(m_buf.at(4));
                m_buf.remove(0, 9 + len);
                if (authType == 0) {
                    QByteArray caps; caps.append(char(4));
                    s->write(frame14(0, 0, caps));
                } else if (m_authStage == 0) {
                    QByteArray body;
                    const QByteArray realm = "Digest:Intel(R) AMT";
                    const QByteArray nonce = "nnn";
                    const QByteArray qop = "auth";
                    body.append(char(realm.size())); body.append(realm);
                    body.append(char(nonce.size())); body.append(nonce);
                    body.append(char(qop.size())); body.append(qop);
                    s->write(frame14(1, 4, body));
                    m_authStage = 1;
                } else {
                    s->write(frame14(0, 4, {}));
                }
            } else if (tag == 0x20) {
                if (m_buf.size() < 24) return;
                m_buf.remove(0, 24);
                s->write(solOpenReplyOk());
            } else if (tag == 0x27) {
                if (m_buf.size() < 14) return;
                m_buf.remove(0, 14);
                s->write(solDisplay(QByteArrayLiteral("READY> ")));
            } else if (tag == 0x28) {
                if (m_buf.size() < 10) return;
                const quint16 n = static_cast<unsigned char>(m_buf.at(8))
                                | static_cast<unsigned char>(m_buf.at(9)) << 8;
                if (m_buf.size() < 10 + n) return;
                m_serialIn.append(m_buf.mid(10, n));
                m_buf.remove(0, 10 + n);
            } else if (tag == 0x2B) {
                if (m_buf.size() < 8) return;
                m_buf.remove(0, 8);
            } else {
                m_buf.clear();
                return;
            }
        }
    }

    QByteArray m_buf;
    QByteArray m_serialIn;
    int m_authStage = 0;
    QTcpServer *m_server;
};

} // namespace

class TestSolController : public QObject
{
    Q_OBJECT
private slots:
    void initialStateAndScreen();
    void openWithEmptyHostFails();
    void connectAndReceiveData();
    void respondsToWindowSizeQuery();
};

void TestSolController::initialStateAndScreen()
{
    SolController c;
    QCOMPARE(c.state(), SolController::State::Disconnected);
    QVERIFY(c.screen() != nullptr);
    QCOMPARE(c.screen()->rows(), 24);
    QCOMPARE(c.screen()->columns(), 80);
}

void TestSolController::openWithEmptyHostFails()
{
    SolController c;
    QSignalSpy errSpy(&c, &SolController::lastErrorChanged);
    c.open();
    QCOMPARE(c.state(), SolController::State::Failed);
    QVERIFY(errSpy.count() >= 1);
    QVERIFY(!c.lastError().isEmpty());
}

void TestSolController::connectAndReceiveData()
{
    MockServer server;
    QVERIFY(server.listen());

    SolController c;
    c.setHost(QStringLiteral("127.0.0.1"));
    c.setPortForTest(server.port());
    c.setUser(QStringLiteral("admin"));
    c.setPassword(QStringLiteral("p"));
    c.open();

    QVERIFY(waitFor(5000, [&]() { return c.state() == SolController::State::Connected; }));
    QVERIFY(waitFor(2000, [&]() {
        return c.screen()->lineHtml(0).contains(QStringLiteral("READY"));
    }));

    c.close();
    QCOMPARE(c.state(), SolController::State::Disconnected);
}

void TestSolController::respondsToWindowSizeQuery()
{
    MockServer server;
    QVERIFY(server.listen());

    SolController c;
    c.setHost(QStringLiteral("127.0.0.1"));
    c.setPortForTest(server.port());
    c.setUser(QStringLiteral("admin"));
    c.setPassword(QStringLiteral("p"));
    c.screen()->resize(28, 108);
    c.open();

    QVERIFY(waitFor(5000, [&]() { return c.state() == SolController::State::Connected; }));
    // Feed the XTWINOPS query that `resize(1)` sends. The reply must
    // come back via the SOL data channel — that's how the remote
    // sees our actual grid size without us ever typing a command.
    c.screen()->feed(QByteArrayLiteral("\x1b[18t"));
    QVERIFY(waitFor(2000, [&]() {
        return server.serialInput().contains("\x1b[8;28;108t");
    }));

    c.close();
}

QTEST_MAIN(TestSolController)
#include "test_solcontroller.moc"
