// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvm/kvm_codec.h"
#include "kvm/kvm_session.h"
#include "redir/redir_client.h"
#include "redir/redir_codec.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using namespace qumesh::kvm;
using qumesh::redir::RedirectionClient;

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

QByteArray be16(quint16 v)
{
    QByteArray b(2, '\0');
    b[0] = static_cast<char>((v >> 8) & 0xFF);
    b[1] = static_cast<char>(v & 0xFF);
    return b;
}

QByteArray be32(quint32 v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>((v >> 24) & 0xFF);
    b[1] = static_cast<char>((v >> 16) & 0xFF);
    b[2] = static_cast<char>((v >> 8) & 0xFF);
    b[3] = static_cast<char>(v & 0xFF);
    return b;
}

QByteArray le16(quint16 v)
{
    QByteArray b(2, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    return b;
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

/// Mock that walks the redirection auth, then plays a scripted KVM
/// session: ProtocolVersion → security types → security result →
/// ServerInit → one FramebufferUpdate with a single RAW rect.
class MockServer : public QObject
{
    Q_OBJECT
public:
    MockServer() : m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            m_socket = m_server->nextPendingConnection();
            connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
                handleData();
            });
        });
    }
    bool listen() { return m_server->listen(QHostAddress::LocalHost); }
    quint16 port() const { return m_server->serverPort(); }

    QByteArray received; // bytes received after auth completes
    QPointer<QTcpSocket> m_socket;

private:
    void handleData()
    {
        if (!m_socket) return;
        m_inbox.append(m_socket->readAll());

        for (;;) {
            if (m_inbox.isEmpty()) return;
            const quint8 tag = static_cast<unsigned char>(m_inbox.at(0));

            if (m_kvmActive) {
                received.append(m_inbox);
                m_inbox.clear();
                return;
            }

            if (tag == 0x10) {
                if (m_inbox.size() < 8) return;
                m_inbox.remove(0, 8);
                QByteArray reply(13, '\0');
                reply[0] = 0x11;
                m_socket->write(reply);
            } else if (tag == 0x13) {
                if (m_inbox.size() < 9) return;
                const int len = static_cast<unsigned char>(m_inbox.at(5))
                              | static_cast<unsigned char>(m_inbox.at(6)) << 8
                              | static_cast<unsigned char>(m_inbox.at(7)) << 16
                              | static_cast<unsigned char>(m_inbox.at(8)) << 24;
                if (m_inbox.size() < 9 + len) return;
                const quint8 authType = static_cast<unsigned char>(m_inbox.at(4));
                m_inbox.remove(0, 9 + len);

                if (authType == 0) {
                    QByteArray caps; caps.append(char(4));
                    m_socket->write(frame14(0, 0, caps));
                } else if (m_authStage == 0) {
                    QByteArray body;
                    const QByteArray realm = "Digest:Intel(R) AMT";
                    const QByteArray nonce = "nnn";
                    const QByteArray qop = "auth";
                    body.append(char(realm.size())); body.append(realm);
                    body.append(char(nonce.size())); body.append(nonce);
                    body.append(char(qop.size())); body.append(qop);
                    m_socket->write(frame14(1, 4, body));
                    m_authStage = 1;
                } else {
                    m_socket->write(frame14(0, 4, {}));
                    // Auth complete — switch to KVM-script mode and
                    // push the RFB ProtocolVersion banner.
                    m_kvmActive = true;
                    m_socket->write(QByteArrayLiteral("RFB 003.008\n"));
                }
            } else {
                received.append(m_inbox);
                m_inbox.clear();
                return;
            }
        }
    }

    QByteArray m_inbox;
    int m_authStage = 0;
    bool m_kvmActive = false;
    QTcpServer *m_server;
};

