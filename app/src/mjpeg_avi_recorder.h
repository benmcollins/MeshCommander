// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

namespace qumesh::app {

/// Streaming Motion-JPEG AVI writer.
///
/// AVI is a four-decades-old RIFF format; every desktop video player on
/// macOS and Windows still opens it. MJPEG packs the file as a sequence
/// of independent JPEG frames (one per `pushFrame`), which keeps the
/// muxer trivial: no codec state, no key/B-frame distinction, every
/// frame is a keyframe.
///
/// Pass: write headers with placeholder `totalFrames` / RIFF size →
/// stream JPEG frames into the `movi` LIST → on `stop` write the `idx1`
/// index, then seek back and patch the deferred fields.
class MjpegAviRecorder : public QObject
{
    Q_OBJECT

public:
    explicit MjpegAviRecorder(QObject *parent = nullptr);
    ~MjpegAviRecorder() override;

    [[nodiscard]] bool isRecording() const { return m_file.isOpen(); }
    [[nodiscard]] int frameCount() const { return m_frames.size(); }

    /// Open `path` and write the AVI header for a stream of `width`×`height`
    /// JPEG frames at `fps`. Returns false if any of the args are invalid
    /// or the file can't be opened.
    bool start(const QString &path, int width, int height, int fps);

    /// JPEG-encode `image` at `quality` (1..100; 85 is a reasonable
    /// default for KVM captures) and append it to the `movi` list.
    /// `image` is auto-resized if it doesn't match the start dimensions —
    /// some KVM hosts emit a desktop resize mid-stream, which we'd
    /// rather flatten than drop the recording.
    bool pushFrame(const QImage &image, int quality = 85);

    /// Write the index, patch the header fields, and close.
    void stop();

private:
    struct Entry {
        quint32 offset; // relative to movi data (after the LIST type "movi")
        quint32 size;
    };

    bool writeRiffHeader();
    void writeIndex();

    QFile m_file;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 5;
    qint64 m_moviListStart = 0;    // file position of the movi "LIST" fourcc;
                                   // idx1 chunk offsets are measured from here
    qint64 m_moviSizeOffset = 0;   // movi LIST size field (patched at stop)
    qint64 m_aviHeaderTotalFramesOffset = 0;
    qint64 m_strhLengthOffset = 0;
    QVector<Entry> m_frames;
};

} // namespace qumesh::app
