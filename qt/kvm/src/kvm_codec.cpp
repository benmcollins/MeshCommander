// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvm/kvm_codec.h"

namespace meshcommander::kvm {

namespace {

constexpr int kVersionMessageSize = 12;
constexpr int kSecurityResultSize = 4;
constexpr int kServerInitFixedSize = 24;
constexpr int kRectHeaderSize = 12;
constexpr int kBytesPerPixel = 2; // RGB565

QByteArray pack16Be(quint16 v)
{
    QByteArray b(2, '\0');
    b[0] = static_cast<char>((v >> 8) & 0xFF);
    b[1] = static_cast<char>(v & 0xFF);
    return b;
}

QByteArray pack32Be(quint32 v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>((v >> 24) & 0xFF);
    b[1] = static_cast<char>((v >> 16) & 0xFF);
    b[2] = static_cast<char>((v >> 8) & 0xFF);
    b[3] = static_cast<char>(v & 0xFF);
    return b;
}

quint16 read16Be(QByteArrayView b, int off)
{
    return (static_cast<quint16>(static_cast<unsigned char>(b[off])) << 8)
         | static_cast<quint16>(static_cast<unsigned char>(b[off + 1]));
}

qint32 read32Be(QByteArrayView b, int off)
{
    return (static_cast<qint32>(static_cast<unsigned char>(b[off])) << 24)
         | (static_cast<qint32>(static_cast<unsigned char>(b[off + 1])) << 16)
         | (static_cast<qint32>(static_cast<unsigned char>(b[off + 2])) << 8)
         | static_cast<qint32>(static_cast<unsigned char>(b[off + 3]));
}

} // namespace

quint32 rgb565ToArgb(quint16 v)
{
    const quint32 r = (v >> 8) & 0xF8;
    const quint32 g = (v >> 3) & 0xFC;
    const quint32 b = (v & 0x1F) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// --- Handshake -------------------------------------------------------

QByteArray buildVersionResponse()
{
    return QByteArrayLiteral("RFB 003.008\n");
}

int tryParseSecurityTypes(QByteArrayView buffer)
{
    if (buffer.size() < 1) return 0;
    const int count = static_cast<unsigned char>(buffer[0]);
    const int total = 1 + count;
    if (buffer.size() < total) return 0;
    return total;
}

QByteArray buildSecurityChoice()
{
    QByteArray b(1, '\0');
    b[0] = 0x01;
    return b;
}

bool tryParseSecurityResult(QByteArrayView buffer, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < kSecurityResultSize) return false;
    if (consumed != nullptr) *consumed = kSecurityResultSize;
    // We don't actually return the result here — the caller checks via
    // the consumed count. A non-zero status is handled by the session
    // driver before calling this.
    return read32Be(buffer, 0) == 0;
}

QByteArray buildClientInit()
{
    QByteArray b(1, '\0');
    b[0] = 0x01;
    return b;
}

bool tryParseServerInit(QByteArrayView buffer, ServerInit *info, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < kServerInitFixedSize) return false;
    const qint32 namelen = read32Be(buffer, 20);
    if (namelen < 0 || namelen > (1 << 20)) return false; // sanity
    if (buffer.size() < kServerInitFixedSize + namelen) return false;
    if (info != nullptr) {
        info->width  = read16Be(buffer, 0);
        info->height = read16Be(buffer, 2);
        info->name = QString::fromUtf8(buffer.data() + kServerInitFixedSize, namelen);
    }
    if (consumed != nullptr) *consumed = kServerInitFixedSize + namelen;
    return true;
}

// --- Initial outbound configuration ---------------------------------

QByteArray buildSetEncodings()
{
    // Type byte + 1 padding + u16 number + (RLE, RAW, DesktopSize).
    // Order matters: AMT prefers encodings in the order listed, and RAW
    // must be present.
    QByteArray b;
    b.append(char(MsgSetEncodings));
    b.append(char(0x00));
    b.append(pack16Be(3));
    b.append(pack32Be(static_cast<quint32>(EncRle)));
    b.append(pack32Be(static_cast<quint32>(EncRaw)));
    b.append(pack32Be(static_cast<quint32>(EncDesktopSize)));
    return b;
}

