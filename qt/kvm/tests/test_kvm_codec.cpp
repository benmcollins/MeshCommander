// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvm/kvm_codec.h"

#include <QtTest>

#include <zlib.h>

using namespace qumesh::kvm;

namespace {

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

/// Raw-deflate the bytes in `in` using a fresh stream with the same
/// window-bits the inflate side uses (-15). Returns the compressed
/// stream that, when fed to InflateStream::inflate, recovers `in`.
QByteArray rawDeflate(const QByteArray &in)
{
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                       -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return {};
    }
    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    zs.avail_in = static_cast<uInt>(in.size());

    QByteArray out;
    QByteArray buf(4096, '\0');
    int rc;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(buf.data());
        zs.avail_out = static_cast<uInt>(buf.size());
        rc = deflate(&zs, Z_FINISH);
        const int produced = static_cast<int>(buf.size() - zs.avail_out);
        if (produced > 0) out.append(buf.constData(), produced);
    } while (rc == Z_OK);
    deflateEnd(&zs);
    return out;
}

} // namespace

class TestKvmCodec : public QObject
{
    Q_OBJECT
private slots:
    void rgb565ToArgbBoundaries();
    void buildVersionAndChoice();
    void parseSecurityTypes();
    void parseServerInitWithName();
    void framebufferUpdateRequestLayout();
    void keyAndPointerEventLayouts();
    void parseFrameUpdateAndRectHeader();
    void decodeRawRect();
    void decodeRleSolid();
    void decodeRleSingleColorRun();
    void decodeDesktopSizeFlag();
    void decodeRleCompressedReportsUnsupported();
    void decodeRleNeedsMoreOnShortBuffer();
    void decodeRleCompressedWithInflate();
    void inflateStreamPreservesWindowAcrossBlocks();
};

void TestKvmCodec::rgb565ToArgbBoundaries()
{
    QCOMPARE(rgb565ToArgb(0x0000), quint32(0xFF000000));
    QCOMPARE(rgb565ToArgb(0xFFFF), quint32(0xFFF8FCF8));
    // Pure red (top 5 bits set)
    QCOMPARE(rgb565ToArgb(0xF800), quint32(0xFFF80000));
    // Pure green (middle 6 bits set)
    QCOMPARE(rgb565ToArgb(0x07E0), quint32(0xFF00FC00));
    // Pure blue (low 5 bits set)
    QCOMPARE(rgb565ToArgb(0x001F), quint32(0xFF0000F8));
}

void TestKvmCodec::buildVersionAndChoice()
{
    QCOMPARE(buildVersionResponse(), QByteArrayLiteral("RFB 003.008\n"));
    QCOMPARE(buildSecurityChoice(), QByteArrayLiteral("\x01"));
    QCOMPARE(buildClientInit(), QByteArrayLiteral("\x01"));
}

void TestKvmCodec::parseSecurityTypes()
{
    QByteArray buf;
    buf.append(char(0x02));      // 2 security types
    buf.append(char(0x01));      // None
    buf.append(char(0x02));      // VNC auth
    const int n = tryParseSecurityTypes(buf);
    QCOMPARE(n, 3);

    QByteArray short_(1, '\0'); short_[0] = 0x02;
    QCOMPARE(tryParseSecurityTypes(short_), 0);
}

void TestKvmCodec::parseServerInitWithName()
{
    QByteArray frame;
    frame.append(be16(1024));     // width
    frame.append(be16(768));      // height
    frame.append(QByteArray(16, '\0')); // pixel format fields (ignored)
    frame.append(be32(4));        // namelen
    frame.append("AMTP");

    ServerInit info;
    int consumed = -1;
    QVERIFY(tryParseServerInit(frame, &info, &consumed));
    QCOMPARE(consumed, 28);
    QCOMPARE(info.width, quint16(1024));
    QCOMPARE(info.height, quint16(768));
    QCOMPARE(info.name, QStringLiteral("AMTP"));
}

void TestKvmCodec::framebufferUpdateRequestLayout()
{
    QByteArray f = buildFramebufferUpdateRequest(true, 0, 0, 800, 600);
    QCOMPARE(f.size(), 10);
    QCOMPARE(static_cast<unsigned char>(f.at(0)), quint8(MsgFramebufferUpdateRequest));
    QCOMPARE(static_cast<unsigned char>(f.at(1)), quint8(1));
    QCOMPARE(static_cast<unsigned char>(f.at(6)), quint8(800 >> 8));
    QCOMPARE(static_cast<unsigned char>(f.at(7)), quint8(800 & 0xFF));
    QCOMPARE(static_cast<unsigned char>(f.at(8)), quint8(600 >> 8));
}

