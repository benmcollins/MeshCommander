// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "migrate/chromium_storage.h"

#include <leveldb/db.h>
#include <leveldb/options.h>

#include <QTemporaryDir>
#include <QtTest>

using namespace meshcommander::migrate;

namespace {

QByteArray makeKey(const QByteArray &origin, const QByteArray &name)
{
    QByteArray k;
    k.append('_');
    k.append(origin);
    k.append('\x00');
    k.append('\x01');
    k.append(name);
    return k;
}

QByteArray makeLatin1Value(const QByteArray &data)
{
    QByteArray v;
    v.append('\x01');
    v.append(data);
    return v;
}

QByteArray makeUtf16Value(const QString &s)
{
    QByteArray v;
    v.append('\x00');
    const auto *u16 = reinterpret_cast<const char *>(s.utf16());
    v.append(u16, s.size() * 2);
    return v;
}

void populate(const QString &path, const QList<std::pair<QByteArray, QByteArray>> &entries)
{
    leveldb::Options opts;
    opts.create_if_missing = true;
    leveldb::DB *db = nullptr;
    QVERIFY(leveldb::DB::Open(opts, path.toStdString(), &db).ok());
    for (const auto &[k, v] : entries) {
        QVERIFY(db->Put(leveldb::WriteOptions(),
                        leveldb::Slice(k.constData(), k.size()),
                        leveldb::Slice(v.constData(), v.size()))
                    .ok());
    }
    delete db;
}

} // namespace

class TestChromiumStorage : public QObject
{
    Q_OBJECT
private slots:
    void decodeLatin1Value();
    void decodeUtf16Value();
    void decodeUnknownTag();
    void readOriginPicksMatchingKeys();
    void readOriginIgnoresOtherOrigins();
    void readOriginOnUnopenedDbErrors();
};

void TestChromiumStorage::decodeLatin1Value()
{
    QCOMPARE(ChromiumStorageReader::decodeValue(makeLatin1Value("hello")),
             QByteArray("hello"));
}

void TestChromiumStorage::decodeUtf16Value()
{
    QCOMPARE(ChromiumStorageReader::decodeValue(makeUtf16Value(QStringLiteral("héllo"))),
             QStringLiteral("héllo").toUtf8());
}

void TestChromiumStorage::decodeUnknownTag()
{
    QByteArray v;
    v.append('\x42');
    v.append("blah");
    QCOMPARE(ChromiumStorageReader::decodeValue(v), QByteArray());
}

void TestChromiumStorage::readOriginPicksMatchingKeys()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QByteArray origin = "chrome-extension://abcdefghijklmnop";

    populate(tmp.path(), {
        {makeKey(origin, "computers"), makeLatin1Value("v2:deadbeef")},
        {makeKey(origin, "checkForUpdate"), makeLatin1Value("true")},
        {makeKey(origin, "meshserverurl"), makeUtf16Value(QStringLiteral("https://x"))},
        // Some unrelated noise keys that should not be returned:
        {QByteArrayLiteral("VERSION"), QByteArrayLiteral("\x01")},
        {QByteArrayLiteral("META:") + origin, QByteArrayLiteral("\x01garbage")},
    });

    ChromiumStorageReader r(tmp.path());
    QVERIFY2(r.open(), qPrintable(r.lastError()));
    QList<ChromiumEntry> entries = r.readOrigin(QString::fromUtf8(origin));
    QCOMPARE(entries.size(), 3);

    QStringList keys;
    for (const auto &e : entries) keys << e.key;
    keys.sort();
    QCOMPARE(keys, (QStringList{"checkForUpdate", "computers", "meshserverurl"}));

    for (const auto &e : entries) {
        if (e.key == QStringLiteral("computers"))
            QCOMPARE(e.value, QByteArray("v2:deadbeef"));
        else if (e.key == QStringLiteral("checkForUpdate"))
            QCOMPARE(e.value, QByteArray("true"));
        else if (e.key == QStringLiteral("meshserverurl"))
            QCOMPARE(e.value, QByteArray("https://x"));
    }
}

void TestChromiumStorage::readOriginIgnoresOtherOrigins()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    populate(tmp.path(), {
        {makeKey("chrome-extension://wantedaaaaaaaaa", "computers"), makeLatin1Value("yes")},
        {makeKey("chrome-extension://otherbbbbbbbbbb", "computers"), makeLatin1Value("no")},
    });

    ChromiumStorageReader r(tmp.path());
    QVERIFY2(r.open(), qPrintable(r.lastError()));
    QList<ChromiumEntry> entries =
        r.readOrigin(QStringLiteral("chrome-extension://wantedaaaaaaaaa"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().value, QByteArray("yes"));
}

void TestChromiumStorage::readOriginOnUnopenedDbErrors()
{
    ChromiumStorageReader r(QStringLiteral("/no/such/path"));
    auto entries = r.readOrigin(QStringLiteral("chrome-extension://x"));
    QVERIFY(entries.isEmpty());
    QVERIFY(!r.lastError().isEmpty());
}

QTEST_GUILESS_MAIN(TestChromiumStorage)
#include "test_chromium_storage.moc"