QByteArray buildFramebufferUpdateRequest(bool incremental, quint16 x, quint16 y,
                                          quint16 w, quint16 h)
{
    QByteArray b;
    b.append(char(MsgFramebufferUpdateRequest));
    b.append(char(incremental ? 1 : 0));
    b.append(pack16Be(x));
    b.append(pack16Be(y));
    b.append(pack16Be(w));
    b.append(pack16Be(h));
    return b;
}

// --- Input frames ----------------------------------------------------

QByteArray buildKeyEvent(quint32 keysym, bool down)
{
    QByteArray b;
    b.append(char(MsgKeyEvent));
    b.append(char(down ? 1 : 0));
    b.append(char(0x00));
    b.append(char(0x00));
    b.append(pack32Be(keysym));
    return b;
}

QByteArray buildPointerEvent(quint8 buttonMask, quint16 x, quint16 y)
{
    QByteArray b;
    b.append(char(MsgPointerEvent));
    b.append(char(buttonMask));
    b.append(pack16Be(x));
    b.append(pack16Be(y));
    return b;
}

// --- Frame parsing ---------------------------------------------------

bool tryParseFrameUpdate(QByteArrayView buffer, FrameUpdateHeader *out, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < 4) return false;
    if (static_cast<unsigned char>(buffer[0]) != MsgFramebufferUpdate) return false;
    if (out != nullptr) out->numRects = read16Be(buffer, 2);
    if (consumed != nullptr) *consumed = 4;
    return true;
}

bool tryParseRectHeader(QByteArrayView buffer, RectHeader *out, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < kRectHeaderSize) return false;
    if (out != nullptr) {
        out->x = read16Be(buffer, 0);
        out->y = read16Be(buffer, 2);
        out->w = read16Be(buffer, 4);
        out->h = read16Be(buffer, 6);
        out->encoding = read32Be(buffer, 8);
    }
    if (consumed != nullptr) *consumed = kRectHeaderSize;
    return true;
}

