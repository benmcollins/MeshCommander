// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "asciicast_recorder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace qumesh::app {

namespace {

/// Length of an incomplete UTF-8 sequence at the very end of `bytes`,
/// or 0 if the last byte completes its codepoint. Used to peel off
/// the partial tail before handing the rest to `QString::fromUtf8`,
/// so a multi-byte codepoint split across two SOL reads doesn't get
/// each half replaced with U+FFFD. See #288.
int incompleteUtf8TailLen(const QByteArray &bytes)
{
    const int n = bytes.size();
    for (int back = 1; back <= 3 && back <= n; ++back) {
        const uchar b = static_cast<uchar>(bytes[n - back]);
        if ((b & 0x80) == 0) {
            // ASCII — last sequence is complete.
            return 0;
        }
        if ((b & 0xC0) == 0x80) {
            // Continuation byte — the lead byte is further back.
            continue;
        }
        // Lead byte. Determine how many bytes the sequence needs and
        // whether `back` covers them all.
        int needed;
        if      ((b & 0xE0) == 0xC0) needed = 2;
        else if ((b & 0xF0) == 0xE0) needed = 3;
        else if ((b & 0xF8) == 0xF0) needed = 4;
        else                          return 0; // invalid lead; let fromUtf8 substitute
        return (back < needed) ? back : 0;
    }
    return 0;
}

} // namespace

AsciicastRecorder::AsciicastRecorder(QObject *parent) : QObject(parent) {}

AsciicastRecorder::~AsciicastRecorder() { stop(); }

bool AsciicastRecorder::start(const QString &path, int cols, int rows,
                              const QString &title)
{
    if (m_file.isOpen()) stop();
    if (path.isEmpty() || cols <= 0 || rows <= 0) return false;

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QJsonObject header;
    header.insert("version", 2);
    header.insert("width", cols);
    header.insert("height", rows);
    header.insert("timestamp", QDateTime::currentSecsSinceEpoch());
    if (!title.isEmpty()) header.insert("title", title);

    const QByteArray line = QJsonDocument(header).toJson(QJsonDocument::Compact)
                                + '\n';
    if (m_file.write(line) != line.size()) {
        m_file.close();
        return false;
    }
    m_tail.clear();
    m_timer.start();
    return true;
}

void AsciicastRecorder::pushBytes(const QByteArray &bytes)
{
    if (!m_file.isOpen() || bytes.isEmpty()) return;

    // Reattach any partial-codepoint tail held back from the previous
    // call, then peel the new incomplete tail off the end so the next
    // `pushBytes` can stitch it together. Asciicast expects UTF-8
    // text, not arbitrary bytes; ill-formed UTF-8 gets replaced with
    // U+FFFD by fromUtf8 so the file stays a valid JSON stream even
    // if the SOL stream slips a stray binary byte.
    QByteArray buf = m_tail + bytes;
    const int tailLen = incompleteUtf8TailLen(buf);
    m_tail = buf.right(tailLen);
    buf.chop(tailLen);
    if (buf.isEmpty()) return;

    const QString text = QString::fromUtf8(buf);

    QJsonArray event;
    event.append(static_cast<double>(m_timer.nsecsElapsed()) / 1e9);
    event.append(QStringLiteral("o"));
    event.append(text);

    const QByteArray line = QJsonDocument(event).toJson(QJsonDocument::Compact)
                                + '\n';
    m_file.write(line);
    m_file.flush();
}

void AsciicastRecorder::stop()
{
    if (!m_file.isOpen()) {
        m_tail.clear();
        return;
    }
    // Flush any still-buffered tail. Whatever's left is by definition
    // incomplete UTF-8 (otherwise pushBytes would have written it),
    // so fromUtf8 substitutes U+FFFD — the recording closes with a
    // visible "the session ended mid-codepoint" marker instead of
    // silently dropping the bytes.
    if (!m_tail.isEmpty()) {
        QJsonArray event;
        event.append(static_cast<double>(m_timer.nsecsElapsed()) / 1e9);
        event.append(QStringLiteral("o"));
        event.append(QString::fromUtf8(m_tail));
        const QByteArray line = QJsonDocument(event).toJson(QJsonDocument::Compact)
                                    + '\n';
        m_file.write(line);
        m_tail.clear();
    }
    m_file.flush();
    m_file.close();
}

} // namespace qumesh::app
