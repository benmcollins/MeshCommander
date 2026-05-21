// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "backup_codec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using qumesh::app::BackupCodec;

class TestBackupCodec : public QObject
{
    Q_OBJECT
private slots:
    void roundTripsEmptyList();
    void roundTripsSmallList();
    void roundTripsLargeList();
    void wrongPasswordReturnsBadPassword();
    void truncatedFileReturnsCorrupt();
    void unknownTypeReturnsBadFormat();
    void unknownVersionReturnsBadFormat();
    void tamperedCiphertextReturnsBadPassword();
    void envelopeHasExpectedHeaders();
    void emptyPasswordOnDecodeIsRejected();
    void rejectsOversizedKdfIterations();
    void rejectsOversizedFile();
    void rejectsOversizedPassword();
};

namespace {
QJsonArray sampleComputers(int n)
{
    QJsonArray arr;
    for (int i = 0; i < n; ++i) {
        arr.push_back(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("uuid-%1").arg(i)},
            {QStringLiteral("name"), QStringLiteral("amt-%1").arg(i)},
            {QStringLiteral("host"), QStringLiteral("10.0.0.%1").arg(i % 254 + 1)},
            {QStringLiteral("user"), QStringLiteral("admin")},
            {QStringLiteral("pass"), QStringLiteral("hunter2-%1").arg(i)},
            {QStringLiteral("tls"), (i % 2) == 0},
        });
    }
    return arr;
}
} // namespace

void TestBackupCodec::roundTripsEmptyList()
{
    const QString pw = QStringLiteral("correct horse battery staple");
    const auto enc = BackupCodec::encode(QJsonArray{}, pw);
    QVERIFY2(enc.ok(), qPrintable(enc.error));

    const auto dec = BackupCodec::decode(enc.bytes, pw);
    QVERIFY2(dec.ok(), qPrintable(dec.error));
    QCOMPARE(dec.computers.size(), 0);
}

void TestBackupCodec::roundTripsSmallList()
{
    const QJsonArray src = sampleComputers(3);
    const QString pw = QStringLiteral("a-strong-password");
    const auto enc = BackupCodec::encode(src, pw);
    QVERIFY(enc.ok());

    const auto dec = BackupCodec::decode(enc.bytes, pw);
    QVERIFY(dec.ok());
    QCOMPARE(dec.computers, src);
}

/// Costs about ~600 ms total on a 2024 laptop because PBKDF2 runs at
/// import-cost twice (encode + decode). Keep `n` modest so the suite
/// stays fast; this test exists to catch buffer-sizing regressions
/// the small case can mask.
void TestBackupCodec::roundTripsLargeList()
{
    const QJsonArray src = sampleComputers(500);
    const QString pw = QStringLiteral("pw-long-enough");
    const auto enc = BackupCodec::encode(src, pw);
    QVERIFY(enc.ok());

    const auto dec = BackupCodec::decode(enc.bytes, pw);
    QVERIFY(dec.ok());
    QCOMPARE(dec.computers.size(), src.size());
    QCOMPARE(dec.computers.last().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("amt-499"));
}

void TestBackupCodec::wrongPasswordReturnsBadPassword()
{
    const auto enc = BackupCodec::encode(sampleComputers(2), QStringLiteral("right"));
    QVERIFY(enc.ok());
    const auto dec = BackupCodec::decode(enc.bytes, QStringLiteral("wrong"));
    QVERIFY(!dec.ok());
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadPassword);
}

void TestBackupCodec::truncatedFileReturnsCorrupt()
{
    const auto enc = BackupCodec::encode(sampleComputers(1), QStringLiteral("pw"));
    QVERIFY(enc.ok());

    // Lop off the last quarter — the JSON envelope itself becomes
    // invalid, so we go through the BadFormat path.
    QByteArray bad = enc.bytes;
    bad.truncate(bad.size() * 3 / 4);
    const auto dec = BackupCodec::decode(bad, QStringLiteral("pw"));
    QVERIFY(!dec.ok());
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadFormat);
}

void TestBackupCodec::unknownTypeReturnsBadFormat()
{
    const QByteArray fake = QJsonDocument(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("something-else")},
        {QStringLiteral("version"), 1},
    }).toJson();
    const auto dec = BackupCodec::decode(fake, QStringLiteral("pw"));
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadFormat);
}