void TestKvmCodec::keyAndPointerEventLayouts()
{
    QByteArray k = buildKeyEvent(0xFF0D, true);
    QCOMPARE(k.size(), 8);
    QCOMPARE(static_cast<unsigned char>(k.at(0)), quint8(MsgKeyEvent));
    QCOMPARE(static_cast<unsigned char>(k.at(1)), quint8(1));
    // keysym big-endian
    QCOMPARE(static_cast<unsigned char>(k.at(6)), quint8(0xFF));
    QCOMPARE(static_cast<unsigned char>(k.at(7)), quint8(0x0D));

    QByteArray p = buildPointerEvent(0x01, 0x1234, 0x5678);
    QCOMPARE(p.size(), 6);
    QCOMPARE(static_cast<unsigned char>(p.at(0)), quint8(MsgPointerEvent));
    QCOMPARE(static_cast<unsigned char>(p.at(1)), quint8(0x01));
    QCOMPARE(static_cast<unsigned char>(p.at(2)), quint8(0x12));
    QCOMPARE(static_cast<unsigned char>(p.at(5)), quint8(0x78));
}

void TestKvmCodec::parseFrameUpdateAndRectHeader()
{
    QByteArray buf;
    buf.append(char(MsgFramebufferUpdate));
    buf.append(char(0x00));
    buf.append(be16(5));  // 5 rects

    FrameUpdateHeader hdr;
    int consumed = -1;
    QVERIFY(tryParseFrameUpdate(buf, &hdr, &consumed));
    QCOMPARE(consumed, 4);
    QCOMPARE(hdr.numRects, quint16(5));

    QByteArray rectBuf;
    rectBuf.append(be16(10));         // x
    rectBuf.append(be16(20));         // y
    rectBuf.append(be16(64));         // w
    rectBuf.append(be16(48));         // h
    rectBuf.append(be32(0));          // RAW encoding

    RectHeader rect;
    int rc = -1;
    QVERIFY(tryParseRectHeader(rectBuf, &rect, &rc));
    QCOMPARE(rc, 12);
    QCOMPARE(rect.x, quint16(10));
    QCOMPARE(rect.y, quint16(20));
    QCOMPARE(rect.w, quint16(64));
    QCOMPARE(rect.h, quint16(48));
    QCOMPARE(rect.encoding, qint32(EncRaw));
}

void TestKvmCodec::decodeRawRect()
{
    RectHeader rect;
    rect.x = 0; rect.y = 0; rect.w = 2; rect.h = 2;
    rect.encoding = EncRaw;

    // 2x2 image: red, green, blue, white
    QByteArray payload;
    payload.append(le16(0xF800)); // red
    payload.append(le16(0x07E0)); // green
    payload.append(le16(0x001F)); // blue
    payload.append(le16(0xFFFF)); // white-ish

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed), DecodeStatus::Ok);
    QCOMPARE(consumed, 8);
    QVERIFY(!dr.image.isNull());
    QCOMPARE(dr.image.width(), 2);
    QCOMPARE(dr.image.height(), 2);
    QCOMPARE(dr.image.pixel(0, 0), QRgb(0xFFF80000));
    QCOMPARE(dr.image.pixel(1, 0), QRgb(0xFF00FC00));
    QCOMPARE(dr.image.pixel(0, 1), QRgb(0xFF0000F8));
}

void TestKvmCodec::decodeRleSolid()
{
    RectHeader rect;
    rect.w = 4; rect.h = 4;
    rect.encoding = EncRle;

    QByteArray innerData;
    innerData.append(char(0x01));    // subencoding: solid
    innerData.append(le16(0xF800)); // red
    const quint32 dataLen = static_cast<quint32>(5 + innerData.size());

    QByteArray payload;
    payload.append(be32(dataLen));
    // Uncompressed ZLib marker: byte 0=0, u16 len LE
    payload.append(char(0x00));
    payload.append(le16(static_cast<quint16>(innerData.size())));
    // Two more zero bytes to pad out the 5-byte marker
    payload.append(char(0x00));
    payload.append(char(0x00));
    payload.append(innerData);

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed), DecodeStatus::Ok);
    QCOMPARE(consumed, 4 + static_cast<int>(dataLen));
    QCOMPARE(dr.image.width(), 4);
    QCOMPARE(dr.image.height(), 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            QCOMPARE(dr.image.pixel(x, y), QRgb(0xFFF80000));
        }
    }
}

void TestKvmCodec::decodeRleSingleColorRun()
{
    RectHeader rect;
    rect.w = 4; rect.h = 1;             // 4 pixels total
    rect.encoding = EncRle;

    QByteArray innerData;
    innerData.append(char(0x80));        // subencoding: 128 (single-color RLE)
    innerData.append(le16(0x07E0));     // green
    innerData.append(char(0x03));        // run length: 1 + 3 = 4 pixels

    const quint32 dataLen = static_cast<quint32>(5 + innerData.size());
    QByteArray payload;
    payload.append(be32(dataLen));
    payload.append(char(0x00));
    payload.append(le16(static_cast<quint16>(innerData.size())));
    payload.append(char(0x00));
    payload.append(char(0x00));
    payload.append(innerData);

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed), DecodeStatus::Ok);
    QCOMPARE(consumed, 4 + static_cast<int>(dataLen));
    for (int x = 0; x < 4; ++x) {
        QCOMPARE(dr.image.pixel(x, 0), QRgb(0xFF00FC00));
    }
}

