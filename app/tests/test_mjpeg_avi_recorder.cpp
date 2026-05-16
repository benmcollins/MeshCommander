// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "mjpeg_avi_recorder.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

using qumesh::app::MjpegAviRecorder;

namespace {

// Read a little-endian u32 from `data` at `off`.
quint32 leU32(const QByteArray &data, int off)
{
    quint32 v;
    memcpy(&v, data.constData() + off, 4);
    return qFromLittleEndian(v);
}

QByteArray fourcc(const QByteArray &data, int off)
{
    return data.mid(off, 4);
}

QImage makeFrame(int w, int h, QColor color)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(color);
    return img;
}

} // namespace

class TestMjpegAviRecorder : public QObject
{
    Q_OBJECT

private slots:
    void start_rejectsInvalidArgs();
    void roundtrip_writesRiffAviStructure();
    void resizedFrame_rescaledToStartDimensions();
};

void TestMjpegAviRecorder::start_rejectsInvalidArgs()
{
    MjpegAviRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!r.start(QString(), 64, 48, 5));
    QVERIFY(!r.start(dir.filePath("a.avi"), 0, 48, 5));
    QVERIFY(!r.start(dir.filePath("a.avi"), 64, 0, 5));
    QVERIFY(!r.start(dir.filePath("a.avi"), 64, 48, 0));
    QVERIFY(!r.isRecording());
}

void TestMjpegAviRecorder::roundtrip_writesRiffAviStructure()
{
    MjpegAviRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clip.avi");
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

    // RIFF<size>AVI
    QCOMPARE(fourcc(data, 0), QByteArray("RIFF"));
    QCOMPARE(fourcc(data, 8), QByteArray("AVI "));
    // RIFF size equals fileSize - 8.
    QCOMPARE(leU32(data, 4), quint32(data.size() - 8));

    // LIST hdrl @ 12.
    QCOMPARE(fourcc(data, 12), QByteArray("LIST"));
    QCOMPARE(fourcc(data, 20), QByteArray("hdrl"));

    // avih @ 24.
    QCOMPARE(fourcc(data, 24), QByteArray("avih"));
    // microSecPerFrame at avih payload byte 0 (off 32) = 1e6 / 5 fps.
    QCOMPARE(leU32(data, 32), quint32(200000));
    // totalFrames at avih payload byte 16 (off 48).
    QCOMPARE(leU32(data, 48), quint32(3));

    // Find idx1 near end of file: contains 3 keyframe entries.
    int idxOff = data.indexOf(QByteArray("idx1"));
    QVERIFY(idxOff > 0);
    QCOMPARE(leU32(data, idxOff + 4), quint32(3 * 16));

    // First idx entry: ckid + flags + offset + size.
    QCOMPARE(fourcc(data, idxOff + 8), QByteArray("00dc"));
    QCOMPARE(leU32(data, idxOff + 12), quint32(0x10));

    // The first frame chunk header sits at the start of `movi` data.
    int moviOff = data.indexOf(QByteArray("movi"));
    QVERIFY(moviOff > 0);
    QCOMPARE(fourcc(data, moviOff + 4), QByteArray("00dc"));
    // JPEG SOI marker (0xFFD8) at frame data start.
    const int jpegStart = moviOff + 4 + 8;
    QCOMPARE(quint8(data.at(jpegStart)),     quint8(0xFF));
    QCOMPARE(quint8(data.at(jpegStart + 1)), quint8(0xD8));
}

void TestMjpegAviRecorder::resizedFrame_rescaledToStartDimensions()
{
    MjpegAviRecorder r;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(r.start(dir.filePath("rs.avi"), 32, 32, 5));
    // Push a differently-sized frame — recorder should rescale, not reject.
    QVERIFY(r.pushFrame(makeFrame(64, 48, Qt::red)));
    QCOMPARE(r.frameCount(), 1);
    r.stop();
}

QTEST_MAIN(TestMjpegAviRecorder)
#include "test_mjpeg_avi_recorder.moc"
