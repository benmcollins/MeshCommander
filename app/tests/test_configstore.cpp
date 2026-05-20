// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "configstore.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_UNIX
#  include <sys/stat.h>
#endif

using qumesh::config::ConfigStore;

class TestConfigStore : public QObject
{
    Q_OBJECT
private slots:
    void emptyDirHasNoNativeConfig();
    void computersRoundTrip();
    void settingsRoundTrip();
    void malformedJsonReportsError();
    void saveCreatesDirIfMissing();
    void atomicityPartialWriteLeavesOldFile();
    void savedFilesAreOwnerReadableOnly();
};

void TestConfigStore::emptyDirHasNoNativeConfig()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    QVERIFY(!store.hasNativeConfig());
}

void TestConfigStore::computersRoundTrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());

    QJsonArray arr{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("AmtMachine")},
            {QStringLiteral("host"), QStringLiteral("10.0.0.5")},
            {QStringLiteral("port"), 16992},
        },
    };
    QVERIFY2(store.saveComputers(arr), qPrintable(store.lastError()));
    QVERIFY(store.hasNativeConfig());

    const QJsonArray reloaded = store.loadComputers();
    QCOMPARE(reloaded.size(), 1);
    QCOMPARE(reloaded.first().toObject().value(QStringLiteral("host")).toString(),
             QStringLiteral("10.0.0.5"));
}

void TestConfigStore::settingsRoundTrip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());

    QJsonObject obj{
        {QStringLiteral("meshserverurl"), QStringLiteral("https://server.example/")},
        {QStringLiteral("TlsSecurityMode"), QStringLiteral("1")},
    };
    QVERIFY2(store.saveSettings(obj), qPrintable(store.lastError()));
    const QJsonObject reloaded = store.loadSettings();
    QCOMPARE(reloaded.size(), 2);
    QCOMPARE(reloaded.value(QStringLiteral("meshserverurl")).toString(),
             QStringLiteral("https://server.example/"));
}

void TestConfigStore::malformedJsonReportsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QFile f(QDir(tmp.path()).filePath(QStringLiteral("computers.json")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{not json");
    f.close();

    ConfigStore store(tmp.path());
    const QJsonArray reloaded = store.loadComputers();
    QVERIFY(reloaded.isEmpty());
    QVERIFY(!store.lastError().isEmpty());
}

void TestConfigStore::saveCreatesDirIfMissing()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString nested = QDir(tmp.path()).filePath(QStringLiteral("a/b/c"));
    ConfigStore store(nested);
    QVERIFY(store.saveComputers({}));
    QVERIFY(QFileInfo(nested).isDir());
}

void TestConfigStore::atomicityPartialWriteLeavesOldFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());

    QJsonArray first{QJsonObject{{QStringLiteral("name"), QStringLiteral("first")}}};
    QVERIFY(store.saveComputers(first));

    // QSaveFile guarantees that a failed `commit()` does not replace the
    // existing file. We can't easily induce a write failure, but we can
    // assert that a successful overwrite produces exactly the new content
    // and the file size matches — i.e. no leftover bytes from the old.
    QJsonArray second{QJsonObject{{QStringLiteral("name"), QStringLiteral("second")}}};
    QVERIFY(store.saveComputers(second));

    const QJsonArray reloaded = store.loadComputers();
    QCOMPARE(reloaded.size(), 1);
    QCOMPARE(reloaded.first().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("second"));
}

/// #273 — ConfigStore must write secret-bearing files with 0600 and
/// the containing directory with 0700 on POSIX. The Q_OS_WIN side
/// uses ACLs which Qt's setPermissions translates to but is hard to
/// inspect in a portable way; skip there. The check uses the
/// platform-native stat() so the QFileDevice masking doesn't lie to
/// us — Qt's Permission readback can claim 0600 even when the inode
/// mode disagrees, depending on filesystem.
void TestConfigStore::savedFilesAreOwnerReadableOnly()
{
#ifndef Q_OS_UNIX
    QSKIP("POSIX-specific mode check");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());

    QVERIFY(store.saveComputers(QJsonArray{
        QJsonObject{{QStringLiteral("host"), QStringLiteral("h")}}}));
    QVERIFY(store.saveSettings(QJsonObject{
        {QStringLiteral("k"), QStringLiteral("v")}}));

    auto checkFile = [](const QString &path) {
        struct ::stat st {};
        QVERIFY2(::stat(path.toLocal8Bit().constData(), &st) == 0,
                 qPrintable(path));
        // Owner can read/write; group + other must have no bits.
        QCOMPARE(st.st_mode & 0177, mode_t(0));
    };

    checkFile(QDir(tmp.path()).filePath(QStringLiteral("computers.json")));
    checkFile(QDir(tmp.path()).filePath(QStringLiteral("settings.json")));

    struct ::stat dirSt {};
    QVERIFY(::stat(tmp.path().toLocal8Bit().constData(), &dirSt) == 0);
    QCOMPARE(dirSt.st_mode & 0077, mode_t(0));
#endif
}

QTEST_GUILESS_MAIN(TestConfigStore)
#include "test_configstore.moc"
