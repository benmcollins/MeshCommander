// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ider/ider_codec.h"
#include "ider/ider_session.h"
#include "ider/scsi.h"
#include "redir/redir_client.h"
#include "redir/redir_codec.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QtTest>

using namespace qumesh::ider;
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

QByteArray pack32Le(quint32 v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    return b;
}

QByteArray buildOpenSessionReply(quint32 sequence, quint16 readBuf, quint16 writeBuf)
{
    QByteArray b(30, '\0');
    b[0] = char(CmdOpenSessionReply);
    QByteArray seq = pack32Le(sequence);
    for (int i = 0; i < 4; ++i) b[4 + i] = seq.at(i);
    b[8] = 0x01;
    b[9] = 0x00;
    b[16] = static_cast<char>(readBuf & 0xFF);
    b[17] = static_cast<char>((readBuf >> 8) & 0xFF);
    b[18] = static_cast<char>(writeBuf & 0xFF);
    b[19] = static_cast<char>((writeBuf >> 8) & 0xFF);
    b[21] = 0x00; // proto = 0
    b[29] = 0x00; // no OEM data
    return b;
}

QByteArray buildScsiCommand(quint32 sequence, quint8 deviceFlags, quint8 featureRegister,
                             const QByteArray &cdb12)
{
    QByteArray b(28, '\0');
    b[0] = char(CmdCommandWritten);
    QByteArray seq = pack32Le(sequence);
    for (int i = 0; i < 4; ++i) b[4 + i] = seq.at(i);
    b[9]  = static_cast<char>(featureRegister);
    b[14] = static_cast<char>(deviceFlags);
    for (int i = 0; i < cdb12.size() && i < 12; ++i) b[16 + i] = cdb12.at(i);
    return b;
}

/// Mock that walks the redirection auth + serves as an IDE-R peer.
/// The test script pokes the server to send specific IDE-R frames; the
/// session under test is expected to respond on schedule.
class MockAmtServer : public QObject
{
    Q_OBJECT
public:
    MockAmtServer() : m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *s = m_server->nextPendingConnection();
            m_client = s;
            connect(s, &QTcpSocket::readyRead, s, [this, s]() { handleData(s); });
            connect(s, &QTcpSocket::disconnected, s, &QTcpSocket::deleteLater);
        });
    }
    bool listen() { return m_server->listen(QHostAddress::LocalHost); }
    quint16 port() const { return m_server->serverPort(); }

    void send(const QByteArray &b) { if (m_client) m_client->write(b); }

    QByteArray received;
    QPointer<QTcpSocket> m_client;

private:
    void handleData(QTcpSocket *s)
    {
        m_inbox.append(s->readAll());
        for (;;) {
            if (m_inbox.isEmpty()) return;
            const quint8 tag = static_cast<unsigned char>(m_inbox.at(0));

            if (tag == 0x10) {
                if (m_inbox.size() < 8) return;
                m_inbox.remove(0, 8);
                QByteArray reply(13, '\0');
                reply[0] = 0x11;
                s->write(reply);
            } else if (tag == 0x13) {
                if (m_inbox.size() < 9) return;
                const int len = static_cast<unsigned char>(m_inbox.at(5))
                              | static_cast<unsigned char>(m_inbox.at(6)) << 8
                              | static_cast<unsigned char>(m_inbox.at(7)) << 16
                              | static_cast<unsigned char>(m_inbox.at(8)) << 24;
                if (m_inbox.size() < 9 + len) return;
                const unsigned char authType = static_cast<unsigned char>(m_inbox.at(4));
                m_inbox.remove(0, 9 + len);
                if (authType == 0) {
                    QByteArray caps;
                    caps.append(char(4));
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
            } else {
                // Everything else is IDE-R traffic from the session
                // under test — append for the test to inspect.
                received.append(m_inbox);
                m_inbox.clear();
                return;
            }
        }
    }

    QByteArray m_inbox;
    int m_authStage = 0;
    QTcpServer *m_server;
};

} // namespace

class TestIderSession : public QObject
{
    Q_OBJECT
private slots:
    void readScsiCdServesIsoBytes();
    void initCleanup() { /* default */ }
};

