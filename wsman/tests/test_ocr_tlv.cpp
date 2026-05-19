// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include <QtTest>

using namespace qumesh::wsman;

class TestOcrTlv : public QObject
{
    Q_OBJECT
private slots:
    void pbaTlvEmitsBootStringAndLength();
    void httpsPinnedTlvFiresSha256ByDefault();
    void httpsPinnedTlvDispatchesSha384And512();
    void httpsPinnedTlvSkipsHashWhenAlgEmpty();
    void httpsPinnedTlvIncludesServerCertHashAndCreds();
    void httpsPinnedTlvSkipsUnknownHashAlg();
};

namespace {

// Read a little-endian u16 / u32 out of the blob at `offset`. Keeps
// the assertions below tight — every TLV walker uses the same shape.
quint16 readU16Le(const QByteArray &b, int offset)
{
    return static_cast<quint16>(
        static_cast<quint8>(b.at(offset))
        | (static_cast<quint8>(b.at(offset + 1)) << 8));
}

quint32 readU32Le(const QByteArray &b, int offset)
{
    return static_cast<quint32>(
        static_cast<quint8>(b.at(offset))
        | (static_cast<quint8>(b.at(offset + 1)) << 8)
        | (static_cast<quint8>(b.at(offset + 2)) << 16)
        | (static_cast<quint8>(b.at(offset + 3)) << 24));
}

struct Tlv {
    int next;
    quint16 vendor;
    quint16 type;
    quint32 length;
    QByteArray value;
};

// Walk one TLV entry starting at `offset`. Returns the position after
// the entry plus the parsed fields.
Tlv walkOne(const QByteArray &blob, int offset)
{
    Tlv t;
    t.vendor = readU16Le(blob, offset);
    t.type = readU16Le(blob, offset + 2);
    t.length = readU32Le(blob, offset + 4);
    t.value = blob.mid(offset + 8, static_cast<int>(t.length));
    t.next = offset + 8 + static_cast<int>(t.length);
    return t;
}

} // namespace

void TestOcrTlv::pbaTlvEmitsBootStringAndLength()
{
    int count = 0;
    const QByteArray blob = buildOcrPbaBootTlv(QStringLiteral("PBA.efi"), &count);

    // 2 entries: EfiFileDevicePath + EfiDevicePathLen.
    QCOMPARE(count, 2);
    // 8 bytes header per entry. "PBA.efi" = 7 bytes; the second entry
    // carries a 2-byte u16 length payload. Total 8 + 7 + 8 + 2 = 25.
    QCOMPARE(blob.size(), 25);

    Tlv a = walkOne(blob, 0);
    QCOMPARE(a.vendor, quint16{0x8086});
    QCOMPARE(a.type, quint16(OcrTlvType::EfiFileDevicePath));
    QCOMPARE(a.length, quint32{7});
    QCOMPARE(a.value, QByteArray("PBA.efi"));

    Tlv b = walkOne(blob, a.next);
    QCOMPARE(b.vendor, quint16{0x8086});
    QCOMPARE(b.type, quint16(OcrTlvType::EfiDevicePathLen));
    QCOMPARE(b.length, quint32{2});
    QCOMPARE(static_cast<quint8>(b.value.at(0)), quint8{7});
    QCOMPARE(static_cast<quint8>(b.value.at(1)), quint8{0});
    QCOMPARE(b.next, blob.size());
}

void TestOcrTlv::httpsPinnedTlvFiresSha256ByDefault()
{
    const QByteArray imageHash(32, '\xAB');
    int count = 0;
    const QByteArray blob = buildOcrHttpsBootPinnedTlv(
        QStringLiteral("https://recovery.example/pba.efi"),
        QStringLiteral("sha256"),
        imageHash,
        /*serverCertHashAlg*/ {},
        /*serverCertHash*/ {},
        /*username*/ {},
        /*password*/ {},
        &count);

    // URL + image-hash + sync-root-CA = 3 entries.
    QCOMPARE(count, 3);
    Tlv a = walkOne(blob, 0);
    QCOMPARE(a.type, quint16(OcrTlvType::EfiNetworkDevicePath));
    QCOMPARE(a.value, QByteArray("https://recovery.example/pba.efi"));

    Tlv b = walkOne(blob, a.next);
    QCOMPARE(b.type, quint16(OcrTlvType::BootImageHashSha256));
    QCOMPARE(b.value, imageHash);

    Tlv c = walkOne(blob, b.next);
    QCOMPARE(c.type, quint16(OcrTlvType::HttpsCertSyncRootCa));
    QCOMPARE(c.length, quint32{1});
    QCOMPARE(static_cast<quint8>(c.value.at(0)), quint8{1});
}

