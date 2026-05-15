// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QImage>
#include <QString>
#include <QVector>

namespace qumesh::kvm {

/// Raw-deflate (-windowBits=-15) inflate stream. The KVM RLE encoding
/// uses a single zlib stream that spans the whole session — successive
/// compressed RLE blocks share a sliding window, so the state must
/// survive across rect boundaries. Construct one per session and
/// reset on (re)negotiation.
class InflateStream
{
public:
    InflateStream();
    ~InflateStream();
    InflateStream(const InflateStream &) = delete;
    InflateStream &operator=(const InflateStream &) = delete;

    /// Reset to a fresh state, discarding any window.
    void reset();

    /// Decompress `in` (the raw block bytes after the dataLen header)
    /// into `out`. State persists across calls so back-references into
    /// prior blocks resolve correctly. Returns false on a zlib error.
    bool inflate(QByteArrayView in, QByteArray *out);

private:
    struct Impl;
    Impl *m_impl;
};

/// RFB-variant client → server message types.
enum ClientMsg : quint8 {
    MsgSetPixelFormat            = 0,
    MsgSetEncodings              = 2,
    MsgFramebufferUpdateRequest  = 3,
    MsgKeyEvent                  = 4,
    MsgPointerEvent              = 5,
    MsgClientCutText             = 6,
};

/// Server → client message types we care about.
enum ServerMsg : quint8 {
    MsgFramebufferUpdate         = 0,
    MsgBell                      = 2,
    MsgServerCutText             = 3,
};

/// RFB-variant pixel encodings used by AMT.
enum Encoding : qint32 {
    EncRaw          = 0,
    EncRle          = 16,
    EncInband       = 1092,                              // DesktopInband
    EncDesktopSize  = static_cast<qint32>(0xFFFFFF21),   // -223
};

/// Parsed ServerInit: width, height, plus the desktop name. The pixel
/// format fields are deliberately discarded — we negotiate RGB565 below.
struct ServerInit {
    quint16 width  = 0;
    quint16 height = 0;
    QString name;
};

/// Parsed FramebufferUpdate header.
struct FrameUpdateHeader {
    quint16 numRects = 0;
};

/// Per-rect header inside a FramebufferUpdate.
struct RectHeader {
    quint16 x = 0;
    quint16 y = 0;
    quint16 w = 0;
    quint16 h = 0;
    qint32 encoding = 0;
};

/// Result of decoding one rect's payload: the pixels in RGBA8888 ready
/// to be uploaded into a QImage tile. For DesktopSize this is empty and
/// the caller should treat (w, h) as the new desktop dimensions instead.
struct DecodedRect {
    QImage image;            // empty for DesktopSize
    bool isDesktopSize = false;
};

// --- Handshake -------------------------------------------------------

/// Look for the 12-byte ProtocolVersion string. Always emits "RFB 003.008".
QByteArray buildVersionResponse();

/// Parse the 1-byte count + n bytes of security types. Currently we
/// always reply with "None" (type 1) since redirection-digest already
/// authenticated us. Returns total bytes consumed (>= 2). Returns 0 if
/// more bytes are needed.
int tryParseSecurityTypes(QByteArrayView buffer);

/// Reply chosen security type. Always 1 (None).
QByteArray buildSecurityChoice();

/// Parse the 4-byte SecurityResult. Returns true on success (status=0),
/// false if status != 0 or more bytes needed.
bool tryParseSecurityResult(QByteArrayView buffer, int *consumed);

/// Build the 1-byte ClientInit (shared-flag = 1).
QByteArray buildClientInit();

/// Parse ServerInit: 24-byte header + namelen + namebytes.
bool tryParseServerInit(QByteArrayView buffer, ServerInit *info, int *consumed);

// --- Initial outbound configuration ---------------------------------

/// Build the 20-byte SetPixelFormat (RFB msg type 0) locking the
/// server into RGB565: bpp=16, depth=16, little-endian, true-color,
/// red shifted 11, green shifted 5, blue shifted 0. Our decoder
/// assumes RGB565 everywhere; without this message AMT picks its
/// own default per firmware build and on some machines the pixels
/// we receive don't match what we decode (resulting in a black or
/// scrambled framebuffer even though dimensions come through fine).
QByteArray buildSetPixelFormat();

/// Build SetEncodings with the encodings we support. AMT requires RAW
/// to always be present; we add RLE and DesktopSize.
QByteArray buildSetEncodings();

/// Build an Intel KvmExt command, smuggled as a ClientCutText frame
/// (message type 6). The legacy uses this to flip AMT into modes the
/// stock RFB protocol doesn't expose — most importantly, command `4`
/// with value `0` disables the zlib compression on RLE blocks, which
/// keeps every block in the uncompressed-marker path our decoder
/// handles cleanly. Older AMT firmwares that don't recognize KvmExt
/// silently ignore the frame (it looks like a clipboard write).
QByteArray buildKvmExtCmd(quint8 cmd, quint8 value);

/// Build FramebufferUpdateRequest. `incremental=0` triggers a full
/// repaint; `1` requests only the changed regions since the last frame.
QByteArray buildFramebufferUpdateRequest(bool incremental, quint16 x, quint16 y,
                                          quint16 w, quint16 h);

// --- Input frames ----------------------------------------------------

/// Build a KeyEvent. `keysym` follows the X11 keysym convention (mostly
/// ASCII + XK_* names from keysymdef.h); `down=true` is a key-down.
QByteArray buildKeyEvent(quint32 keysym, bool down);

/// Build a PointerEvent. `buttonMask` is a bitmask of buttons currently
/// held: bit 0 = left, bit 1 = middle, bit 2 = right, bits 3-4 = wheel.
QByteArray buildPointerEvent(quint8 buttonMask, quint16 x, quint16 y);

// --- Frame parsing ---------------------------------------------------

/// Parse the FramebufferUpdate header (tag byte + padding + numRects).
/// On success consumes 4 bytes.
bool tryParseFrameUpdate(QByteArrayView buffer, FrameUpdateHeader *out, int *consumed);

/// Parse a single rect header (12 bytes). Does NOT consume any payload.
bool tryParseRectHeader(QByteArrayView buffer, RectHeader *out, int *consumed);

/// Outcome of `tryDecodeRect` — distinguishes "wait for more bytes"
/// from terminal "this is something we can't decode and the session
/// should give up".
enum class DecodeStatus {
    Ok,
    NeedMore,
    UnsupportedEncoding,
    UnsupportedSubencoding,
    Malformed,
};

/// Decode a single rect's payload. Caller passes the rect header and a
/// view that starts immediately after the rect header. On Ok, writes
/// pixels into `out->image` and sets `*consumed` to the number of
/// payload bytes used.
///
/// Supported encodings (bpp=2 / RGB565):
///   - 0 (RAW)
///   - 16 (RLE), subencodings 0, 1, 128. The uncompressed-ZLib-marker
///     path requires no inflate state. Compressed blocks decode when a
///     non-null `InflateStream*` is supplied.
///   - 0xFFFFFF21 (DesktopSize) — returns isDesktopSize=true, no image.
DecodeStatus tryDecodeRect(QByteArrayView payload, const RectHeader &rect,
                            DecodedRect *out, int *consumed,
                            InflateStream *inflate = nullptr);

/// Helper: convert an RGB565 u16 into a 32-bit ARGB pixel.
quint32 rgb565ToArgb(quint16 v);

} // namespace qumesh::kvm
