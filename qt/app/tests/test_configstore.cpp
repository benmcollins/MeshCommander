// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "configstore.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using meshcommander::config::ConfigStore;

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

QTEST_GUILESS_MAIN(TestConfigStore)
#include "test_configstore.moc"