void TestKvmCodec::decodeDesktopSizeFlag()
{
    RectHeader rect;
    rect.w = 1280; rect.h = 1024;
    rect.encoding = EncDesktopSize;

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(QByteArrayView(), rect, &dr, &consumed), DecodeStatus::Ok);
    QCOMPARE(consumed, 0);
    QVERIFY(dr.isDesktopSize);
    QVERIFY(dr.image.isNull());
}

void TestKvmCodec::decodeRleCompressedReportsUnsupported()
{
    RectHeader rect;
    rect.w = 2; rect.h = 2;
    rect.encoding = EncRle;

    // Compressed block — first byte is not 0x00, so the uncompressed
    // marker check fails. Should surface as UnsupportedSubencoding,
    // never NeedMore.
    const quint32 dataLen = 32;
    QByteArray payload;
    payload.append(be32(dataLen));
    payload.append(char(0x78)); // ZLib compressed magic
    payload.append(char(0x9C));
    for (quint32 i = 2; i < dataLen; ++i) payload.append(char(0xAA));

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed),
              DecodeStatus::UnsupportedSubencoding);
}

void TestKvmCodec::decodeRleNeedsMoreOnShortBuffer()
{
    RectHeader rect;
    rect.w = 2; rect.h = 2;
    rect.encoding = EncRle;

    // dataLen claims 50 bytes but we only deliver 10. Must say NeedMore,
    // never Malformed / UnsupportedSubencoding.
    QByteArray payload;
    payload.append(be32(50));
    for (int i = 0; i < 10; ++i) payload.append(char(0x00));

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed), DecodeStatus::NeedMore);
}

void TestKvmCodec::decodeRleCompressedWithInflate()
{
    RectHeader rect;
    rect.w = 4; rect.h = 4;
    rect.encoding = EncRle;

    // Reference inner payload (no marker): subencoding 1 (solid) +
    // RGB565 red. Compress with raw-deflate and wrap as a RLE rect.
    QByteArray inner;
    inner.append(char(0x01));
    inner.append(le16(0xF800));

    const QByteArray deflated = rawDeflate(inner);
    QVERIFY(!deflated.isEmpty());
    // First byte of a raw deflate stream is almost never 0x00, but we
    // assert anyway because if it ever is, we'd accidentally hit the
    // uncompressed-marker path.
    QVERIFY(static_cast<unsigned char>(deflated.at(0)) != 0x00);

    QByteArray payload;
    payload.append(be32(static_cast<quint32>(deflated.size())));
    payload.append(deflated);

    InflateStream stream;
    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed, &stream),
              DecodeStatus::Ok);
    QCOMPARE(consumed, 4 + deflated.size());
    QCOMPARE(dr.image.width(), 4);
    QCOMPARE(dr.image.height(), 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            QCOMPARE(dr.image.pixel(x, y), QRgb(0xFFF80000));
}

void TestKvmCodec::inflateStreamPreservesWindowAcrossBlocks()
{
    // Compress two blocks with the SAME deflate stream so the second
    // block references the first's dictionary. Feed both through one
    // InflateStream and verify both inflate correctly.
    z_stream zs{};
    QVERIFY(deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                          Z_DEFAULT_STRATEGY) == Z_OK);

    auto deflateBlock = [&zs](const QByteArray &in) -> QByteArray {
        zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
        zs.avail_in = static_cast<uInt>(in.size());
        QByteArray out;
        QByteArray buf(4096, '\0');
        int rc;
        do {
            zs.next_out = reinterpret_cast<Bytef *>(buf.data());
            zs.avail_out = static_cast<uInt>(buf.size());
            rc = deflate(&zs, Z_SYNC_FLUSH);
            const int produced = static_cast<int>(buf.size() - zs.avail_out);
            if (produced > 0) out.append(buf.constData(), produced);
        } while (zs.avail_out == 0);
        return out;
    };

    const QByteArray inner1 = QByteArrayLiteral("\x01") + le16(0x07E0); // green
    const QByteArray inner2 = QByteArrayLiteral("\x01") + le16(0x001F); // blue
    const QByteArray d1 = deflateBlock(inner1);
    const QByteArray d2 = deflateBlock(inner2);
    deflateEnd(&zs);

    InflateStream stream;

    auto runRect = [&stream](const QByteArray &dBlock,
                              quint16 expected) {
        RectHeader rect;
        rect.w = 2; rect.h = 2;
        rect.encoding = EncRle;

        QByteArray payload;
        payload.append(be32(static_cast<quint32>(dBlock.size())));
        payload.append(dBlock);

        DecodedRect dr;
        int consumed = -1;
        QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed, &stream),
                  DecodeStatus::Ok);

        const QRgb expectedArgb = rgb565ToArgb(expected);
        QCOMPARE(dr.image.pixel(0, 0), expectedArgb);
    };

    runRect(d1, 0x07E0);
    runRect(d2, 0x001F);
}

QTEST_GUILESS_MAIN(TestKvmCodec)
#include "test_kvm_codec.moc"
