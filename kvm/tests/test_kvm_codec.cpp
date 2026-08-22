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

/// Deflate the bytes in `in` as a zlib stream (with the 2-byte header)
/// — matches what real AMT firmware ships and what
/// `InflateStream` (windowBits=15) expects. Name kept for historical
/// reasons; the helper used to emit raw deflate.
QByteArray rawDeflate(const QByteArray &in)
{
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                       15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
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
    void buildSetPixelFormatPinsRgb565();
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
    void decodeRlePackedPalette1Bit();
    void decodeRlePaletteRleWithRuns();
    // #433 — 8-bit RGB332 and the AMT display-buffer ceiling.
    void rgb332ToArgbBoundaries();
    void buildSetPixelFormat8BitMatchesReference();
    void chooseFormatHonoursDisplayBufferCeiling();
    void decodeRaw8Bit();
    void decodeRleSolid8Bit();
    void decodeRleSingleColorRun8Bit();
    void decodeRlePackedPalette8Bit();
    void decodeRlePaletteRle8Bit();
    // #436 — dataLen sanity bound.
    void decodeRleRejectsOversizedDataLen();
    void decodeRleAcceptsLargestHonestBlock();
    void decodeRleHugeDataLenDoesNotStallOnNeedMore();
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

void TestKvmCodec::buildSetPixelFormatPinsRgb565()
{
    // Locking the exact 20-byte wire matters: a single bad byte would
    // break KVM on every machine. The decoder treats pixels as
    // little-endian RGB565 whenever this format is negotiated, so the
    // bytes we send must match that assumption exactly.
    const QByteArray got = buildSetPixelFormat(PixelFormat::Rgb565);
    QCOMPARE(got.size(), 20);

    QByteArray want;
    want.append(char(0x00));      // msg type SetPixelFormat
    want.append(char(0x00));      // padding
    want.append(char(0x00));
    want.append(char(0x00));
    want.append(char(16));        // bpp
    want.append(char(16));        // depth
    want.append(char(0));         // big-endian = false (little-endian)
    want.append(char(1));         // true-colour = true
    want.append(be16(31));        // red-max
    want.append(be16(63));        // green-max
    want.append(be16(31));        // blue-max
    want.append(char(11));        // red-shift
    want.append(char(5));         // green-shift
    want.append(char(0));         // blue-shift
    want.append(char(0x00));      // padding
    want.append(char(0x00));
    want.append(char(0x00));
    QCOMPARE(got, want);
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
    QVERIFY(deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8,
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

void TestKvmCodec::decodeRlePackedPalette1Bit()
{
    // Subencoding 2: 2-entry palette (RGB565), then 1bpp packed indices.
    // 4x4 tile → 16 pixels → 2 bytes of packed indices.
    // Palette: 0=red, 1=blue.
    // Bit pattern: row0 0000, row1 1111, row2 0101, row3 1010 → bytes
    //   byte0 = 00001111 = 0x0F (rows 0+1)
    //   byte1 = 01011010 = 0x5A (rows 2+3)
    QByteArray inner;
    inner.append(char(2));                  // subencoding
    inner.append(le16(0xF800));              // palette[0] = red
    inner.append(le16(0x001F));              // palette[1] = blue
    inner.append(char(0x0F));
    inner.append(char(0x5A));

    // Wrap as an uncompressed-marker block (stored-deflate-style).
    QByteArray block;
    block.append(char(0));                                       // marker
    block.append(le16(static_cast<quint16>(inner.size())));      // LEN
    block.append(QByteArray(2, '\0'));                           // NLEN ignored
    block.append(inner);

    QByteArray payload;
    payload.append(be32(static_cast<quint32>(block.size())));
    payload.append(block);

    RectHeader rect;
    rect.w = 4; rect.h = 4;
    rect.encoding = EncRle;

    InflateStream stream;
    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed, &stream),
              DecodeStatus::Ok);

    const QRgb red  = rgb565ToArgb(0xF800);
    const QRgb blue = rgb565ToArgb(0x001F);
    // row 0: 0000 → all red
    for (int x = 0; x < 4; ++x) QCOMPARE(dr.image.pixel(x, 0), red);
    // row 1: 1111 → all blue
    for (int x = 0; x < 4; ++x) QCOMPARE(dr.image.pixel(x, 1), blue);
    // row 2: 0101 → red, blue, red, blue
    QCOMPARE(dr.image.pixel(0, 2), red);
    QCOMPARE(dr.image.pixel(1, 2), blue);
    QCOMPARE(dr.image.pixel(2, 2), red);
    QCOMPARE(dr.image.pixel(3, 2), blue);
    // row 3: 1010 → blue, red, blue, red
    QCOMPARE(dr.image.pixel(0, 3), blue);
    QCOMPARE(dr.image.pixel(1, 3), red);
    QCOMPARE(dr.image.pixel(2, 3), blue);
    QCOMPARE(dr.image.pixel(3, 3), red);
}

void TestKvmCodec::decodeRlePaletteRleWithRuns()
{
    // Subencoding 130: 2-entry palette + RLE'd indices. High bit set on
    // an index means the next byte(s) extend the run (255 = continue).
    //
    // Plan for a 4x4 tile (16 pixels):
    //   index 0x80 then 0x07 → palette[0] for run of 8 pixels (1 + 7)
    //   index 0x81 then 0x07 → palette[1] for run of 8 pixels
    QByteArray inner;
    inner.append(char(130));                 // subencoding
    inner.append(le16(0xF800));              // palette[0] = red
    inner.append(le16(0x001F));              // palette[1] = blue
    inner.append(char(0x80));                // palette[0], run flag
    inner.append(char(0x07));                // run +7 → total 8
    inner.append(char(0x81));                // palette[1], run flag
    inner.append(char(0x07));                // run +7 → total 8

    QByteArray block;
    block.append(char(0));
    block.append(le16(static_cast<quint16>(inner.size())));
    block.append(QByteArray(2, '\0'));
    block.append(inner);

    QByteArray payload;
    payload.append(be32(static_cast<quint32>(block.size())));
    payload.append(block);

    RectHeader rect;
    rect.w = 4; rect.h = 4;
    rect.encoding = EncRle;

    InflateStream stream;
    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed, &stream),
              DecodeStatus::Ok);

    const QRgb red  = rgb565ToArgb(0xF800);
    const QRgb blue = rgb565ToArgb(0x001F);
    // First 8 pixels (rows 0-1) red, next 8 (rows 2-3) blue.
    for (int x = 0; x < 4; ++x) QCOMPARE(dr.image.pixel(x, 0), red);
    for (int x = 0; x < 4; ++x) QCOMPARE(dr.image.pixel(x, 1), red);
    for (int x = 0; x < 4; ++x) QCOMPARE(dr.image.pixel(x, 2), blue);
    for (int x = 0; x < 4; ++x) QCOMPARE(dr.image.pixel(x, 3), blue);
}

