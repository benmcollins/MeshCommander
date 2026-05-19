// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "mjpeg_mov_recorder.h"

#include <QBuffer>
#include <QByteArray>
#include <QtEndian>

namespace qumesh::app {

namespace {

// MOV / ISO-BMFF atoms use big-endian sizes and fourccs. Helpers here
// either write directly to the file (for streaming `mdat`) or to a
// `QByteArray` (for the moov box, which is materialised in memory and
// flushed at the end of the recording).

void appendU16(QByteArray &out, quint16 v)
{
    quint16 be = qToBigEndian(v);
    out.append(reinterpret_cast<const char *>(&be), 2);
}

void appendU32(QByteArray &out, quint32 v)
{
    quint32 be = qToBigEndian(v);
    out.append(reinterpret_cast<const char *>(&be), 4);
}

void appendBytes(QByteArray &out, const char *p, int n) { out.append(p, n); }
void appendFourCC(QByteArray &out, const char (&id)[5]) { out.append(id, 4); }

void writeU32(QFile &f, quint32 v)
{
    quint32 be = qToBigEndian(v);
    f.write(reinterpret_cast<const char *>(&be), 4);
}

void writeFourCC(QFile &f, const char (&id)[5]) { f.write(id, 4); }

void patchU32At(QFile &f, qint64 offset, quint32 v)
{
    const qint64 here = f.pos();
    f.seek(offset);
    writeU32(f, v);
    f.seek(here);
}

/// Build a complete atom from a payload that's already been
/// serialised, prefixing it with the 8-byte size + fourcc header.
QByteArray atom(const char (&fourcc)[5], const QByteArray &payload)
{
    QByteArray out;
    out.reserve(8 + payload.size());
    appendU32(out, static_cast<quint32>(8 + payload.size()));
    appendFourCC(out, fourcc);
    out.append(payload);
    return out;
}

QByteArray buildFtypPayload()
{
    QByteArray p;
    appendFourCC(p, "qt  ");          // major_brand
    appendU32(p, 0x00000200);         // minor_version (2.0; what ffmpeg writes)
    appendFourCC(p, "qt  ");          // compatible_brands[0]
    return p;
}

QByteArray buildMvhdPayload(quint32 timescale, quint32 duration)
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags (v0)
    appendU32(p, 0);                  // creation_time
    appendU32(p, 0);                  // modification_time
    appendU32(p, timescale);
    appendU32(p, duration);
    appendU32(p, 0x00010000);         // preferred rate = 1.0 (16.16 fixed)
    appendU16(p, 0x0100);             // preferred volume = 1.0 (8.8 fixed)
    p.append(QByteArray(10, '\0'));   // reserved
    // matrix: identity (3×3, 16.16 fixed except last column = 2.30 fixed)
    const quint32 mtx[9] = { 0x00010000, 0, 0,
                              0, 0x00010000, 0,
                              0, 0, 0x40000000 };
    for (quint32 v : mtx) appendU32(p, v);
    p.append(QByteArray(24, '\0'));   // pre_defined[6]
    appendU32(p, 2);                  // next_track_id (1 used; 2 next free)
    return p;
}

QByteArray buildTkhdPayload(quint32 trackId, quint32 duration,
                             int width, int height)
{
    QByteArray p;
    appendU32(p, 0x00000007);         // version=0, flags=ENABLED|IN_MOVIE|IN_PREVIEW
    appendU32(p, 0);                  // creation_time
    appendU32(p, 0);                  // modification_time
    appendU32(p, trackId);
    appendU32(p, 0);                  // reserved
    appendU32(p, duration);
    p.append(QByteArray(8, '\0'));    // reserved[2]
    appendU16(p, 0);                  // layer
    appendU16(p, 0);                  // alternate_group
    appendU16(p, 0);                  // volume (0 for video)
    appendU16(p, 0);                  // reserved
    const quint32 mtx[9] = { 0x00010000, 0, 0,
                              0, 0x00010000, 0,
                              0, 0, 0x40000000 };
    for (quint32 v : mtx) appendU32(p, v);
    appendU32(p, static_cast<quint32>(width) << 16);  // width  (16.16 fixed)
    appendU32(p, static_cast<quint32>(height) << 16); // height (16.16 fixed)
    return p;
}

QByteArray buildMdhdPayload(quint32 timescale, quint32 duration)
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 0);                  // creation_time
    appendU32(p, 0);                  // modification_time
    appendU32(p, timescale);
    appendU32(p, duration);
    // language: ISO-639-2/T "und" packed as 5-bit chars - 0x60. und = 0x55C4.
    appendU16(p, 0x55C4);
    appendU16(p, 0);                  // quality (QuickTime extension; 0 fine)
    return p;
}

