// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "asciicast_recorder.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace qumesh::app {

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
    m_timer.start();
    return true;
}

void AsciicastRecorder::pushBytes(const QByteArray &bytes)
{
    if (!m_file.isOpen() || bytes.isEmpty()) return;

    // Asciicast expects UTF-8 text, not arbitrary bytes; ill-formed UTF-8
    // gets replaced with U+FFFD by fromUtf8 so the file stays a valid
    // JSON stream even if the SOL stream slips a stray binary byte.
    const QString text = QString::fromUtf8(bytes);

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
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

} // namespace qumesh::app
