// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "mjpeg_avi_recorder.h"

#include <QBuffer>
#include <QtEndian>

namespace qumesh::app {

namespace {

void writeFourCC(QFile &f, const char (&id)[5])
{
    f.write(id, 4);
}

void writeU32(QFile &f, quint32 v)
{
    quint32 le = qToLittleEndian(v);
    f.write(reinterpret_cast<const char *>(&le), 4);
}

void writeU16(QFile &f, quint16 v)
{
    quint16 le = qToLittleEndian(v);
    f.write(reinterpret_cast<const char *>(&le), 2);
}

void patchU32At(QFile &f, qint64 offset, quint32 v)
{
    const qint64 here = f.pos();
    f.seek(offset);
    quint32 le = qToLittleEndian(v);
    f.write(reinterpret_cast<const char *>(&le), 4);
    f.seek(here);
}

} // namespace

MjpegAviRecorder::MjpegAviRecorder(QObject *parent) : QObject(parent) {}

MjpegAviRecorder::~MjpegAviRecorder() { stop(); }

bool MjpegAviRecorder::start(const QString &path, int width, int height, int fps)
{
    if (m_file.isOpen()) stop();
    if (path.isEmpty() || width <= 0 || height <= 0 || fps <= 0) return false;

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadWrite | QIODevice::Truncate)) return false;

    m_width = width;
    m_height = height;
    m_fps = fps;
    m_frames.clear();

    if (!writeRiffHeader()) {
        m_file.close();
        return false;
    }
    return true;
}

bool MjpegAviRecorder::writeRiffHeader()
{
    // RIFF<size>AVI
    writeFourCC(m_file, "RIFF");
    const qint64 riffSizeOff = m_file.pos();
    writeU32(m_file, 0); // patched at stop()
    writeFourCC(m_file, "AVI ");

    // LIST<size>hdrl
    writeFourCC(m_file, "LIST");
    const qint64 hdrlSizeOff = m_file.pos();
    writeU32(m_file, 0); // hdrl LIST size — patched in just below
    writeFourCC(m_file, "hdrl");

    // avih (AVIMAINHEADER, 56 bytes payload + 8 chunk header = 64 bytes)
    writeFourCC(m_file, "avih");
    writeU32(m_file, 56);
    writeU32(m_file, static_cast<quint32>(1000000 / m_fps)); // microSecPerFrame
    writeU32(m_file, 0);                                     // maxBytesPerSec — fill at stop if we cared
    writeU32(m_file, 0);                                     // paddingGranularity
    writeU32(m_file, 0x10);                                  // flags: AVIF_HASINDEX
    m_aviHeaderTotalFramesOffset = m_file.pos();
    writeU32(m_file, 0);                                     // totalFrames — patched
    writeU32(m_file, 0);                                     // initialFrames
    writeU32(m_file, 1);                                     // streams
    writeU32(m_file, 0);                                     // suggestedBufferSize
    writeU32(m_file, static_cast<quint32>(m_width));
    writeU32(m_file, static_cast<quint32>(m_height));
    writeU32(m_file, 0); writeU32(m_file, 0);                // reserved[4]
    writeU32(m_file, 0); writeU32(m_file, 0);

    // LIST<size>strl
    writeFourCC(m_file, "LIST");
    const qint64 strlSizeOff = m_file.pos();
    writeU32(m_file, 0); // strl LIST size — patched
    writeFourCC(m_file, "strl");

    // strh (AVISTREAMHEADER, 56 bytes payload + 8 chunk header)
    writeFourCC(m_file, "strh");
    writeU32(m_file, 56);
    writeFourCC(m_file, "vids");
    writeFourCC(m_file, "MJPG");
    writeU32(m_file, 0);                  // flags
    writeU16(m_file, 0);                  // priority
    writeU16(m_file, 0);                  // language
    writeU32(m_file, 0);                  // initialFrames
    writeU32(m_file, 1);                  // scale
    writeU32(m_file, static_cast<quint32>(m_fps)); // rate
    writeU32(m_file, 0);                  // start
    m_strhLengthOffset = m_file.pos();
    writeU32(m_file, 0);                  // length — patched
    writeU32(m_file, 0);                  // suggestedBufferSize
    writeU32(m_file, 0xFFFFFFFFu);        // quality (-1)
    writeU32(m_file, 0);                  // sampleSize
    writeU16(m_file, 0); writeU16(m_file, 0); // rcFrame.left, top
    writeU16(m_file, static_cast<quint16>(m_width));
    writeU16(m_file, static_cast<quint16>(m_height));

    // strf (BITMAPINFOHEADER, 40 bytes)
    writeFourCC(m_file, "strf");
    writeU32(m_file, 40);
    writeU32(m_file, 40);                                    // biSize
    writeU32(m_file, static_cast<quint32>(m_width));
    writeU32(m_file, static_cast<quint32>(m_height));
    writeU16(m_file, 1);                                     // biPlanes
    writeU16(m_file, 24);                                    // biBitCount
    writeFourCC(m_file, "MJPG");                             // biCompression
    writeU32(m_file, static_cast<quint32>(m_width * m_height * 3)); // biSizeImage
    writeU32(m_file, 0); writeU32(m_file, 0);                // pels/meter
    writeU32(m_file, 0); writeU32(m_file, 0);                // biClrUsed/Important

    // End of strl.
    const qint64 strlEnd = m_file.pos();
    patchU32At(m_file, strlSizeOff, static_cast<quint32>(strlEnd - (strlSizeOff + 4)));

    // End of hdrl.
    patchU32At(m_file, hdrlSizeOff, static_cast<quint32>(strlEnd - (hdrlSizeOff + 4)));

    // LIST<size>movi — frames go here.
    writeFourCC(m_file, "LIST");
    m_moviSizeOffset = m_file.pos();
    writeU32(m_file, 0); // patched
    writeFourCC(m_file, "movi");
    m_moviDataStart = m_file.pos();
    (void)riffSizeOff; // patched in stop()
    return true;
}