namespace {

/// Materialise a pixel buffer for `w x h` into a QImage. The image
/// owns its data so the caller can outlive the original byte view.
QImage makeArgbImage(int w, int h, const quint32 *argb)
{
    QImage img(w, h, QImage::Format_ARGB32);
    for (int row = 0; row < h; ++row) {
        quint32 *scan = reinterpret_cast<quint32 *>(img.scanLine(row));
        std::memcpy(scan, argb + row * w, static_cast<size_t>(w) * 4);
    }
    return img;
}

DecodeStatus decodeRaw(QByteArrayView payload, const RectHeader &rect,
                        QImage *out, int *consumed)
{
    const int needed = rect.w * rect.h * kBytesPerPixel;
    if (payload.size() < needed) return DecodeStatus::NeedMore;

    QVector<quint32> argb(rect.w * rect.h);
    const unsigned char *p = reinterpret_cast<const unsigned char *>(payload.data());
    for (int i = 0; i < rect.w * rect.h; ++i) {
        const quint16 v = static_cast<quint16>(p[0]) | (static_cast<quint16>(p[1]) << 8);
        argb[i] = rgb565ToArgb(v);
        p += 2;
    }
    *out = makeArgbImage(rect.w, rect.h, argb.constData());
    *consumed = needed;
    return DecodeStatus::Ok;
}

/// Decode RLE-encoded tile data. Returns true on success and writes
/// pixel count actually decoded. Supports subencodings 0, 1, 128. The
/// `data` view is the inner payload AFTER the 1-byte subencoding byte.
bool decodeRleInner(QByteArrayView data, quint8 subenc,
                     const RectHeader &rect, QVector<quint32> *argb)
{
    argb->resize(rect.w * rect.h);
    const int s = rect.w * rect.h;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(data.data());
    const unsigned char *end = p + data.size();

    auto pull2 = [&](quint16 *out) -> bool {
        if (end - p < 2) return false;
        *out = static_cast<quint16>(p[0]) | (static_cast<quint16>(p[1]) << 8);
        p += 2;
        return true;
    };

    if (subenc == 0) {
        // RAW pixels inside the RLE block (one tile).
        if ((end - p) < s * kBytesPerPixel) return false;
        for (int i = 0; i < s; ++i) {
            quint16 v;
            if (!pull2(&v)) return false;
            (*argb)[i] = rgb565ToArgb(v);
        }
        return true;
    }

    if (subenc == 1) {
        // Solid color — one RGB565 pixel applied to the whole tile.
        quint16 v;
        if (!pull2(&v)) return false;
        const quint32 c = rgb565ToArgb(v);
        for (int i = 0; i < s; ++i) (*argb)[i] = c;
        return true;
    }

    if (subenc == 128) {
        // Single-color RLE: color (2 bytes) + run-length (one or more
        // bytes; 255 means continue).
        int i = 0;
        while (i < s) {
            quint16 v;
            if (!pull2(&v)) return false;
            const quint32 c = rgb565ToArgb(v);

            int run = 1;
            for (;;) {
                if (p == end) return false;
                const int delta = *p++;
                run += delta;
                if (delta != 0xFF) break;
            }
            if (i + run > s) run = s - i;
            for (int k = 0; k < run; ++k) (*argb)[i + k] = c;
            i += run;
        }
        return true;
    }

    return false; // unsupported subencoding for now
}

DecodeStatus decodeRle(QByteArrayView payload, const RectHeader &rect,
                        QImage *out, int *consumed)
{
    // Encoding layout: u32 dataLen, then dataLen bytes which begin with
    // a ZLib uncompressed marker [byte0=0, u16 len=dataLen-5] and then
    // the actual tile data starting with the subencoding byte.
    if (payload.size() < 4) return DecodeStatus::NeedMore;
    const qint32 dataLen = (static_cast<qint32>(static_cast<unsigned char>(payload[0])) << 24)
                          | (static_cast<qint32>(static_cast<unsigned char>(payload[1])) << 16)
                          | (static_cast<qint32>(static_cast<unsigned char>(payload[2])) << 8)
                          | static_cast<qint32>(static_cast<unsigned char>(payload[3]));
    if (dataLen < 0) return DecodeStatus::Malformed;
    if (payload.size() < 4 + dataLen) return DecodeStatus::NeedMore;
    // Need at least the 5-byte uncompressed marker + 1-byte subencoding.
    if (dataLen < 6) return DecodeStatus::Malformed;

    const QByteArrayView block = payload.sliced(4, dataLen);

    // Look for the ZLib-uncompressed marker so we can stay on the
    // simple path. A compressed block sets the first byte != 0 and
    // would require inflate(); we don't enable that in v1.
    if (static_cast<unsigned char>(block[0]) == 0x00
        && (static_cast<quint16>(static_cast<unsigned char>(block[1]))
            | (static_cast<quint16>(static_cast<unsigned char>(block[2])) << 8))
                == static_cast<quint16>(dataLen - 5)) {
        const quint8 sub = static_cast<unsigned char>(block[5]);
        QByteArrayView inner = block.sliced(6, dataLen - 6);
        QVector<quint32> argb;
        if (!decodeRleInner(inner, sub, rect, &argb)) {
            return DecodeStatus::UnsupportedSubencoding;
        }
        *out = makeArgbImage(rect.w, rect.h, argb.constData());
        *consumed = 4 + dataLen;
        return DecodeStatus::Ok;
    }

    // Compressed RLE data — distinct from "need more bytes". The caller
    // tears the connection down so the user gets a clear error.
    return DecodeStatus::UnsupportedSubencoding;
}

} // namespace

DecodeStatus tryDecodeRect(QByteArrayView payload, const RectHeader &rect,
                            DecodedRect *out, int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (out == nullptr) return DecodeStatus::Malformed;
    out->image = QImage{};
    out->isDesktopSize = false;

    if (rect.encoding == EncDesktopSize) {
        out->isDesktopSize = true;
        return DecodeStatus::Ok;
    }
    if (rect.w == 0 || rect.h == 0) {
        return DecodeStatus::Ok;
    }
    if (rect.w > 4096 || rect.h > 4096) return DecodeStatus::Malformed;

    if (rect.encoding == EncRaw) {
        int n = 0;
        const DecodeStatus s = decodeRaw(payload, rect, &out->image, &n);
        if (s == DecodeStatus::Ok && consumed != nullptr) *consumed = n;
        return s;
    }
    if (rect.encoding == EncRle) {
        int n = 0;
        const DecodeStatus s = decodeRle(payload, rect, &out->image, &n);
        if (s == DecodeStatus::Ok && consumed != nullptr) *consumed = n;
        return s;
    }
    return DecodeStatus::UnsupportedEncoding;
}

} // namespace meshcommander::kvm