QByteArray makeServerInit(quint16 w, quint16 h, const QByteArray &name)
{
    QByteArray b;
    b.append(be16(w));
    b.append(be16(h));
    b.append(QByteArray(16, '\0'));   // pixel format
    b.append(be32(static_cast<quint32>(name.size())));
    b.append(name);
    return b;
}

QByteArray makeRawRectUpdate(quint16 x, quint16 y, quint16 w, quint16 h,
                              quint16 color)
{
    QByteArray b;
    b.append(char(MsgFramebufferUpdate));
    b.append(char(0x00));
    b.append(be16(1));            // one rect
    b.append(be16(x));
    b.append(be16(y));
    b.append(be16(w));
    b.append(be16(h));
    b.append(be32(static_cast<quint32>(EncRaw)));
    for (int i = 0; i < w * h; ++i) b.append(le16(color));
    return b;
}

} // namespace

class TestKvmSession : public QObject
{
    Q_OBJECT
private slots:
    void handshakeThroughFrameLoopWithRawTile();
};

void TestKvmSession::handshakeThroughFrameLoopWithRawTile()
{
    MockServer server;
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(qumesh::redir::Protocol::Kvm);
    client.setCredentials(QStringLiteral("admin"), QStringLiteral("p"));

    KvmSession session(&client);
    QSignalSpy resizedSpy(&session, &KvmSession::desktopResized);
    QSignalSpy tileSpy(&session, &KvmSession::tileUpdated);

    QObject::connect(&client, &RedirectionClient::authenticated,
                     &session, &KvmSession::start);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() {
        return client.state() == RedirectionClient::State::Authenticated;
    }));

    // The session should now be at Version, waiting for the banner. The
    // mock pushed it on auth completion — wait until the session has
    // written "RFB 003.008\n" back.
    QVERIFY(waitFor(2000, [&]() { return server.received.startsWith("RFB 003.008\n"); }));
    server.received.clear();

    // Send security types: count=1 + None(1).
    QByteArray sec(2, '\0');
    sec[0] = 0x01;
    sec[1] = 0x01;
    server.m_socket->write(sec);

    // Wait for the security choice (1 byte = 0x01).
    QVERIFY(waitFor(2000, [&]() {
        return server.received.size() >= 1
            && static_cast<unsigned char>(server.received.at(0)) == 0x01;
    }));
    server.received.clear();

    // Security result = 0 (success).
    server.m_socket->write(be32(0));

    // Wait for ClientInit (1 byte = 0x01).
    QVERIFY(waitFor(2000, [&]() {
        return server.received.size() >= 1
            && static_cast<unsigned char>(server.received.at(0)) == 0x01;
    }));
    server.received.clear();

    // ServerInit: 320x240 desktop named "AMT".
    server.m_socket->write(makeServerInit(320, 240, "AMT"));

    QVERIFY(waitFor(2000, [&]() { return resizedSpy.count() == 1; }));
    QCOMPARE(resizedSpy.first().at(0).toInt(), 320);
    QCOMPARE(resizedSpy.first().at(1).toInt(), 240);

    // Wait for SetEncodings + FramebufferUpdateRequest to arrive.
    QVERIFY(waitFor(2000, [&]() { return server.received.size() >= 14; }));
    QCOMPARE(static_cast<unsigned char>(server.received.at(0)), quint8(MsgSetEncodings));

    // Send a single 4x4 red rect.
    server.m_socket->write(makeRawRectUpdate(0, 0, 4, 4, 0xF800));

    QVERIFY(waitFor(2000, [&]() { return tileSpy.count() == 1; }));
    const auto args = tileSpy.first();
    QCOMPARE(args.at(0).toInt(), 0);
    QCOMPARE(args.at(1).toInt(), 0);
    const QImage img = args.at(2).value<QImage>();
    QCOMPARE(img.width(), 4);
    QCOMPARE(img.height(), 4);
    QCOMPARE(img.pixel(2, 2), QRgb(0xFFF80000));
}

QTEST_MAIN(TestKvmSession)
#include "test_kvm_session.moc"