void TestBackupCodec::unknownVersionReturnsBadFormat()
{
    const auto enc = BackupCodec::encode(sampleComputers(1), QStringLiteral("pw"));
    QVERIFY(enc.ok());

    QJsonDocument doc = QJsonDocument::fromJson(enc.bytes);
    QJsonObject env = doc.object();
    env.insert(QStringLiteral("version"), 999);
    const QByteArray bumped = QJsonDocument(env).toJson();

    const auto dec = BackupCodec::decode(bumped, QStringLiteral("pw"));
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadFormat);
}

void TestBackupCodec::tamperedCiphertextReturnsBadPassword()
{
    const auto enc = BackupCodec::encode(sampleComputers(2), QStringLiteral("pw"));
    QVERIFY(enc.ok());

    QJsonDocument doc = QJsonDocument::fromJson(enc.bytes);
    QJsonObject env = doc.object();
    // Flip a byte in the ciphertext base64 by re-encoding a mutated raw blob.
    QByteArray ct = QByteArray::fromBase64(env.value(QStringLiteral("ciphertext")).toString().toLatin1());
    QVERIFY(!ct.isEmpty());
    ct[0] = ct[0] ^ 0x42;
    env.insert(QStringLiteral("ciphertext"), QString::fromLatin1(ct.toBase64()));
    const QByteArray mutated = QJsonDocument(env).toJson();

    const auto dec = BackupCodec::decode(mutated, QStringLiteral("pw"));
    // GCM can't tell tamper from wrong password — both surface as
    // BadPassword by design, see the comment in backup_codec.cpp.
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadPassword);
}

void TestBackupCodec::envelopeHasExpectedHeaders()
{
    const auto enc = BackupCodec::encode(sampleComputers(1), QStringLiteral("pw"));
    QVERIFY(enc.ok());

    const QJsonObject env = QJsonDocument::fromJson(enc.bytes).object();
    QCOMPARE(env.value(QStringLiteral("type")).toString(), QStringLiteral("qumesh-backup"));
    QCOMPARE(env.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(env.value(QStringLiteral("envelope")).toString(), QStringLiteral("v1"));
    QCOMPARE(env.value(QStringLiteral("kdf")).toObject().value(QStringLiteral("iterations")).toInt(),
             BackupCodec::kPbkdf2Iterations);
    QCOMPARE(env.value(QStringLiteral("kdf")).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("pbkdf2-sha256"));
    QCOMPARE(env.value(QStringLiteral("cipher")).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("aes-256-gcm"));
}

void TestBackupCodec::emptyPasswordOnDecodeIsRejected()
{
    const auto enc = BackupCodec::encode(sampleComputers(1), QStringLiteral("pw"));
    QVERIFY(enc.ok());
    const auto dec = BackupCodec::decode(enc.bytes, QString());
    QVERIFY(!dec.ok());
}

/// Security: an attacker-controlled envelope must not be able to set
/// `kdf.iterations` to INT_MAX and freeze the UI thread inside
/// PBKDF2 for hours. The decoder caps at kPbkdf2MaxIterations.
void TestBackupCodec::rejectsOversizedKdfIterations()
{
    const auto enc = BackupCodec::encode(sampleComputers(1), QStringLiteral("pw"));
    QVERIFY(enc.ok());

    QJsonDocument doc = QJsonDocument::fromJson(enc.bytes);
    QJsonObject env = doc.object();
    QJsonObject kdf = env.value(QStringLiteral("kdf")).toObject();
    kdf.insert(QStringLiteral("iterations"), 100'000'000);
    env.insert(QStringLiteral("kdf"), kdf);
    const QByteArray bumped = QJsonDocument(env).toJson();

    const auto dec = BackupCodec::decode(bumped, QStringLiteral("pw"));
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadFormat);
}

/// Security: a multi-GB file must be rejected before being read
/// fully into memory inside `BackupCodec::decode`. We mirror the cap
/// at the codec layer for defense in depth.
void TestBackupCodec::rejectsOversizedFile()
{
    QByteArray oversized(BackupCodec::kMaxFileBytes + 1, '\0');
    const auto dec = BackupCodec::decode(oversized, QStringLiteral("pw"));
    QCOMPARE(dec.reason, BackupCodec::FailReason::BadFormat);
}

/// Cap on input password length protects PBKDF2 from a multi-MB
/// paste in the password field.
void TestBackupCodec::rejectsOversizedPassword()
{
    const QString huge = QString(BackupCodec::kMaxPasswordChars + 1, QChar('x'));
    const auto enc = BackupCodec::encode(sampleComputers(1), huge);
    QVERIFY(!enc.ok());
}

QTEST_GUILESS_MAIN(TestBackupCodec)
#include "test_backup_codec.moc"