// --- #433: 8-bit RGB332 support -------------------------------------
//
// AMT's KVM display buffer is capped at 9,216,000 bytes. A 4K desktop
// needs 16.6 MB at 16-bit but only 8.3 MB at 8-bit, so 8-bit is the
// only way to drive one — the firmware silently drops the redirection
// connection rather than reporting the overflow.

namespace {

/// Wrap `inner` (subencoding byte + body) in the uncompressed-ZLib
/// marker framing an RLE rect uses, and prepend the u32 dataLen.
QByteArray wrapUncompressedRle(const QByteArray &inner)
{
    const quint32 dataLen = static_cast<quint32>(5 + inner.size());
    QByteArray payload;
    payload.append(be32(dataLen));
    payload.append(char(0x00));
    payload.append(le16(static_cast<quint16>(inner.size())));
    payload.append(char(0x00));
    payload.append(char(0x00));
    payload.append(inner);
    return payload;
}

} // namespace

void TestKvmCodec::rgb332ToArgbBoundaries()
{
    QCOMPARE(rgb332ToArgb(0x00), quint32(0xFF000000));
    QCOMPARE(rgb332ToArgb(0xFF), quint32(0xFFFFFFFF));  // must reach true white
    QCOMPARE(rgb332ToArgb(0xE0), quint32(0xFFFF0000));  // pure red
    QCOMPARE(rgb332ToArgb(0x1C), quint32(0xFF00FF00));  // pure green
    QCOMPARE(rgb332ToArgb(0x03), quint32(0xFF0000FF));  // pure blue
}

