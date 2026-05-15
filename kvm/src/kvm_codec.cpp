// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvm/kvm_codec.h"

#include <QLoggingCategory>
#include <zlib.h>

Q_LOGGING_CATEGORY(lcKvmCodec, "qumesh.kvm.codec")

namespace qumesh::kvm {

struct InflateStream::Impl {
    z_stream zs{};
    bool initialized = false;

    Impl()
    {
        zs.zalloc = Z_NULL;
        zs.zfree  = Z_NULL;
        zs.opaque = Z_NULL;
        // windowBits = 15 means "expect a zlib header (0x78 0x9c …)".
        // Modern AMT firmware ships the RLE-16 payload as a standard
        // zlib stream — the legacy NW.js code used -15 (raw deflate)
        // and its JS ZLIB port silently tolerated the header; native
        // zlib does not, so we have to declare it.
        if (inflateInit2(&zs, 15) == Z_OK) initialized = true;
    }
    ~Impl() { if (initialized) inflateEnd(&zs); }
    void reset() { if (initialized) inflateReset(&zs); }
};

InflateStream::InflateStream() : m_impl(new Impl) {}
InflateStream::~InflateStream() { delete m_impl; }

void InflateStream::reset() { m_impl->reset(); }

bool InflateStream::inflate(QByteArrayView in, QByteArray *out)
{
    if (!m_impl->initialized || out == nullptr) return false;
    out->clear();
    m_impl->zs.next_in = const_cast<Bytef *>(
        reinterpret_cast<const Bytef *>(in.data()));
    m_impl->zs.avail_in = static_cast<uInt>(in.size());

    // A single rect's inflated tile is bounded by 64×64 × 2 bytes (8 KB)
    // plus the subencoding byte; 16 KB chunks are more than enough.
    constexpr int kChunk = 16 * 1024;
    QByteArray buf(kChunk, '\0');
    for (;;) {
        m_impl->zs.next_out = reinterpret_cast<Bytef *>(buf.data());
        m_impl->zs.avail_out = static_cast<uInt>(buf.size());
        const int rc = ::inflate(&m_impl->zs, Z_SYNC_FLUSH);
        const int produced = static_cast<int>(buf.size() - m_impl->zs.avail_out);
        if (produced > 0) out->append(buf.constData(), produced);
        if (rc == Z_STREAM_END) break;
        if (rc != Z_OK && rc != Z_BUF_ERROR) {
            qCWarning(lcKvmCodec) << "zlib inflate rc=" << rc
                                  << "msg=" << (m_impl->zs.msg ? m_impl->zs.msg : "(null)")
                                  << "avail_in=" << m_impl->zs.avail_in
                                  << "produced=" << produced;
            return false;
        }
        // Stop once we've consumed all input AND produced less than a
        // full chunk — the stream has nothing more to emit for now.
        if (m_impl->zs.avail_in == 0 && produced < buf.size()) break;
    }
    return true;
}

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

QByteArray buildKvmExtCmd(quint8 cmd, quint8 value)
{
    // ClientCutText (msg type 6) carrying a magic '\0KvmExtCmd\0' tag
    // plus a 1-byte command and 1-byte value. Length is the big-endian
    // count of bytes after the 8-byte header.
    static constexpr char kTag[] = "\0KvmExtCmd\0";   // 11 bytes
    constexpr int kTagLen = sizeof(kTag) - 1;          // strip trailing nul
    const quint32 bodyLen = static_cast<quint32>(kTagLen + 2);

    QByteArray b;
    b.append(char(6));                                 // type
    b.append(char(0)); b.append(char(0)); b.append(char(0));
    b.append(char((bodyLen >> 24) & 0xFF));
    b.append(char((bodyLen >> 16) & 0xFF));
    b.append(char((bodyLen >> 8) & 0xFF));
    b.append(char(bodyLen & 0xFF));
    b.append(kTag, kTagLen);
    b.append(char(cmd));
    b.append(char(value));
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
/// the decoded pixels into `argb`. Subencodings handled:
///   * 0          — RAW pixels (s × RGB565).
///   * 1          — solid colour (1 × RGB565).
///   * 2 … 16     — packed palette: subenc palette entries followed by
///                  packed pixel indices (1 / 2 / 4 bits per pixel).
///   * 128        — single-colour RLE.
///   * 130 …      — palette RLE: (subenc-128) palette entries followed
///                  by index bytes; high bit on the index means the
///                  next byte(s) extend the run (255 = continue).
/// `data` is the inner payload AFTER the 1-byte subencoding byte.
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

    if (subenc >= 2 && subenc <= 16) {
        // Packed palette. The palette has `subenc` RGB565 entries; the
        // following bytes pack 1/2/4-bit indices MSB-first.
        QVector<quint32> palette(subenc);
        for (int i = 0; i < subenc; ++i) {
            quint16 v;
            if (!pull2(&v)) return false;
            palette[i] = rgb565ToArgb(v);
        }
        int br, bm;
        if (subenc == 2)       { br = 1; bm = 0x01; } // 1bpp, 8 px/byte
        else if (subenc <= 4)  { br = 2; bm = 0x03; } // 2bpp, 4 px/byte
        else                   { br = 4; bm = 0x0F; } // 4bpp, 2 px/byte

        int i = 0;
        while (i < s && p < end) {
            const int v = *p++;
            for (int shift = 8 - br; shift >= 0 && i < s; shift -= br) {
                (*argb)[i++] = palette[(v >> shift) & bm];
            }
        }
        return i >= s;
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

    if (subenc >= 130) {
        // Palette RLE. (subenc - 128) palette entries, then a stream of
        // index bytes. Each index byte's low 7 bits select a palette
        // entry; bit 7 set means the next byte(s) extend a run (sum of
        // bytes, with 255 continuing). Index byte without bit 7 set
        // paints exactly one pixel.
        const int pSize = subenc - 128;
        QVector<quint32> palette(pSize);
        for (int i = 0; i < pSize; ++i) {
            quint16 v;
            if (!pull2(&v)) return false;
            palette[i] = rgb565ToArgb(v);
        }

        int i = 0;
        while (i < s && p < end) {
            const int index = *p++;
            const int paletteIx = index & 0x7F;
            if (paletteIx >= pSize) return false;
            const quint32 c = palette[paletteIx];
            int run = 1;
            if (index & 0x80) {
                for (;;) {
                    if (p == end) return false;
                    const int delta = *p++;
                    run += delta;
                    if (delta != 0xFF) break;
                }
            }
            if (i + run > s) run = s - i;
            for (int k = 0; k < run; ++k) (*argb)[i + k] = c;
            i += run;
        }
        return i >= s;
    }

    return false; // unsupported subencoding (e.g. 17..127, 129)
}

DecodeStatus decodeRle(QByteArrayView payload, const RectHeader &rect,
                        QImage *out, int *consumed,
                        InflateStream *inflate)
{
    // Encoding layout: u32 dataLen, then dataLen bytes which begin with
    // either a "ZLib uncompressed" marker [0x00, u16 len, padding] or a
    // standard raw-deflate stream. Either way the inner payload starts
    // with the subencoding byte once decompressed.
    if (payload.size() < 4) return DecodeStatus::NeedMore;
    const qint32 dataLen = (static_cast<qint32>(static_cast<unsigned char>(payload[0])) << 24)
                          | (static_cast<qint32>(static_cast<unsigned char>(payload[1])) << 16)
                          | (static_cast<qint32>(static_cast<unsigned char>(payload[2])) << 8)
                          | static_cast<qint32>(static_cast<unsigned char>(payload[3]));
    if (dataLen < 0) return DecodeStatus::Malformed;
    if (payload.size() < 4 + dataLen) return DecodeStatus::NeedMore;
    if (dataLen < 1) return DecodeStatus::Malformed;

    const QByteArrayView block = payload.sliced(4, dataLen);

    // Look for the uncompressed marker first — that path requires no
    // inflate state and matches what AMT BIOS firmware tends to emit.
    const bool uncompressedMarker =
        dataLen >= 6
        && static_cast<unsigned char>(block[0]) == 0x00
        && (static_cast<quint16>(static_cast<unsigned char>(block[1]))
            | (static_cast<quint16>(static_cast<unsigned char>(block[2])) << 8))
                == static_cast<quint16>(dataLen - 5);

    if (uncompressedMarker) {
        const quint8 sub = static_cast<unsigned char>(block[5]);
        QByteArrayView inner = block.sliced(6, dataLen - 6);
        QVector<quint32> argb;
        if (!decodeRleInner(inner, sub, rect, &argb)) {
            qCWarning(lcKvmCodec) << "RLE uncompressed: unsupported subencoding"
                                  << sub << "for" << rect.w << "x" << rect.h;
            return DecodeStatus::UnsupportedSubencoding;
        }
        *out = makeArgbImage(rect.w, rect.h, argb.constData());
        *consumed = 4 + dataLen;
        return DecodeStatus::Ok;
    }

    if (inflate == nullptr) {
        // Caller can't decompress — surface as unsupported so the
        // session tears down rather than spinning on "need more".
        return DecodeStatus::UnsupportedSubencoding;
    }

    QByteArray inflated;
    const bool inflateOk = inflate->inflate(block, &inflated);
    if (!inflateOk || inflated.isEmpty()) {
        qCWarning(lcKvmCodec) << "RLE compressed: inflate"
                              << (inflateOk ? "empty" : "FAILED")
                              << "for" << rect.w << "x" << rect.h
                              << "dataLen" << dataLen
                              << "first32" << QByteArray(block.data(),
                                                          qMin<int>(32, block.size())).toHex();
        return DecodeStatus::Malformed;
    }
    const quint8 sub = static_cast<unsigned char>(inflated.at(0));
    QByteArrayView inner(inflated.constData() + 1, inflated.size() - 1);
    QVector<quint32> argb;
    if (!decodeRleInner(inner, sub, rect, &argb)) {
        qCWarning(lcKvmCodec) << "RLE compressed: unsupported subencoding"
                              << sub << "for" << rect.w << "x" << rect.h;
        return DecodeStatus::UnsupportedSubencoding;
    }
    *out = makeArgbImage(rect.w, rect.h, argb.constData());
    *consumed = 4 + dataLen;
    return DecodeStatus::Ok;
}

} // namespace

DecodeStatus tryDecodeRect(QByteArrayView payload, const RectHeader &rect,
                            DecodedRect *out, int *consumed,
                            InflateStream *inflate)
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
        const DecodeStatus s = decodeRle(payload, rect, &out->image, &n, inflate);
        if (s == DecodeStatus::Ok && consumed != nullptr) *consumed = n;
        return s;
    }
    return DecodeStatus::UnsupportedEncoding;
}

} // namespace qumesh::kvm
