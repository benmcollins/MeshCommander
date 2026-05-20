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
    void pushBytes_stitchesUtf8AcrossChunks();
    void pushBytes_handlesAllMultibyteSplits();
    void stop_flushesIncompleteTailAsReplacement();
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

// `é` is U+00E9 — two-byte UTF-8 0xC3 0xA9. The pre-#288 implementation
// decoded each half with `fromUtf8` per chunk, so the operator saw
// `��` in their asciicast where they expected a single `é`.
void TestAsciicastRecorder::pushBytes_stitchesUtf8AcrossChunks()
{
    AsciicastRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("split.cast");
    QVERIFY(r.start(path, 80, 24, QString()));

    // Half a codepoint per call — the 0xC3 stays in the tail until
    // the 0xA9 arrives.
    r.pushBytes(QByteArray::fromHex("c3"));
    r.pushBytes(QByteArray::fromHex("a9"));
    r.stop();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    f.readLine(); // header

    // First pushBytes had only the lead byte — nothing emitted.
    // Second pushBytes stitched it together and emitted "é".
    const QByteArray ev = f.readLine();
    QVERIFY(!ev.isEmpty());
    const QJsonArray a = QJsonDocument::fromJson(ev.trimmed()).array();
    QCOMPARE(a.size(), 3);
    QCOMPARE(a.at(2).toString(), QString::fromUtf8("\xc3\xa9"));
    QCOMPARE(a.at(2).toString().size(), 1); // one codepoint, not two FFFDs.

    // Only one event was written (the lead-byte-only call produced
    // nothing).
    QVERIFY(f.atEnd());
}

void TestAsciicastRecorder::pushBytes_handlesAllMultibyteSplits()
{
    // Exercise 2 / 3 / 4-byte sequences split at every internal
    // boundary. Each pair of (whole sequence, split index) should
    // decode to the same single codepoint as the whole sequence.
    struct Case {
        const char *label;
        QByteArray seq;
        QString want;
    };
    const Case cases[] = {
        // U+00E9 é
        { "2b@1",  QByteArray::fromHex("c3a9"),     QString::fromUtf8("\xc3\xa9") },
        // U+20AC € (E2 82 AC)
        { "3b@1",  QByteArray::fromHex("e282ac"),   QString::fromUtf8("\xe2\x82\xac") },
        { "3b@2",  QByteArray::fromHex("e282ac"),   QString::fromUtf8("\xe2\x82\xac") },
        // U+1F600 😀 (F0 9F 98 80)
        { "4b@1",  QByteArray::fromHex("f09f9880"), QString::fromUtf8("\xf0\x9f\x98\x80") },
        { "4b@2",  QByteArray::fromHex("f09f9880"), QString::fromUtf8("\xf0\x9f\x98\x80") },
        { "4b@3",  QByteArray::fromHex("f09f9880"), QString::fromUtf8("\xf0\x9f\x98\x80") },
    };
    const int splitAt[] = { 1, 1, 2, 1, 2, 3 };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        AsciicastRecorder r;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("c.cast"));
        QVERIFY(r.start(path, 80, 24, QString()));
        const QByteArray &seq = cases[i].seq;
        r.pushBytes(seq.left(splitAt[i]));
        r.pushBytes(seq.mid(splitAt[i]));
        r.stop();

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        f.readLine(); // header
        QString glued;
        while (!f.atEnd()) {
            const QByteArray ln = f.readLine();
            if (ln.trimmed().isEmpty()) continue;
            const QJsonArray a = QJsonDocument::fromJson(ln.trimmed()).array();
            if (a.size() == 3) glued += a.at(2).toString();
        }
        QVERIFY2(glued == cases[i].want,
                 qPrintable(QStringLiteral("case %1 (%2)")
                                .arg(cases[i].label)
                                .arg(QString::fromLatin1(seq.toHex()))));
    }
}

void TestAsciicastRecorder::stop_flushesIncompleteTailAsReplacement()
{
    // If the SOL session ends mid-codepoint we'd rather emit a
    // visible U+FFFD than silently drop the trailing bytes — they
    // were on the wire, the operator should see something landed.
    AsciicastRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("truncated.cast");
    QVERIFY(r.start(path, 80, 24, QString()));
    r.pushBytes(QByteArray::fromHex("c3")); // lone lead byte, no continuation.
    r.stop();

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    f.readLine(); // header
    const QByteArray ev = f.readLine();
    QVERIFY(!ev.isEmpty());
    const QJsonArray a = QJsonDocument::fromJson(ev.trimmed()).array();
    QCOMPARE(a.size(), 3);
    QCOMPARE(a.at(2).toString(), QString(QChar(QChar::ReplacementCharacter)));
}

QTEST_MAIN(TestAsciicastRecorder)
#include "test_asciicast_recorder.moc"