void TestIderSession::readScsiCdServesIsoBytes()
{
    // Build a synthetic "ISO": 4 sectors × 2048 bytes, each sector a
    // distinct byte pattern so we can verify the slice that comes back.
    QTemporaryFile iso;
    QVERIFY(iso.open());
    for (int sector = 0; sector < 4; ++sector) {
        QByteArray block(2048, static_cast<char>('A' + sector));
        QCOMPARE(iso.write(block), qint64(block.size()));
    }
    iso.flush();
    const QString isoPath = iso.fileName();

    MockAmtServer server;
    QVERIFY(server.listen());

    RedirectionClient client;
    client.setProtocol(qumesh::redir::Protocol::Ider);
    client.setCredentials(QStringLiteral("admin"), QStringLiteral("p"));

    IderSession session(&client);
    session.setIsoPath(isoPath);
    QSignalSpy openedSpy(&session, &IderSession::sessionOpened);

    QObject::connect(&client, &RedirectionClient::authenticated,
                     &session, &IderSession::start);
    client.connectTo(QStringLiteral("127.0.0.1"), server.port());

    QVERIFY(waitFor(5000, [&]() {
        return client.state() == RedirectionClient::State::Authenticated;
    }));

    // The session should have written the 20-byte OpenSession frame.
    QVERIFY(waitFor(2000, [&]() {
        return !server.received.isEmpty()
            && static_cast<unsigned char>(server.received.at(0)) == 0x40;
    }));
    QCOMPARE(static_cast<unsigned char>(server.received.at(0)), quint8(CmdOpenSession));
    server.received.clear();

    // Reply with OpenSessionReply: 2KB read buffer (so READ_10 of 4
    // sectors will be split into 4 individual chunks).
    server.send(buildOpenSessionReply(0, 2048, 8192));
    QVERIFY(waitFor(2000, [&]() { return openedSpy.count() == 1; }));

    // Drain the EnableFeatures frame the session sends after open. Wait
    // until at least the full frame (8 hdr + 1 type + 4 payload = 13)
    // has landed at the server before clearing.
    QVERIFY(waitFor(2000, [&]() {
        return !server.received.isEmpty()
            && static_cast<unsigned char>(server.received.at(0)) == 0x48
            && server.received.size() >= 13;
    }));
    server.received.clear();

    // Build a SCSI READ_10 for 4 sectors starting at LBA 0 (full file).
    QByteArray cdb(12, '\0');
    cdb[0] = char(scsi::kRead10);
    cdb[2] = 0; cdb[3] = 0; cdb[4] = 0; cdb[5] = 0; // LBA = 0
    cdb[7] = 0; cdb[8] = 4;                           // 4 blocks (BE)
    // device_flags 0x10 → CD-ROM; feature register = 0 (PIO)
    server.send(buildScsiCommand(1, 0x10, 0, cdb));

    // The session should reply with 4 × SendDataToHost frames carrying
    // the four 2048-byte sectors of our synthetic ISO.
    QVERIFY(waitFor(5000, [&]() { return server.received.size() >= 4 * (8 + 26 + 2048); }));

    QByteArray collected;
    int off = 0;
    for (int i = 0; i < 4; ++i) {
        QVERIFY(off + 34 + 2048 <= server.received.size());
        QCOMPARE(static_cast<unsigned char>(server.received.at(off)),
                 quint8(CmdSendDataToHost));
        // Payload starts at offset off+8 (header) + 26 (body header).
        collected.append(server.received.mid(off + 8 + 26, 2048));
        off += 8 + 26 + 2048;
    }
    QCOMPARE(collected.size(), 4 * 2048);
    QCOMPARE(collected.left(2048), QByteArray(2048, 'A'));
    QCOMPARE(collected.mid(2048, 2048), QByteArray(2048, 'B'));
    QCOMPARE(collected.mid(4096, 2048), QByteArray(2048, 'C'));
    QCOMPARE(collected.mid(6144, 2048), QByteArray(2048, 'D'));
}

QTEST_MAIN(TestIderSession)
#include "test_ider_session.moc"
