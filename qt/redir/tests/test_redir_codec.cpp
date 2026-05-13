#include "redir/redir_codec.h"

#include <QtTest>

using namespace meshcommander::redir;

class TestRedirCodec : public QObject
{
    Q_OBJECT
private slots:
    void solSelectorBytes();
    void kvmSelectorBytes();
    void iderSelectorBytes();
    void parseReplySuccessWithNoOem();
    void parseReplySuccessWithOem();
    void parseReplyBusy();
    void parseReplyNeedsMoreBytes();
    void parseReplyWrongFirstByte();
};

void TestRedirCodec::solSelectorBytes()
{
    const QByteArray b = buildSelector(Protocol::Sol);
    QCOMPARE(b.size(), 8);
    QCOMPARE(b.toHex(), QByteArray("10000000534f4c20"));
}

void TestRedirCodec::kvmSelectorBytes()
{
    const QByteArray b = buildSelector(Protocol::Kvm);
    QCOMPARE(b.toHex(), QByteArray("100100004b564d52"));
}

void TestRedirCodec::iderSelectorBytes()
{
    const QByteArray b = buildSelector(Protocol::Ider);
    QCOMPARE(b.toHex(), QByteArray("1000000049444552"));
}

void TestRedirCodec::parseReplySuccessWithNoOem()
{
    // 13-byte fixed header: 0x11 (cmd) 0x00 (status=Success) + 10 reserved + 0x00 (oemlen)
    QByteArray buf(13, '\0');
    buf[0] = 0x11;
    buf[1] = 0x00;
    buf[12] = 0x00;

    StartSessionReply reply;
    int consumed = 0;
    QVERIFY(tryParseStartSessionReply(buf, &reply, &consumed));
    QCOMPARE(consumed, 13);
    QCOMPARE(static_cast<int>(reply.status),
             static_cast<int>(StartSessionStatus::Success));
    QVERIFY(reply.oemData.isEmpty());
}

void TestRedirCodec::parseReplySuccessWithOem()
{
    QByteArray buf(13, '\0');
    buf[0] = 0x11;
    buf[1] = 0x00;
    buf[12] = 3;
    buf.append(QByteArrayLiteral("abc"));

    StartSessionReply reply;
    int consumed = 0;
    QVERIFY(tryParseStartSessionReply(buf, &reply, &consumed));
    QCOMPARE(consumed, 16);
    QCOMPARE(reply.oemData, QByteArray("abc"));
}

void TestRedirCodec::parseReplyBusy()
{
    QByteArray buf(13, '\0');
    buf[0] = 0x11;
    buf[1] = 0x02; // Busy
    StartSessionReply reply;
    int consumed = 0;
    QVERIFY(tryParseStartSessionReply(buf, &reply, &consumed));
    QCOMPARE(static_cast<int>(reply.status),
             static_cast<int>(StartSessionStatus::Busy));
}

void TestRedirCodec::parseReplyNeedsMoreBytes()
{
    // Only header byte present.
    QByteArray buf(1, '\x11');
    StartSessionReply reply;
    int consumed = 99;
    QVERIFY(!tryParseStartSessionReply(buf, &reply, &consumed));
    QCOMPARE(consumed, 0);
}

void TestRedirCodec::parseReplyWrongFirstByte()
{
    QByteArray buf(13, '\0');
    buf[0] = 0x20;
    StartSessionReply reply;
    int consumed = 99;
    QVERIFY(!tryParseStartSessionReply(buf, &reply, &consumed));
    QCOMPARE(consumed, 0);
}

QTEST_GUILESS_MAIN(TestRedirCodec)
#include "test_redir_codec.moc"
