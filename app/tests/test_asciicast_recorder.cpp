// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "asciicast_recorder.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using qumesh::app::AsciicastRecorder;

class TestAsciicastRecorder : public QObject
{
    Q_OBJECT

private slots:
    void start_rejectsInvalidArgs();
    void start_writesHeader();
    void pushBytes_writesEventLines();
    void roundtrip_parsesAsAsciicastV2();
};

void TestAsciicastRecorder::start_rejectsInvalidArgs()
{
    AsciicastRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!r.start(QString(), 80, 24, "x"));
    QVERIFY(!r.start(dir.filePath("a.cast"), 0, 24, "x"));
    QVERIFY(!r.start(dir.filePath("a.cast"), 80, 0, "x"));
    QVERIFY(!r.isRecording());
}

void TestAsciicastRecorder::start_writesHeader()
{
    AsciicastRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("hdr.cast");
    QVERIFY(r.start(path, 80, 24, "hello"));
    r.stop();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray firstLine = f.readLine();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(firstLine.trimmed(), &err);
    QCOMPARE(err.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject());
    const QJsonObject hdr = doc.object();
    QCOMPARE(hdr.value("version").toInt(), 2);
    QCOMPARE(hdr.value("width").toInt(), 80);
    QCOMPARE(hdr.value("height").toInt(), 24);
    QCOMPARE(hdr.value("title").toString(), QStringLiteral("hello"));
    QVERIFY(hdr.value("timestamp").toDouble() > 0);
}

void TestAsciicastRecorder::pushBytes_writesEventLines()
{
    AsciicastRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("ev.cast");
    QVERIFY(r.start(path, 80, 24, QString()));
    r.pushBytes(QByteArrayLiteral("hello\r\n"));
    r.pushBytes(QByteArrayLiteral("world"));
    r.stop();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    f.readLine(); // header
    const QByteArray e1 = f.readLine();
    const QByteArray e2 = f.readLine();
    QVERIFY(!e1.isEmpty());
    QVERIFY(!e2.isEmpty());

    const QJsonArray a1 = QJsonDocument::fromJson(e1.trimmed()).array();
    const QJsonArray a2 = QJsonDocument::fromJson(e2.trimmed()).array();
    QCOMPARE(a1.size(), 3);
    QCOMPARE(a1.at(1).toString(), QStringLiteral("o"));
    QCOMPARE(a1.at(2).toString(), QStringLiteral("hello\r\n"));
    QCOMPARE(a2.at(2).toString(), QStringLiteral("world"));
    // Event timestamps are non-decreasing.
    QVERIFY(a2.at(0).toDouble() >= a1.at(0).toDouble());
}

void TestAsciicastRecorder::roundtrip_parsesAsAsciicastV2()
{
    AsciicastRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("rt.cast");
    QVERIFY(r.start(path, 132, 50, QStringLiteral("session")));
    r.pushBytes(QByteArrayLiteral("$ ls\r\n"));
    r.pushBytes(QByteArrayLiteral("a  b  c\r\n"));
    r.stop();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QStringList lines;
    while (!f.atEnd()) lines << QString::fromUtf8(f.readLine()).trimmed();
    QCOMPARE(lines.size(), 3);

    // Every line is valid JSON.
    for (const QString &l : lines) {
        QJsonParseError err{};
        QJsonDocument::fromJson(l.toUtf8(), &err);
        QCOMPARE(err.error, QJsonParseError::NoError);
    }
}

QTEST_MAIN(TestAsciicastRecorder)
#include "test_asciicast_recorder.moc"
