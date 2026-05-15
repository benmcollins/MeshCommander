// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "migrate/legacy_decrypt.h"
#include "migrate/migrate.h"

#include <leveldb/db.h>
#include <leveldb/options.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace qumesh::migrate;

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
                 const QByteArray &computersV2, const QByteArray &certsV2,
                 const QList<std::pair<QByteArray, QByteArray>> &settings)
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
    put(chromiumKey(origin, "certificates"), latin1Value(certsV2));
    for (const auto &[name, value] : settings) {
        put(chromiumKey(origin, name), latin1Value(value));
    }
    delete db;
}

} // namespace

class TestMigrate : public QObject
{
    Q_OBJECT
private slots:
    void endToEnd();
    void refusesToReRunWithoutOverwrite();
    void missingLegacyDirReportsError();
};

void TestMigrate::endToEnd()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString legacyDir = QDir(tmp.path()).filePath(QStringLiteral("legacy"));
    const QString outputDir = QDir(tmp.path()).filePath(QStringLiteral("output"));

    const QByteArray origin = "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";
    const QByteArray computersJson = R"([{"name":"AmtMachine","host":"10.0.0.5"}])";
    const QByteArray certsJson = R"([])";

    const QByteArray computersV2 = LegacyCrypto::encryptV2(computersJson);
    const QByteArray certsV2 = LegacyCrypto::encryptV2(certsJson);

    seedLeveldb(legacyDir, origin, computersV2, certsV2,
                {{"meshserverurl", "https://server.example/"},
                 {"checkForUpdate", "false"}});

    MigrationOptions opts;
    opts.legacyDataDir = legacyDir;
    opts.outputDir = outputDir;
    opts.origin = QString::fromUtf8(origin);

    Migrator m;
    const MigrationResult r = m.run(opts);
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.computersCount, 1);
    QCOMPARE(r.certificatesCount, 0);
    QCOMPARE(r.settingsCount, 2);

    auto readJson = [&](const QString &name) -> QJsonDocument {
        const QString path = QDir(outputDir).filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "could not open" << path << ":" << f.errorString();
            return {};
        }
        return QJsonDocument::fromJson(f.readAll());
    };

    const auto computersDoc = readJson(QStringLiteral("computers.json"));
    QVERIFY(computersDoc.isArray());
    QCOMPARE(computersDoc.array().size(), 1);
    QCOMPARE(computersDoc.array().first().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("AmtMachine"));

    const auto settingsDoc = readJson(QStringLiteral("settings.json"));
    QVERIFY(settingsDoc.isObject());
    QCOMPARE(settingsDoc.object().value(QStringLiteral("meshserverurl")).toString(),
             QStringLiteral("https://server.example/"));
    QCOMPARE(settingsDoc.object().value(QStringLiteral("checkForUpdate")).toString(),
             QStringLiteral("false"));

    const auto stampDoc = readJson(QStringLiteral("migration.json"));
    QVERIFY(stampDoc.isObject());
    QCOMPARE(stampDoc.object().value(QStringLiteral("from")).toString(), legacyDir);
}

void TestMigrate::refusesToReRunWithoutOverwrite()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString legacyDir = QDir(tmp.path()).filePath(QStringLiteral("legacy"));
    const QString outputDir = QDir(tmp.path()).filePath(QStringLiteral("output"));
    const QByteArray origin = "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";

    seedLeveldb(legacyDir, origin,
                LegacyCrypto::encryptV2(QByteArrayLiteral("[]")),
                LegacyCrypto::encryptV2(QByteArrayLiteral("[]")), {});

    MigrationOptions opts;
    opts.legacyDataDir = legacyDir;
    opts.outputDir = outputDir;
    opts.origin = QString::fromUtf8(origin);

    Migrator m;
    QVERIFY(m.run(opts).ok);

    // Second run without overwrite should refuse.
    const MigrationResult second = m.run(opts);
    QVERIFY(!second.ok);
    QVERIFY(second.error.contains(QStringLiteral("already contains a migration")));

    // With overwrite it should succeed again.
    opts.overwrite = true;
    QVERIFY(m.run(opts).ok);
}

void TestMigrate::missingLegacyDirReportsError()
{
    MigrationOptions opts;
    opts.legacyDataDir = QStringLiteral("/definitely/not/a/dir");
    opts.outputDir = QDir::tempPath() + QStringLiteral("/qumesh-test-output");

    Migrator m;
    const MigrationResult r = m.run(opts);
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("does not exist")));
}

QTEST_GUILESS_MAIN(TestMigrate)
#include "test_migrate.moc"