QByteArray buildHdlrPayload()
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 0);                  // pre_defined (component_type — 0 in mp4)
    appendFourCC(p, "vide");          // handler_type
    p.append(QByteArray(12, '\0'));   // reserved[3]
    // name: null-terminated UTF-8. QuickTime tolerates Pascal-style too;
    // a plain "VideoHandler\0" matches what ffmpeg / Apple SDK both emit.
    static const char kName[] = "VideoHandler";
    appendBytes(p, kName, sizeof(kName)); // includes the trailing NUL
    return p;
}

QByteArray buildVmhdPayload()
{
    QByteArray p;
    appendU32(p, 0x00000001);         // version=0, flags=1 (no_lean_ahead)
    appendU16(p, 0);                  // graphicsmode
    appendU16(p, 0); appendU16(p, 0); appendU16(p, 0); // opcolor[3]
    return p;
}

QByteArray buildDrefPayload()
{
    // Single 'url ' entry with the 'self_contained' flag set (no payload).
    QByteArray inner;
    appendU32(inner, 0x00000001);     // version=0, flags=1 (self contained)
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 1);                  // entry_count
    p.append(atom("url ", inner));
    return p;
}

QByteArray buildStsdPayloadJpeg(int width, int height)
{
    // VisualSampleEntry for codec 'jpeg'. 78-byte fixed layout after
    // the entry's own size+fourcc header.
    QByteArray entry;
    appendU32(entry, 0);              // reserved[0..3]
    appendU16(entry, 0);              // reserved[4..5]
    appendU16(entry, 1);              // data_reference_index
    appendU16(entry, 0);              // pre_defined
    appendU16(entry, 0);              // reserved
    appendU32(entry, 0); appendU32(entry, 0); appendU32(entry, 0); // pre_defined[3]
    appendU16(entry, static_cast<quint16>(width));
    appendU16(entry, static_cast<quint16>(height));
    appendU32(entry, 0x00480000);     // horizresolution 72 dpi (16.16)
    appendU32(entry, 0x00480000);     // vertresolution  72 dpi
    appendU32(entry, 0);              // reserved
    appendU16(entry, 1);              // frame_count (1 sample per chunk for video)
    // compressorname: 32-byte Pascal-style string (1 length byte + 31 bytes).
    char compressorname[32] = { 0 };
    static const char kName[] = "Motion JPEG";
    compressorname[0] = static_cast<char>(sizeof(kName) - 1);
    memcpy(compressorname + 1, kName, sizeof(kName) - 1);
    entry.append(compressorname, 32);
    appendU16(entry, 24);             // depth
    appendU16(entry, 0xFFFF);         // pre_defined (-1 = no color table)

    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 1);                  // entry_count
    p.append(atom("jpeg", entry));
    return p;
}

QByteArray buildSttsPayload(quint32 sampleCount)
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 1);                  // entry_count
    appendU32(p, sampleCount);        // sample_count
    appendU32(p, 1);                  // sample_delta (1 tick each)
    return p;
}

QByteArray buildStscPayload(quint32 sampleCount)
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 1);                  // entry_count
    appendU32(p, 1);                  // first_chunk
    appendU32(p, sampleCount);        // samples_per_chunk
    appendU32(p, 1);                  // sample_description_index
    return p;
}

QByteArray buildStszPayload(const QVector<quint32> &sampleSizes)
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 0);                  // sample_size (0 = per-sample sizes follow)
    appendU32(p, static_cast<quint32>(sampleSizes.size()));
    for (quint32 s : sampleSizes) appendU32(p, s);
    return p;
}

QByteArray buildStcoPayload(quint32 chunkOffset)
{
    QByteArray p;
    appendU32(p, 0);                  // version+flags
    appendU32(p, 1);                  // entry_count
    appendU32(p, chunkOffset);
    return p;
}

} // namespace

MjpegMovRecorder::MjpegMovRecorder(QObject *parent) : QObject(parent) {}

MjpegMovRecorder::~MjpegMovRecorder() { stop(); }