bool MjpegAviRecorder::pushFrame(const QImage &image, int quality)
{
    if (!m_file.isOpen() || image.isNull()) return false;

    QImage frame = image;
    if (frame.width() != m_width || frame.height() != m_height) {
        frame = frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation);
    }
    // QImage expects no alpha for JPEG and AVI viewers expect BGR-like
    // ordering; RGB888 is what QImage encodes JPEG from natively without
    // an extra copy at JPEG-encode time.
    if (frame.format() != QImage::Format_RGB888) {
        frame = frame.convertToFormat(QImage::Format_RGB888);
    }

    QByteArray jpeg;
    {
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        if (!frame.save(&buf, "JPEG", quality)) return false;
    }

    // 00dc<size><jpeg><pad-to-even>
    writeFourCC(m_file, "00dc");
    writeU32(m_file, static_cast<quint32>(jpeg.size()));
    const qint64 frameOffset = m_file.pos() - m_moviDataStart - 8; // start of "00dc"
    m_file.write(jpeg);
    if (jpeg.size() & 1) m_file.write("\x00", 1);

    Entry e;
    e.offset = static_cast<quint32>(frameOffset);
    e.size = static_cast<quint32>(jpeg.size());
    m_frames.append(e);
    return true;
}

void MjpegAviRecorder::writeIndex()
{
    // Patch movi LIST size now that all frames are written.
    const qint64 moviEnd = m_file.pos();
    patchU32At(m_file, m_moviSizeOffset,
               static_cast<quint32>(moviEnd - (m_moviSizeOffset + 4)));

    // idx1: 16 bytes per entry { ckid, flags, offset, size }
    writeFourCC(m_file, "idx1");
    writeU32(m_file, static_cast<quint32>(m_frames.size() * 16));
    for (const Entry &e : m_frames) {
        writeFourCC(m_file, "00dc");
        writeU32(m_file, 0x10); // AVIIF_KEYFRAME
        writeU32(m_file, e.offset);
        writeU32(m_file, e.size);
    }
}

void MjpegAviRecorder::stop()
{
    if (!m_file.isOpen()) return;

    writeIndex();
    const qint64 fileEnd = m_file.pos();

    patchU32At(m_file, 4, static_cast<quint32>(fileEnd - 8)); // RIFF size
    patchU32At(m_file, m_aviHeaderTotalFramesOffset,
               static_cast<quint32>(m_frames.size()));
    patchU32At(m_file, m_strhLengthOffset,
               static_cast<quint32>(m_frames.size()));

    m_file.flush();
    m_file.close();
    m_frames.clear();
}

} // namespace qumesh::app
