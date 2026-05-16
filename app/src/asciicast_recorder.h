// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QString>

namespace qumesh::app {

/// Streaming asciicast v2 writer for SOL session recordings.
///
/// Format: one JSON header line, then one JSON event array per record:
///
///     {"version":2,"width":80,"height":24,"timestamp":1700000000,...}
///     [0.000,"o","login: "]
///     [1.234,"o","root\r\n"]
///
/// Spec: https://github.com/asciinema/asciinema/blob/develop/doc/asciicast-v2.md
///
/// Each `pushBytes` writes one event line and flushes; if the app
/// crashes mid-recording the prefix is still a valid asciicast.
class AsciicastRecorder : public QObject
{
    Q_OBJECT

public:
    explicit AsciicastRecorder(QObject *parent = nullptr);
    ~AsciicastRecorder() override;

    [[nodiscard]] bool isRecording() const { return m_file.isOpen(); }

    /// Open `path` and write the header. `title` is recorded into the
    /// header's optional "title" field; pass an empty string to omit.
    /// Returns false if the file can't be opened.
    bool start(const QString &path, int cols, int rows, const QString &title);

    /// Append one "o" (output) event with the current timestamp.
    void pushBytes(const QByteArray &bytes);

    /// Flush and close. Safe to call when not recording.
    void stop();

private:
    QFile m_file;
    QElapsedTimer m_timer;
};

} // namespace qumesh::app