void TestKvmCodec::buildSetPixelFormat8BitMatchesReference()
{
    // Byte-for-byte the legacy client's RLE8 SetPixelFormat
    // (Commander.htm:35714): 8bpp / depth 8, little-endian, true-colour,
    // max 7/7/3, shift 5/2/0.
    const QByteArray got = buildSetPixelFormat(PixelFormat::Rgb332);
    QCOMPARE(got.size(), 20);

    QByteArray want;
    want.append(char(0x00));      // msg type SetPixelFormat
    want.append(char(0x00));      // padding
    want.append(char(0x00));
    want.append(char(0x00));
    want.append(char(8));         // bpp
    want.append(char(8));         // depth
    want.append(char(0));         // big-endian = false
    want.append(char(1));         // true-colour = true
    want.append(be16(7));         // red-max
    want.append(be16(7));         // green-max
    want.append(be16(3));         // blue-max
    want.append(char(5));         // red-shift
    want.append(char(2));         // green-shift
    want.append(char(0));         // blue-shift
    want.append(char(0x00));      // padding
    want.append(char(0x00));
    want.append(char(0x00));
    QCOMPARE(got, want);
}

void TestKvmCodec::chooseFormatHonoursDisplayBufferCeiling()
{
    QCOMPARE(kMaxDisplayBufferBytes, Q_INT64_C(9216000));

    // Ordinary desktops keep 16-bit colour.
    QCOMPARE(chooseFormatFor(1024, 768),  PixelFormat::Rgb565);
    QCOMPARE(chooseFormatFor(1920, 1080), PixelFormat::Rgb565);
    QCOMPARE(chooseFormatFor(2560, 1440), PixelFormat::Rgb565);

    // Exactly at the ceiling still fits: 2560x1800x2 == 9,216,000.
    QCOMPARE(displayBufferBytes(PixelFormat::Rgb565, 2560, 1800),
             kMaxDisplayBufferBytes);
    QCOMPARE(chooseFormatFor(2560, 1800), PixelFormat::Rgb565);

    // One pixel row over drops to 8-bit.
    QCOMPARE(chooseFormatFor(2560, 1801), PixelFormat::Rgb332);

    // The reporter's 4K target: 16-bit is 1.8x over, 8-bit fits.
    QCOMPARE(chooseFormatFor(3840, 2160), PixelFormat::Rgb332);
    QCOMPARE(displayBufferBytes(PixelFormat::Rgb565, 3840, 2160),
             Q_INT64_C(16588800));
    QVERIFY(displayBufferBytes(PixelFormat::Rgb332, 3840, 2160)
            <= kMaxDisplayBufferBytes);

    // Beyond 9,216,000 pixels no format can fit, and the caller is
    // expected to notice rather than let AMT hang up.
    QVERIFY(displayBufferBytes(chooseFormatFor(7680, 4320), 7680, 4320)
            > kMaxDisplayBufferBytes);
}

void TestKvmCodec::decodeRaw8Bit()
{
    RectHeader rect;
    rect.x = 0; rect.y = 0; rect.w = 2; rect.h = 2;
    rect.encoding = EncRaw;

    // One byte per pixel: red, green, blue, white.
    QByteArray payload;
    payload.append(char(0xE0));
    payload.append(char(0x1C));
    payload.append(char(0x03));
    payload.append(char(0xFF));

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed, nullptr,
                            PixelFormat::Rgb332),
             DecodeStatus::Ok);
    QCOMPARE(consumed, 4);            // half of what RGB565 would consume
    QCOMPARE(dr.image.pixel(0, 0), QRgb(0xFFFF0000));
    QCOMPARE(dr.image.pixel(1, 0), QRgb(0xFF00FF00));
    QCOMPARE(dr.image.pixel(0, 1), QRgb(0xFF0000FF));
    QCOMPARE(dr.image.pixel(1, 1), QRgb(0xFFFFFFFF));
}

