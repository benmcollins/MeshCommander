// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

namespace qumesh::app {

/// Streaming Motion-JPEG QuickTime (.mov) writer.
///
/// We previously used MJPEG-in-AVI. The frames decoded fine in VLC and
/// ffplay, but macOS QuickTime Player rendered MJPEG-in-AVI as a black
/// canvas — its AVI demuxer doesn't handle the codec/container pair.
/// Wrapping the same JPEG payload in QuickTime's native MOV container
/// fixes that without introducing a real video codec dependency.
///
/// File layout:
///   ftyp ('qt  ')
///   mdat — appended one JPEG per `pushFrame` (no inner chunk headers);
///          sample sizes and offsets live in the moov table written
///          at `stop()`.
///   moov — built in memory at stop:
///     mvhd / trak / tkhd / mdia / mdhd / hdlr / minf / vmhd /
///     dinf / dref / stbl / stsd / stts / stsc / stsz / stco.
///
/// 32-bit `stco` keeps clips under 4 GiB; the recorder is intended
/// for KVM-evidence captures, not long-form video. If we ever need
/// >4 GiB we'll switch to `co64`.
class MjpegMovRecorder : public QObject
{
    Q_OBJECT

public:
    explicit MjpegMovRecorder(QObject *parent = nullptr);
    ~MjpegMovRecorder() override;

    [[nodiscard]] bool isRecording() const { return m_file.isOpen(); }
    [[nodiscard]] int frameCount() const { return m_frames.size(); }

    /// Open `path` and write the `ftyp` box for a stream of `width`×`height`
    /// JPEG frames at `fps`. Returns false if any of the args are invalid
    /// or the file can't be opened.
    bool start(const QString &path, int width, int height, int fps);

    /// JPEG-encode `image` at `quality` (1..100; 85 is a reasonable
    /// default for KVM captures) and append it to the `mdat` payload.
    /// `image` is auto-resized if it doesn't match the start dimensions —
    /// some KVM hosts emit a desktop resize mid-stream, which we'd
    /// rather flatten than drop the recording.
    bool pushFrame(const QImage &image, int quality = 85);

    /// Patch the mdat box size, then write the moov box, and close.
    void stop();

private:
    struct Sample {
        quint32 size;     // bytes of the JPEG payload
        quint32 offset;   // absolute file offset of the JPEG within mdat
    };

    bool writeFtyp();
    bool beginMdat();
    void writeMoov();

    QFile m_file;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 5;
    qint64 m_mdatSizeOffset = 0;   // file pos of the mdat box's 32-bit size field
    qint64 m_mdatPayloadStart = 0; // first byte after the mdat header
    QVector<Sample> m_frames;
};

} // namespace qumesh::app
