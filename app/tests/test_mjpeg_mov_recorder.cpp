// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "mjpeg_mov_recorder.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <utility>

using qumesh::app::MjpegMovRecorder;

namespace {

// MOV / ISO-BMFF atoms are big-endian.
quint32 beU32(const QByteArray &data, int off)
{
    quint32 v;
    memcpy(&v, data.constData() + off, 4);
    return qFromBigEndian(v);
}

QByteArray fourcc(const QByteArray &data, int off)
{
    return data.mid(off, 4);
}

/// Walk a parent atom body starting at `start` (inclusive) up to `end`
/// (exclusive), find the first child box with `name`, and return its
/// body range as a pair of file offsets [bodyStart, bodyEnd). Returns
/// {-1, -1} on miss. Iteration stops as soon as the first match is
/// hit — the boxes we care about (mdat, moov, trak, mdia, …) are all
/// unique at their level in our writer.
///
/// Named `findBox` rather than the more natural `findChild` because the
/// test class is a `QObject` and `QObject::findChild` would shadow the
/// free function at the call site.
std::pair<int, int> findBox(const QByteArray &data, int start, int end,
                          const QByteArray &name)
{
    int p = start;
    while (p + 8 <= end) {
        const quint32 size = beU32(data, p);
        if (size < 8 || p + int(size) > end) break;
        const QByteArray box = fourcc(data, p + 4);
        const int bodyStart = p + 8;
        const int bodyEnd   = p + int(size);
        if (box == name) return { bodyStart, bodyEnd };
        p = bodyEnd;
    }
    return { -1, -1 };
}

/// Same as findBox, but follows a list of names to descend through
/// the atom tree (parent → child → grandchild …).
std::pair<int, int> findPath(const QByteArray &data, int start, int end,
                         std::initializer_list<const char *> path)
{
    std::pair<int, int> range = { start, end };
    for (const char *name : path) {
        range = findBox(data, range.first, range.second, QByteArray(name));
        if (range.first < 0) return range;
    }
    return range;
}

QImage makeFrame(int w, int h, QColor color)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(color);
    return img;
}

} // namespace

class TestMjpegMovRecorder : public QObject
{
    Q_OBJECT

private slots:
    void start_rejectsInvalidArgs();
    void roundtrip_writesQuickTimeMovStructure();
    void resizedFrame_rescaledToStartDimensions();
};

void TestMjpegMovRecorder::start_rejectsInvalidArgs()
{
    MjpegMovRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!r.start(QString(), 64, 48, 5));
    QVERIFY(!r.start(dir.filePath("a.mov"), 0, 48, 5));
    QVERIFY(!r.start(dir.filePath("a.mov"), 64, 0, 5));
    QVERIFY(!r.start(dir.filePath("a.mov"), 64, 48, 0));
    QVERIFY(!r.isRecording());
}

void TestMjpegMovRecorder::roundtrip_writesQuickTimeMovStructure()
{
    MjpegMovRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clip.mov");
    QVERIFY(r.start(path, 64, 48, 5));

    QVERIFY(r.pushFrame(makeFrame(64, 48, Qt::red)));
    QVERIFY(r.pushFrame(makeFrame(64, 48, Qt::green)));
    QVERIFY(r.pushFrame(makeFrame(64, 48, Qt::blue)));
    QCOMPARE(r.frameCount(), 3);
    r.stop();
    QVERIFY(!r.isRecording());

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray data = f.readAll();
    QVERIFY(data.size() > 100);

    // ftyp — first box, brand 'qt  '.
    QCOMPARE(fourcc(data, 4), QByteArray("ftyp"));
    QCOMPARE(fourcc(data, 8), QByteArray("qt  "));

    // mdat — present, and its payload starts with a JPEG SOI marker.
    auto mdat = findBox(data, 0, data.size(), "mdat");
    QVERIFY(mdat.first > 0);
    QCOMPARE(quint8(data.at(mdat.first)),     quint8(0xFF));
    QCOMPARE(quint8(data.at(mdat.first + 1)), quint8(0xD8));

    // moov → trak → mdia → minf → stbl.
    auto moov = findBox(data, 0, data.size(), "moov");
    QVERIFY(moov.first > 0);
    auto stbl = findPath(data, moov.first, moov.second,
                          {"trak", "mdia", "minf", "stbl"});
    QVERIFY(stbl.first > 0);

    // stsd entry codec is 'jpeg'. Inside stsd: 4 bytes version+flags,
    // 4 bytes entry_count, then the entry: 4 bytes size, 4 bytes fourcc.
    auto stsd = findBox(data, stbl.first, stbl.second, "stsd");
    QVERIFY(stsd.first > 0);
    QCOMPARE(beU32(data, stsd.first), quint32(0));   // version+flags
    QCOMPARE(beU32(data, stsd.first + 4), quint32(1)); // entry_count
    QCOMPARE(fourcc(data, stsd.first + 8 + 4), QByteArray("jpeg"));

    // stts: one entry with sample_count == 3, sample_delta == 1.
    auto stts = findBox(data, stbl.first, stbl.second, "stts");
    QVERIFY(stts.first > 0);
    QCOMPARE(beU32(data, stts.first + 4),  quint32(1));   // entry_count
    QCOMPARE(beU32(data, stts.first + 8),  quint32(3));   // sample_count
    QCOMPARE(beU32(data, stts.first + 12), quint32(1));   // delta (1 tick @ fps timescale)

    // stsz: 3 sample sizes, each > 0.
    auto stsz = findBox(data, stbl.first, stbl.second, "stsz");
    QVERIFY(stsz.first > 0);
    QCOMPARE(beU32(data, stsz.first + 4),  quint32(0));   // sample_size (0 → per-sample table)
    QCOMPARE(beU32(data, stsz.first + 8),  quint32(3));   // sample_count
    for (int i = 0; i < 3; ++i) {
        QVERIFY(beU32(data, stsz.first + 12 + i * 4) > 0);
    }

    // stco: one chunk, offset points into mdat payload (specifically,
    // the start of the first JPEG sample, which is mdat's body byte 0).
    auto stco = findBox(data, stbl.first, stbl.second, "stco");
    QVERIFY(stco.first > 0);
    QCOMPARE(beU32(data, stco.first + 4), quint32(1));   // entry_count
    const quint32 chunkOff = beU32(data, stco.first + 8);
    QCOMPARE(int(chunkOff), mdat.first);

    // The byte the chunk offset resolves to is the JPEG SOI marker.
    QCOMPARE(quint8(data.at(int(chunkOff))),     quint8(0xFF));
    QCOMPARE(quint8(data.at(int(chunkOff) + 1)), quint8(0xD8));
}

void TestMjpegMovRecorder::resizedFrame_rescaledToStartDimensions()
{
    MjpegMovRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(r.start(dir.filePath("rs.mov"), 32, 32, 5));
    // Push a differently-sized frame — recorder should rescale, not reject.
    QVERIFY(r.pushFrame(makeFrame(64, 48, Qt::red)));
    QCOMPARE(r.frameCount(), 1);
    r.stop();
}

QTEST_MAIN(TestMjpegMovRecorder)
#include "test_mjpeg_mov_recorder.moc"