void TestKvmCodec::decodeRleSolid8Bit()
{
    RectHeader rect;
    rect.w = 4; rect.h = 4;
    rect.encoding = EncRle;

    QByteArray inner;
    inner.append(char(0x01));   // subencoding: solid
    inner.append(char(0xE0));   // red, one byte in 8-bit mode

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(wrapUncompressedRle(inner), rect, &dr, &consumed,
                            nullptr, PixelFormat::Rgb332),
             DecodeStatus::Ok);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            QCOMPARE(dr.image.pixel(x, y), QRgb(0xFFFF0000));
}

void TestKvmCodec::decodeRleSingleColorRun8Bit()
{
    RectHeader rect;
    rect.w = 4; rect.h = 1;
    rect.encoding = EncRle;

    // subenc 128: colour byte + run-length bytes (run = 1 + sum).
    QByteArray inner;
    inner.append(char(char(128)));
    inner.append(char(0xE0)); inner.append(char(1));   // red x2
    inner.append(char(0x03)); inner.append(char(1));   // blue x2

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(wrapUncompressedRle(inner), rect, &dr, &consumed,
                            nullptr, PixelFormat::Rgb332),
             DecodeStatus::Ok);
    QCOMPARE(dr.image.pixel(0, 0), QRgb(0xFFFF0000));
    QCOMPARE(dr.image.pixel(1, 0), QRgb(0xFFFF0000));
    QCOMPARE(dr.image.pixel(2, 0), QRgb(0xFF0000FF));
    QCOMPARE(dr.image.pixel(3, 0), QRgb(0xFF0000FF));
}

void TestKvmCodec::decodeRlePackedPalette8Bit()
{
    RectHeader rect;
    rect.w = 8; rect.h = 1;
    rect.encoding = EncRle;

    // subenc 2 => 2 palette entries (1 byte each here), 1 bit per pixel.
    QByteArray inner;
    inner.append(char(2));
    inner.append(char(0xE0));   // palette[0] red
    inner.append(char(0x03));   // palette[1] blue
    inner.append(char(0xAA));   // 10101010

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(wrapUncompressedRle(inner), rect, &dr, &consumed,
                            nullptr, PixelFormat::Rgb332),
             DecodeStatus::Ok);
    for (int x = 0; x < 8; ++x) {
        QCOMPARE(dr.image.pixel(x, 0),
                 QRgb((x % 2 == 0) ? 0xFF0000FF : 0xFFFF0000));
    }
}

void TestKvmCodec::decodeRlePaletteRle8Bit()
{
    RectHeader rect;
    rect.w = 4; rect.h = 1;
    rect.encoding = EncRle;

    // subenc 130 => 2 palette entries, then index bytes; bit 7 marks a run.
    QByteArray inner;
    inner.append(char(char(130)));
    inner.append(char(0xE0));   // palette[0] red
    inner.append(char(0x03));   // palette[1] blue
    inner.append(char(0x80));   // index 0, run flag
    inner.append(char(1));      // run = 1 + 1 = 2 red
    inner.append(char(0x81));   // index 1, run flag
    inner.append(char(1));      // run = 2 blue

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(wrapUncompressedRle(inner), rect, &dr, &consumed,
                            nullptr, PixelFormat::Rgb332),
             DecodeStatus::Ok);
    QCOMPARE(dr.image.pixel(0, 0), QRgb(0xFFFF0000));
    QCOMPARE(dr.image.pixel(1, 0), QRgb(0xFFFF0000));
    QCOMPARE(dr.image.pixel(2, 0), QRgb(0xFF0000FF));
    QCOMPARE(dr.image.pixel(3, 0), QRgb(0xFF0000FF));
}


