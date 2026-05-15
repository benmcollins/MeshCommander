// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "migrate/legacy_decrypt.h"

#include <QProcess>
#include <QtTest>

using namespace qumesh::migrate;

class TestLegacyDecrypt : public QObject
{
    Q_OBJECT
private slots:
    void v2RoundTrip();
    void legacyFormatDetection();
    void v2FormatDetection();
    void unknownFormatRejected();
    void evpBytesToKeyMatchesKnownVector();
    void legacyDecryptAgainstOpensslEnc();
    void corruptedV2EnvelopeReportsError();
    void emptyInputRejected();
};

void TestLegacyDecrypt::v2RoundTrip()
{
    const QByteArray plain = R"([{"name":"AmtMachine","host":"10.0.0.5"}])";
    const QByteArray envelope = LegacyCrypto::encryptV2(plain);

    QVERIFY(envelope.startsWith("v2:"));
    auto r = LegacyCrypto::decrypt(envelope);
    QVERIFY2(r.ok(), qPrintable(r.error));
    QCOMPARE(r.format, LegacyCrypto::Format::V2);
    QCOMPARE(r.plaintext, plain);
}

void TestLegacyDecrypt::v2FormatDetection()
{
    QCOMPARE(LegacyCrypto::detectFormat(QByteArrayLiteral("v2:abc")),
             LegacyCrypto::Format::V2);
}

void TestLegacyDecrypt::legacyFormatDetection()
{
    // 512 hex chars + some more hex chars => legacy
    QByteArray legacy(513, 'a');
    QCOMPARE(LegacyCrypto::detectFormat(legacy), LegacyCrypto::Format::Legacy);
}

void TestLegacyDecrypt::unknownFormatRejected()
{
    QCOMPARE(LegacyCrypto::detectFormat(QByteArrayLiteral("not an envelope")),
             LegacyCrypto::Format::Unknown);
    auto r = LegacyCrypto::decrypt(QByteArrayLiteral("not an envelope"));
    QVERIFY(!r.ok());
}

void TestLegacyDecrypt::evpBytesToKeyMatchesKnownVector()
{
    // Reference values produced by:
    //   openssl enc -aes-256-cbc -md md5 -nosalt -k password -P
    auto [key, iv] = LegacyCrypto::evpBytesToKeyMd5(QByteArrayLiteral("password"), 32, 16);
    QCOMPARE(key.toHex().toUpper(),
             QByteArray("5F4DCC3B5AA765D61D8327DEB882CF992B95990A9151374ABD8FF8C5A7A0FE08"));
    QCOMPARE(iv.toHex().toUpper(),
             QByteArray("B7B4372CDFBCB3D16A2631B59B509E94"));
}

void TestLegacyDecrypt::legacyDecryptAgainstOpensslEnc()
{
    // Construct a legacy ciphertext exactly the way the NW.js app did:
    //   stored = passwordHex + AES-256-CTR(EVP_BytesToKey(passwordHex), plain).toHex()
    //
    // Where passwordHex is itself a hex string (the JS code did
    // `buf.toString('hex')` then used the hex *string* as the password).
    const QByteArray passwordHex = QByteArray(512, 'A'); // any 512 hex chars
    const QByteArray plain = QByteArrayLiteral("the quick brown fox");

    // Use our own encryptV2 helper to derive the ciphertext via OpenSSL.
    auto [key, iv] = LegacyCrypto::evpBytesToKeyMd5(passwordHex, 32, 16);

    // We need to manually build the legacy envelope. We don't have a public
    // helper that does this, so use openssl(1) via QProcess to keep the test
    // independent of our own AES code path.
    const QString opensslPath = QStringLiteral("openssl");
    QProcess proc;
    proc.setProgram(opensslPath);
    proc.setArguments({
        QStringLiteral("enc"), QStringLiteral("-aes-256-ctr"),
        QStringLiteral("-K"), QString::fromLatin1(key.toHex()),
        QStringLiteral("-iv"), QString::fromLatin1(iv.toHex()),
        QStringLiteral("-nopad"),
    });
    proc.start();
    if (!proc.waitForStarted()) {
        QSKIP("openssl(1) not available on this system");
    }
    proc.write(plain);
    proc.closeWriteChannel();
    QVERIFY(proc.waitForFinished());
    QCOMPARE(proc.exitCode(), 0);

    const QByteArray ctRaw = proc.readAllStandardOutput();
    QByteArray stored = passwordHex + ctRaw.toHex();

    auto r = LegacyCrypto::decrypt(stored);
    QVERIFY2(r.ok(), qPrintable(r.error));
    QCOMPARE(r.format, LegacyCrypto::Format::Legacy);
    QCOMPARE(r.plaintext, plain);
}

void TestLegacyDecrypt::corruptedV2EnvelopeReportsError()
{
    auto r = LegacyCrypto::decrypt(QByteArrayLiteral("v2:not hex at all"));
    QVERIFY(!r.ok());
}

void TestLegacyDecrypt::emptyInputRejected()
{
    auto r = LegacyCrypto::decrypt(QByteArray{});
    QVERIFY(!r.ok());
}

QTEST_GUILESS_MAIN(TestLegacyDecrypt)
#include "test_legacy_decrypt.moc"