bool MjpegMovRecorder::start(const QString &path, int width, int height, int fps)
{
    if (m_file.isOpen()) stop();
    if (path.isEmpty() || width <= 0 || height <= 0 || fps <= 0) return false;

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadWrite | QIODevice::Truncate)) return false;

    m_width = width;
    m_height = height;
    m_fps = fps;
    m_frames.clear();

    if (!writeFtyp() || !beginMdat()) {
        m_file.close();
        return false;
    }
    return true;
}

bool MjpegMovRecorder::writeFtyp()
{
    const QByteArray box = atom("ftyp", buildFtypPayload());
    return m_file.write(box) == box.size();
}

bool MjpegMovRecorder::beginMdat()
{
    // 32-bit-sized mdat box: <size u32 BE><'mdat'><payload…>. The size
    // is patched at stop() once we know how many JPEG bytes followed.
    // If we ever need >4 GiB clips we'd switch to a 64-bit "largesize"
    // header (size==1 followed by a u64), but that's not in scope here
    // — see #241.
    m_mdatSizeOffset = m_file.pos();
    writeU32(m_file, 0);                  // placeholder, patched at stop()
    writeFourCC(m_file, "mdat");
    m_mdatPayloadStart = m_file.pos();
    return true;
}

bool MjpegMovRecorder::pushFrame(const QImage &image, int quality)
{
    if (!m_file.isOpen() || image.isNull()) return false;

    QImage frame = image;
    if (frame.width() != m_width || frame.height() != m_height) {
        frame = frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation);
    }
    if (frame.format() != QImage::Format_RGB888) {
        frame = frame.convertToFormat(QImage::Format_RGB888);
    }

    QByteArray jpeg;
    {
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        if (!frame.save(&buf, "JPEG", quality)) return false;
    }

    Sample s;
    s.offset = static_cast<quint32>(m_file.pos()); // absolute file offset
    s.size   = static_cast<quint32>(jpeg.size());
    if (m_file.write(jpeg) != jpeg.size()) return false;
    m_frames.append(s);
    return true;
}

void MjpegMovRecorder::writeMoov()
{
    const quint32 nSamples = static_cast<quint32>(m_frames.size());

    // Sample timescale = fps so each sample lasts exactly one tick;
    // movie timescale matches for a single-track file.
    const quint32 timescale = static_cast<quint32>(m_fps);
    const quint32 duration  = nSamples;

    QVector<quint32> sampleSizes;
    sampleSizes.reserve(m_frames.size());
    for (const Sample &s : m_frames) sampleSizes.append(s.size);

    // stbl
    QByteArray stbl;
    stbl.append(atom("stsd", buildStsdPayloadJpeg(m_width, m_height)));
    stbl.append(atom("stts", buildSttsPayload(nSamples)));
    stbl.append(atom("stsc", buildStscPayload(nSamples)));
    stbl.append(atom("stsz", buildStszPayload(sampleSizes)));
    stbl.append(atom("stco", buildStcoPayload(
        nSamples == 0 ? 0u
                       : static_cast<quint32>(m_mdatPayloadStart))));

    // minf
    QByteArray dinf;
    dinf.append(atom("dref", buildDrefPayload()));

    QByteArray minf;
    minf.append(atom("vmhd", buildVmhdPayload()));
    minf.append(atom("dinf", dinf));
    minf.append(atom("stbl", stbl));

    // mdia
    QByteArray mdia;
    mdia.append(atom("mdhd", buildMdhdPayload(timescale, duration)));
    mdia.append(atom("hdlr", buildHdlrPayload()));
    mdia.append(atom("minf", minf));

    // trak
    QByteArray trak;
    trak.append(atom("tkhd", buildTkhdPayload(1, duration, m_width, m_height)));
    trak.append(atom("mdia", mdia));

    // moov
    QByteArray moov;
    moov.append(atom("mvhd", buildMvhdPayload(timescale, duration)));
    moov.append(atom("trak", trak));

    const QByteArray moovBox = atom("moov", moov);
    m_file.write(moovBox);
}

void MjpegMovRecorder::stop()
{
    if (!m_file.isOpen()) return;

    // Patch mdat size before writing moov: size covers the mdat box's
    // own 8-byte header plus the JPEG payload bytes that followed.
    const qint64 mdatEnd = m_file.pos();
    patchU32At(m_file, m_mdatSizeOffset,
               static_cast<quint32>(mdatEnd - m_mdatSizeOffset));

    writeMoov();

    m_file.flush();
    m_file.close();
    m_frames.clear();
}

} // namespace qumesh::app