// --- #436: dataLen sanity bound --------------------------------------

void TestKvmCodec::decodeRleRejectsOversizedDataLen()
{
    RectHeader rect;
    rect.w = 64; rect.h = 64;
    rect.encoding = EncRle;

    // A 64x64 tile cannot honestly need 1 MB. Before the bound this
    // returned NeedMore and the session waited for bytes that were
    // never coming.
    QByteArray payload = be32(1024 * 1024);
    payload.append(QByteArray(64, '\0'));

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(payload, rect, &dr, &consumed, nullptr,
                            PixelFormat::Rgb565),
             DecodeStatus::Malformed);
}

void TestKvmCodec::decodeRleAcceptsLargestHonestBlock()
{
    // The pathological-but-legal case the ceiling must not reject:
    // subencoding 128 where every run is a single pixel, so the block
    // carries colour + run-length for all w*h pixels.
    RectHeader rect;
    rect.w = 8; rect.h = 8;
    rect.encoding = EncRle;

    QByteArray inner;
    inner.append(char(char(128)));
    for (int i = 0; i < 64; ++i) {
        inner.append(le16(quint16(i % 2 ? 0xF800 : 0x001F)));
        inner.append(char(0));   // delta 0 => run of exactly 1
    }

    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(wrapUncompressedRle(inner), rect, &dr, &consumed,
                            nullptr, PixelFormat::Rgb565),
             DecodeStatus::Ok);
    QCOMPARE(dr.image.pixel(0, 0), QRgb(0xFF0000F8));
    QCOMPARE(dr.image.pixel(1, 0), QRgb(0xFFF80000));

    // And the 8-bit equivalent, whose ceiling is correspondingly lower
    // but must still admit its own worst case.
    QByteArray inner8;
    inner8.append(char(char(128)));
    for (int i = 0; i < 64; ++i) {
        inner8.append(char(i % 2 ? 0xE0 : 0x03));
        inner8.append(char(0));
    }
    DecodedRect dr8;
    QCOMPARE(tryDecodeRect(wrapUncompressedRle(inner8), rect, &dr8, &consumed,
                            nullptr, PixelFormat::Rgb332),
             DecodeStatus::Ok);
}

void TestKvmCodec::decodeRleHugeDataLenDoesNotStallOnNeedMore()
{
    RectHeader rect;
    rect.w = 64; rect.h = 64;
    rect.encoding = EncRle;

    // dataLen near INT_MAX: `4 + dataLen` used to be evaluated in int
    // and overflow. Whatever the arithmetic does, the answer must be a
    // terminal Malformed — never NeedMore, which would stall the frame
    // loop permanently.
    for (const qint32 len : {qint32(0x7FFFFFFF), qint32(0x7FFFFFFC),
                             qint32(0x40000000), qint32(0x00FFFFFF)}) {
        QByteArray payload = be32(static_cast<quint32>(len));
        payload.append(QByteArray(32, '\0'));

        DecodedRect dr;
        int consumed = -1;
        const DecodeStatus st = tryDecodeRect(payload, rect, &dr, &consumed,
                                               nullptr, PixelFormat::Rgb565);
        QVERIFY2(st == DecodeStatus::Malformed,
                  qPrintable(QStringLiteral("dataLen 0x%1 gave status %2, "
                                            "expected Malformed")
                                 .arg(len, 0, 16)
                                 .arg(static_cast<int>(st))));
    }

    // A negative-looking length (high bit set) is equally terminal.
    QByteArray neg = be32(0xFFFFFFFFu);
    neg.append(QByteArray(32, '\0'));
    DecodedRect dr;
    int consumed = -1;
    QCOMPARE(tryDecodeRect(neg, rect, &dr, &consumed, nullptr,
                            PixelFormat::Rgb565),
             DecodeStatus::Malformed);
}

QTEST_GUILESS_MAIN(TestKvmCodec)
#include "test_kvm_codec.moc"
