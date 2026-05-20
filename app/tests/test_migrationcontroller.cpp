// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "configstore.h"
#include "migrate/legacy_decrypt.h"
#include "migrationcontroller.h"

#include <leveldb/db.h>
#include <leveldb/options.h>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using qumesh::config::ConfigStore;
using qumesh::app::MigrationController;
using qumesh::migrate::LegacyCrypto;

namespace {

QByteArray chromiumKey(const QByteArray &origin, const QByteArray &name)
{
    QByteArray k;
    k.append('_');
    k.append(origin);
    k.append('\x00');
    k.append('\x01');
    k.append(name);
    return k;
}

QByteArray latin1Value(const QByteArray &data)
{
    QByteArray v;
    v.append('\x01');
    v.append(data);
    return v;
}

void seedLeveldb(const QString &path, const QByteArray &origin,
                 const QByteArray &computersV2)
{
    leveldb::Options opts;
    opts.create_if_missing = true;
    leveldb::DB *db = nullptr;
    QVERIFY(leveldb::DB::Open(opts, path.toStdString(), &db).ok());
    auto put = [&](const QByteArray &k, const QByteArray &v) {
        QVERIFY(db->Put(leveldb::WriteOptions(),
                        leveldb::Slice(k.constData(), k.size()),
                        leveldb::Slice(v.constData(), v.size()))
                    .ok());
    };
    put(chromiumKey(origin, "computers"), latin1Value(computersV2));
    put(chromiumKey(origin, "certificates"), latin1Value(LegacyCrypto::encryptV2("[]")));
    delete db;
}

} // namespace

class TestMigrationController : public QObject
{
    Q_OBJECT
private slots:
    void existingNativeConfigStaysIdle();
    void missingNativeAndLegacyReportsNoLegacy();
    void firstLaunchImportsFromLegacy();
};

void TestMigrationController::existingNativeConfigStaysIdle()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(tmp.path());
    QVERIFY(store.saveComputers({}));

    MigrationController c;
    c.setStore(&store);
    c.setLegacyDataDir(QStringLiteral("/nonexistent"));
    c.checkAndMaybeMigrate();
    QCOMPARE(c.state(), MigrationController::State::Idle);
}

void TestMigrationController::missingNativeAndLegacyReportsNoLegacy()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    ConfigStore store(QDir(tmp.path()).filePath(QStringLiteral("fresh")));

    MigrationController c;
    c.setStore(&store);
    c.setLegacyDataDir(QDir(tmp.path()).filePath(QStringLiteral("nothere")));
    c.checkAndMaybeMigrate();
    QCOMPARE(c.state(), MigrationController::State::NoLegacyData);
}

void TestMigrationController::firstLaunchImportsFromLegacy()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString outputDir = QDir(tmp.path()).filePath(QStringLiteral("fresh"));
    const QString legacyDir = QDir(tmp.path()).filePath(QStringLiteral("legacy"));
    const QByteArray origin = "chrome-extension://gkmjdlhkgdlflppihloafibfdkhfnfco";
    const QByteArray computersJson = R"([{"name":"amt-01","host":"10.0.0.5"}])";

    seedLeveldb(legacyDir, origin, LegacyCrypto::encryptV2(computersJson));

    ConfigStore store(outputDir);
    QVERIFY(!store.hasNativeConfig());

    MigrationController c;
    c.setStore(&store);
    c.setLegacyDataDir(legacyDir);

    QSignalSpy stateSpy(&c, &MigrationController::stateChanged);
    c.checkAndMaybeMigrate();
    QVERIFY(stateSpy.size() >= 1);
    QCOMPARE(c.state(), MigrationController::State::Imported);
    QCOMPARE(c.computersImported(), 1);
    QVERIFY(store.hasNativeConfig());
}

QTEST_GUILESS_MAIN(TestMigrationController)
#include "test_migrationcontroller.moc"
