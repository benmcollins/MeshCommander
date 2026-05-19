// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/cert_request_builder.h"

#include <QtTest>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <memory>

using namespace qumesh::wsman;

class TestCertRequestBuilder : public QObject
{
    Q_OBJECT
private slots:
    void emptyPubkeyReturnsEmptyBlob();
    void buildsParseableCsrWithExpectedSubject();
    void embeddedPublicKeyMatchesInput();
    void usesSha256RsaByDefault();
    void usesSha1RsaWhenRequested();
    void invalidSubjectDnFails();
};

namespace {

// Generate a 2048-bit RSA key pair and return its public-key bytes
// in SubjectPublicKeyInfo (X.509) form — the same shape AMT returns
// in `AMT_PublicPrivateKeyPair.DERKey`.
QByteArray makeSpkiForFreshRsaKey(EVP_PKEY **outFullKey = nullptr)
{
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
        EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    if (!ctx) return {};
    if (EVP_PKEY_keygen_init(ctx.get()) != 1) return {};
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) != 1) return {};
    EVP_PKEY *raw = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &raw) != 1) return {};
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(raw, EVP_PKEY_free);

    unsigned char *out = nullptr;
    const int len = i2d_PUBKEY(key.get(), &out);
    if (len <= 0) return {};
    QByteArray spki(reinterpret_cast<const char *>(out), len);
    OPENSSL_free(out);
    if (outFullKey != nullptr) *outFullKey = key.release();
    return spki;
}

} // namespace

void TestCertRequestBuilder::emptyPubkeyReturnsEmptyBlob()
{
    QString err;
    const QByteArray result = buildNullSignedPkcs10Csr({}, QStringLiteral("CN=Test"),
                                                        /*signing*/ 1, &err);
    QVERIFY(result.isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestCertRequestBuilder::buildsParseableCsrWithExpectedSubject()
{
    QByteArray spki = makeSpkiForFreshRsaKey();
    QVERIFY(!spki.isEmpty());

    QString err;
    const QByteArray req = buildNullSignedPkcs10Csr(spki,
        QStringLiteral("CN=QuMesh AMT, O=Watter, OU=Engineering"),
        /*signing*/ 1, &err);
    QVERIFY2(!req.isEmpty(), qPrintable(err));

    const unsigned char *p = reinterpret_cast<const unsigned char *>(req.constData());
    std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> parsed(
        d2i_X509_REQ(nullptr, &p, static_cast<long>(req.size())), X509_REQ_free);
    QVERIFY(parsed != nullptr);

    X509_NAME *subj = X509_REQ_get_subject_name(parsed.get());
    QVERIFY(subj != nullptr);

    char *subjOneLine = X509_NAME_oneline(subj, nullptr, 0);
    QVERIFY(subjOneLine != nullptr);
    const QString subjStr = QString::fromUtf8(subjOneLine);
    OPENSSL_free(subjOneLine);

    // X509_NAME_oneline emits "/key=value" pairs in document order.
    QVERIFY(subjStr.contains(QStringLiteral("/CN=QuMesh AMT")));
    QVERIFY(subjStr.contains(QStringLiteral("/O=Watter")));
    QVERIFY(subjStr.contains(QStringLiteral("/OU=Engineering")));
}

void TestCertRequestBuilder::embeddedPublicKeyMatchesInput()
{
    EVP_PKEY *rawKey = nullptr;
    QByteArray spki = makeSpkiForFreshRsaKey(&rawKey);
    QVERIFY(!spki.isEmpty());
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> original(rawKey, EVP_PKEY_free);

    QString err;
    const QByteArray req = buildNullSignedPkcs10Csr(spki,
        QStringLiteral("CN=Match Test"), 1, &err);
    QVERIFY2(!req.isEmpty(), qPrintable(err));

    const unsigned char *p = reinterpret_cast<const unsigned char *>(req.constData());
    std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> parsed(
        d2i_X509_REQ(nullptr, &p, static_cast<long>(req.size())), X509_REQ_free);
    QVERIFY(parsed != nullptr);

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> embedded(
        X509_REQ_get_pubkey(parsed.get()), EVP_PKEY_free);
    QVERIFY(embedded != nullptr);

    // EVP_PKEY_eq returns 1 iff the public components match. The
    // private half of `original` isn't relevant — the SPKI we passed
    // in is pubkey-only, and AMT does the same.
    QCOMPARE(EVP_PKEY_eq(embedded.get(), original.get()), 1);
}

void TestCertRequestBuilder::usesSha256RsaByDefault()
{
    QByteArray spki = makeSpkiForFreshRsaKey();
    QVERIFY(!spki.isEmpty());

    const QByteArray req = buildNullSignedPkcs10Csr(spki,
        QStringLiteral("CN=Default Algo"), /*default signing*/ 1);
    QVERIFY(!req.isEmpty());

    const unsigned char *p = reinterpret_cast<const unsigned char *>(req.constData());
    std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> parsed(
        d2i_X509_REQ(nullptr, &p, static_cast<long>(req.size())), X509_REQ_free);
    QVERIFY(parsed != nullptr);

    const X509_ALGOR *sigAlg = nullptr;
    X509_REQ_get0_signature(parsed.get(), nullptr, &sigAlg);
    QVERIFY(sigAlg != nullptr);
    const ASN1_OBJECT *algObj = nullptr;
    X509_ALGOR_get0(&algObj, nullptr, nullptr, sigAlg);
    QVERIFY(algObj != nullptr);
    QCOMPARE(OBJ_obj2nid(algObj), NID_sha256WithRSAEncryption);
}

void TestCertRequestBuilder::usesSha1RsaWhenRequested()
{
    QByteArray spki = makeSpkiForFreshRsaKey();
    QVERIFY(!spki.isEmpty());

    const QByteArray req = buildNullSignedPkcs10Csr(spki,
        QStringLiteral("CN=Legacy Algo"), /*signing*/ 0);
    QVERIFY(!req.isEmpty());

    const unsigned char *p = reinterpret_cast<const unsigned char *>(req.constData());
    std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> parsed(
        d2i_X509_REQ(nullptr, &p, static_cast<long>(req.size())), X509_REQ_free);
    QVERIFY(parsed != nullptr);

    const X509_ALGOR *sigAlg = nullptr;
    X509_REQ_get0_signature(parsed.get(), nullptr, &sigAlg);
    QVERIFY(sigAlg != nullptr);
    const ASN1_OBJECT *algObj = nullptr;
    X509_ALGOR_get0(&algObj, nullptr, nullptr, sigAlg);
    QVERIFY(algObj != nullptr);
    QCOMPARE(OBJ_obj2nid(algObj), NID_sha1WithRSAEncryption);
}

void TestCertRequestBuilder::invalidSubjectDnFails()
{
    QByteArray spki = makeSpkiForFreshRsaKey();
    QVERIFY(!spki.isEmpty());

    QString err;
    // Empty DN — no entries land in the subject. We accept that today
    // (X509_NAME is non-null, just has zero entries); the firmware
    // will reject it but the builder won't. So instead, force an
    // *invalid* attribute name to hit the parseSubjectDn failure path.
    const QByteArray req = buildNullSignedPkcs10Csr(spki,
        QStringLiteral("=novalue"), 1, &err);
    QVERIFY(req.isEmpty());
    QVERIFY(!err.isEmpty());
}

QTEST_GUILESS_MAIN(TestCertRequestBuilder)
#include "test_cert_request_builder.moc"