void TestOcrTlv::httpsPinnedTlvDispatchesSha384And512()
{
    {
        const QByteArray hash(48, '\xCD');
        int count = 0;
        const QByteArray blob = buildOcrHttpsBootPinnedTlv(
            QStringLiteral("https://x/"), QStringLiteral("sha384"), hash,
            {}, {}, {}, {}, &count);
        Tlv hashEntry = walkOne(blob, walkOne(blob, 0).next);
        QCOMPARE(hashEntry.type, quint16(OcrTlvType::BootImageHashSha384));
        QCOMPARE(hashEntry.length, quint32{48});
    }
    {
        const QByteArray hash(64, '\xEF');
        int count = 0;
        const QByteArray blob = buildOcrHttpsBootPinnedTlv(
            QStringLiteral("https://x/"), QStringLiteral("sha512"), hash,
            {}, {}, {}, {}, &count);
        Tlv hashEntry = walkOne(blob, walkOne(blob, 0).next);
        QCOMPARE(hashEntry.type, quint16(OcrTlvType::BootImageHashSha512));
        QCOMPARE(hashEntry.length, quint32{64});
    }
}

void TestOcrTlv::httpsPinnedTlvSkipsHashWhenAlgEmpty()
{
    int count = 0;
    const QByteArray blob = buildOcrHttpsBootPinnedTlv(
        QStringLiteral("https://x/"),
        /*hashAlg*/ {}, /*imageHash*/ {},
        {}, {}, {}, {}, &count);
    // URL + sync-root-CA = 2 entries.
    QCOMPARE(count, 2);
    Tlv a = walkOne(blob, 0);
    Tlv b = walkOne(blob, a.next);
    QCOMPARE(a.type, quint16(OcrTlvType::EfiNetworkDevicePath));
    QCOMPARE(b.type, quint16(OcrTlvType::HttpsCertSyncRootCa));
}

void TestOcrTlv::httpsPinnedTlvIncludesServerCertHashAndCreds()
{
    const QByteArray imageHash(32, '\x11');
    const QByteArray serverCertHash(48, '\x22');
    int count = 0;
    const QByteArray blob = buildOcrHttpsBootPinnedTlv(
        QStringLiteral("https://recovery.example/pba.efi"),
        QStringLiteral("sha256"), imageHash,
        QStringLiteral("sha384"), serverCertHash,
        QStringLiteral("alice"), QStringLiteral("s3cret"),
        &count);

    // URL + image-hash + sync-CA + server-cert-hash + user + pass = 6.
    QCOMPARE(count, 6);

    int o = 0;
    Tlv url = walkOne(blob, o); o = url.next;
    Tlv imgHash = walkOne(blob, o); o = imgHash.next;
    Tlv syncCa = walkOne(blob, o); o = syncCa.next;
    Tlv certHash = walkOne(blob, o); o = certHash.next;
    Tlv user = walkOne(blob, o); o = user.next;
    Tlv pass = walkOne(blob, o); o = pass.next;

    QCOMPARE(url.type, quint16(OcrTlvType::EfiNetworkDevicePath));
    QCOMPARE(imgHash.type, quint16(OcrTlvType::BootImageHashSha256));
    QCOMPARE(syncCa.type, quint16(OcrTlvType::HttpsCertSyncRootCa));
    QCOMPARE(certHash.type, quint16(OcrTlvType::HttpsServerCertHashSha384));
    QCOMPARE(certHash.value, serverCertHash);
    QCOMPARE(user.type, quint16(OcrTlvType::HttpsUserName));
    QCOMPARE(user.value, QByteArray("alice"));
    QCOMPARE(pass.type, quint16(OcrTlvType::HttpsPassword));
    QCOMPARE(pass.value, QByteArray("s3cret"));
    QCOMPARE(o, blob.size());
}

void TestOcrTlv::httpsPinnedTlvSkipsUnknownHashAlg()
{
    int count = 0;
    const QByteArray blob = buildOcrHttpsBootPinnedTlv(
        QStringLiteral("https://x/"),
        QStringLiteral("md5"), QByteArray(16, '\x00'),
        {}, {}, {}, {}, &count);
    // Bogus alg → image hash entry skipped. URL + sync-root-CA = 2.
    QCOMPARE(count, 2);
}

QTEST_GUILESS_MAIN(TestOcrTlv)
#include "test_ocr_tlv.moc"
