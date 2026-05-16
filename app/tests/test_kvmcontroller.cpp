// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "kvmcontroller.h"
#include "kvmframebuffer.h"

#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using qumesh::app::KvmController;
using qumesh::app::KvmFramebuffer;

class TestKvmController : public QObject
{
    Q_OBJECT

private slots:
    void saveScreenshot_emptyFramebuffer_returnsFalse();
    void saveScreenshot_emptyPath_returnsFalse();
    void saveScreenshot_writesPngMatchingFramebuffer();
};

void TestKvmController::saveScreenshot_emptyFramebuffer_returnsFalse()
{
    KvmController c;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("empty.png");
    QVERIFY(!c.saveScreenshot(path));
    QVERIFY(!QFileInfo::exists(path));
}

void TestKvmController::saveScreenshot_emptyPath_returnsFalse()
{
    KvmController c;
    c.framebuffer()->resize(16, 16);
    QVERIFY(!c.saveScreenshot(QString()));
}

void TestKvmController::saveScreenshot_writesPngMatchingFramebuffer()
{
    KvmController c;
    KvmFramebuffer *fb = c.framebuffer();
    fb->resize(32, 16);

    // Paint a recognisable solid-red tile so we can verify roundtrip.
    QImage tile(32, 16, QImage::Format_ARGB32);
    tile.fill(QColor(220, 32, 64));
    fb->applyTile(0, 0, tile);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("shot.png");
    QVERIFY(c.saveScreenshot(path));
    QVERIFY(QFileInfo(path).size() > 0);

    QImage loaded(path);
    QVERIFY(!loaded.isNull());
    QCOMPARE(loaded.width(), 32);
    QCOMPARE(loaded.height(), 16);
    // QImage::pixel returns ARGB; the saved PNG round-trip should
    // preserve the exact RGB we painted in.
    QCOMPARE(QColor(loaded.pixel(0, 0)).red(),   220);
    QCOMPARE(QColor(loaded.pixel(0, 0)).green(),  32);
    QCOMPARE(QColor(loaded.pixel(0, 0)).blue(),   64);
    QCOMPARE(QColor(loaded.pixel(31, 15)).red(), 220);
}

QTEST_MAIN(TestKvmController)
#include "test_kvmcontroller.moc"
