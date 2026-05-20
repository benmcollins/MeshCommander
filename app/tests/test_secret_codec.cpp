// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "keystore.h"
#include "secret_codec.h"
#include "migrate/legacy_decrypt.h"

#include <QtTest>

using qumesh::app::KeyStore;
using qumesh::app::SecretCodec;

class TestSecretCodec : public QObject
{
    Q_OBJECT
private slots:
    void roundTripsThroughV3WhenKeystoreAvailable();
    void fallsBackToV2WhenKeystoreUnavailable();
    void readsLegacyV2EnvelopeUnchanged();
    void readsBareValueAsPreCodec();
    void emptyEncodesToEmpty();
};

/// #306 — encode() should emit a v3 envelope whenever the platform
/// keystore returns a key. CMake sets `QUMESH_NO_KEYSTORE=1` for the
/// suite by default so unattended runs (CI, devs without a
/// signed-and-trusted Keychain item) don't block on a password
/// prompt. To exercise the real keystore path locally, run:
///
///     env -u QUMESH_NO_KEYSTORE \
///         ./build/app/tests/test_secret_codec roundTripsThroughV3
void TestSecretCodec::roundTripsThroughV3WhenKeystoreAvailable()
{
    if (!qgetenv("QUMESH_NO_KEYSTORE").isEmpty()) {
        QSKIP("Keystore disabled via QUMESH_NO_KEYSTORE — see test docs.");
    }
    KeyStore::resetCacheForTest();
    QVERIFY(KeyStore::masterKey().size() == 32);

    const QString plain = QStringLiteral("hunter2:correct horse battery staple");
    const QString wrapped = SecretCodec::encode(plain);
    QVERIFY(wrapped.startsWith(QStringLiteral("v3:")));
    QCOMPARE(SecretCodec::decode(wrapped), plain);
}

/// When the keystore is unavailable (QUMESH_NO_KEYSTORE set, or a
/// stripped-down system without writable XDG home, etc.), encode()
/// must fall back to v2 — the codec keeps working with weaker
/// protection rather than failing the whole save.
void TestSecretCodec::fallsBackToV2WhenKeystoreUnavailable()
{
    qputenv("QUMESH_NO_KEYSTORE", "1");
    KeyStore::resetCacheForTest();
    QVERIFY(KeyStore::masterKey().isEmpty());

    const QString plain = QStringLiteral("fallback");
    const QString wrapped = SecretCodec::encode(plain);
    QVERIFY(wrapped.startsWith(QStringLiteral("v2:")));
    QCOMPARE(SecretCodec::decode(wrapped), plain);
}

/// Backwards compat: existing computers.json files on upgrade hold
/// v2 envelopes (with the AES key inline). The new code must still
/// read them. The ComputerModel save path will rewrite v2→v3 on the
/// next persist, which is the migration mechanism #306 promises.
void TestSecretCodec::readsLegacyV2EnvelopeUnchanged()
{
    const QString plain = QStringLiteral("legacy-secret");
    const QByteArray v2 = qumesh::migrate::LegacyCrypto::encryptV2(plain.toUtf8());
    QVERIFY(v2.startsWith("v2:"));
    QCOMPARE(SecretCodec::decode(QString::fromLatin1(v2)), plain);
}

/// Pre-codec data (no recognised prefix) is returned verbatim, so
/// hand-edited configs from before SecretCodec existed still load.
void TestSecretCodec::readsBareValueAsPreCodec()
{
    QCOMPARE(SecretCodec::decode(QStringLiteral("not-encrypted-yet")),
             QStringLiteral("not-encrypted-yet"));
}

void TestSecretCodec::emptyEncodesToEmpty()
{
    QCOMPARE(SecretCodec::encode(QString()), QString());
    QCOMPARE(SecretCodec::decode(QString()), QString());
}

QTEST_GUILESS_MAIN(TestSecretCodec)
#include "test_secret_codec.moc"
