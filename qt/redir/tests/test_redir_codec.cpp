#include "redir/redir_codec.h"

#include <QRegularExpression>
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

    void authQueryBytes();
    void digestQueryHasUserAndUriOnly();
    void digestResponseHasAllEightFields();
    void parseAuthQueryReplyBitmap();
    void parseAuthChallenge();
    void parseAuthSuccess();
    void computeDigestKnownVector();
    void makeClientNonceIs32HexChars();
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

void TestRedirCodec::authQueryBytes()
{
    QCOMPARE(buildAuthQuery().toHex(), QByteArray("130000000000000000"));
}

void TestRedirCodec::digestQueryHasUserAndUriOnly()
{
    const QByteArray b = buildDigestQuery(QStringLiteral("admin"),
                                          QStringLiteral("/RedirectionService"));
    // 0x13 + 3 reserved + authType=4 + LE32 length + body
    QCOMPARE(static_cast<unsigned char>(b.at(0)), 0x13);
    QCOMPARE(static_cast<unsigned char>(b.at(4)), 0x04);

    // Body length should be 8 length-prefix bytes + len("admin") + len("/RedirectionService")
    const int expectedBody = 8 + 5 + 19;
    const quint32 len = static_cast<unsigned char>(b.at(5))
                        | static_cast<unsigned char>(b.at(6)) << 8
                        | static_cast<unsigned char>(b.at(7)) << 16
                        | static_cast<unsigned char>(b.at(8)) << 24;
    QCOMPARE(static_cast<int>(len), expectedBody);

    // First field is user with length prefix.
    QCOMPARE(static_cast<int>(static_cast<unsigned char>(b.at(9))), 5);
    QCOMPARE(b.mid(10, 5), QByteArray("admin"));
    // Realm (empty) — length byte = 0.
    QCOMPARE(static_cast<int>(static_cast<unsigned char>(b.at(15))), 0);
    // Nonce (empty).
    QCOMPARE(static_cast<int>(static_cast<unsigned char>(b.at(16))), 0);
    // URI.
    QCOMPARE(static_cast<int>(static_cast<unsigned char>(b.at(17))), 19);
    QCOMPARE(b.mid(18, 19), QByteArray("/RedirectionService"));
}

void TestRedirCodec::digestResponseHasAllEightFields()
{
    const QByteArray b = buildDigestResponse(
        QStringLiteral("u"), QStringLiteral("Digest:Intel(R) AMT"),
        QStringLiteral("nnn"), QStringLiteral("/RedirectionService"),
        QStringLiteral("cccc"), QStringLiteral("00000001"),
        QStringLiteral("ddddd"), QStringLiteral("auth"));
    // Walk the body and count length-prefixed fields.
    int p = 9;
    int fields = 0;
    while (p < b.size()) {
        const int n = static_cast<unsigned char>(b.at(p));
        p += 1 + n;
        ++fields;
    }
    QCOMPARE(fields, 8);
    QCOMPARE(p, b.size());
}

void TestRedirCodec::parseAuthQueryReplyBitmap()
{
    // 0x14, status=0, reserved×2, authType=0, len=2, body=[3, 4]
    QByteArray buf;
    buf.append(QByteArray::fromHex("1400000000020000000304"));
    AuthReply reply;
    int consumed = 0;
    QVERIFY(tryParseAuthReply(buf, &reply, &consumed));
    QCOMPARE(consumed, buf.size());
    QCOMPARE(static_cast<int>(reply.authType), 0);
    QVERIFY(reply.caps.hasDigestNoQop);
    QVERIFY(reply.caps.hasDigestWithQop);
    QVERIFY(!reply.caps.hasKerberos);
}

void TestRedirCodec::parseAuthChallenge()
{
    // 0x14, status=1, reserved×2, authType=4, len = body_len
    // body: <5>realm <4>nonc <4>auth
    QByteArray body;
    body.append(char(5)); body.append("realm");
    body.append(char(4)); body.append("nonc");
    body.append(char(4)); body.append("auth");

    QByteArray buf;
    buf.append(char(0x14));
    buf.append(char(0x01));         // status=1
    buf.append(char(0x00));
    buf.append(char(0x00));
    buf.append(char(0x04));         // authType=4
    buf.append(char(body.size() & 0xFF));
    buf.append(char(0)); buf.append(char(0)); buf.append(char(0));
    buf.append(body);

    AuthReply reply;
    int consumed = 0;
    QVERIFY(tryParseAuthReply(buf, &reply, &consumed));
    QCOMPARE(reply.status, quint8(1));
    QCOMPARE(reply.realm, QStringLiteral("realm"));
    QCOMPARE(reply.nonce, QStringLiteral("nonc"));
    QCOMPARE(reply.qop, QStringLiteral("auth"));
}

void TestRedirCodec::parseAuthSuccess()
{
    // 0x14, status=0, authType=4, len=0
    QByteArray buf = QByteArray::fromHex("140000000400000000");
    AuthReply reply;
    int consumed = 0;
    QVERIFY(tryParseAuthReply(buf, &reply, &consumed));
    QCOMPARE(reply.status, quint8(0));
}

void TestRedirCodec::computeDigestKnownVector()
{
    // Reference vector from RFC 2617 §3.5 (adapted to our MD5 path).
    //   user      = Mufasa
    //   realm     = testrealm@host.com
    //   pass      = Circle Of Life
    //   nonce     = dcd98b7102dd2f0e8b11d0f600bfb0c093
    //   nc        = 00000001
    //   cnonce    = 0a4f113b
    //   qop       = auth
    //   authUri   = /dir/index.html      (we use that instead of /RedirectionService)
    //
    // RFC's worked example yields:
    //
    // We always send POST (AMT only accepts POST on /RedirectionService),
    // so we cross-check against the POST variant of the worked example via:
    //   HA1 = MD5("Mufasa:testrealm@host.com:Circle Of Life")
    //       = 939e7578ed9e3c518a452acee763bce9
    //   HA2 = MD5("POST:/dir/index.html")
    //       = c0b64819c3e244af0be89086df86e3fa
    //   response = MD5(HA1:nonce:nc:cnonce:qop:HA2)
    //            = 440c5a7b9ed304fecd2ddd39c9c7b726
    const QString r = computeDigest(
        QStringLiteral("Mufasa"),
        QStringLiteral("testrealm@host.com"),
        QStringLiteral("Circle Of Life"),
        QStringLiteral("dcd98b7102dd2f0e8b11d0f600bfb0c093"),
        QStringLiteral("00000001"),
        QStringLiteral("0a4f113b"),
        QStringLiteral("auth"),
        QStringLiteral("/dir/index.html"));
    QCOMPARE(r, QStringLiteral("440c5a7b9ed304fecd2ddd39c9c7b726"));
}

void TestRedirCodec::makeClientNonceIs32HexChars()
{
    const QString a = makeClientNonce();
    const QString b = makeClientNonce();
    QCOMPARE(a.size(), 32);
    QCOMPARE(b.size(), 32);
    QVERIFY(a != b); // overwhelmingly likely to differ — flakes only at 2^-128
    QRegularExpression hex("^[0-9a-f]{32}$");
    QVERIFY(hex.match(a).hasMatch());
}

QTEST_GUILESS_MAIN(TestRedirCodec)
#include "test_redir_codec.moc"
