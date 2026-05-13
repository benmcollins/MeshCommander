// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "certs/cert_entry.h"
#include "certs/cert_parser.h"
#include "certs/cert_store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

using namespace meshcommander::certs;

namespace {

/// Generate a fresh self-signed X.509 cert with a 2048-bit RSA key.
/// Returns the cert in DER, the key in PEM, and a PKCS#12 bundle
/// protected by `password`. All three reference the same key pair.
struct GeneratedCert {
    QByteArray der;
    QString keyPem;
    QByteArray p12;
};

GeneratedCert generateSelfSigned(const QString &cn, const QString &password)
{
    GeneratedCert g;

    EVP_PKEY *pkey = EVP_RSA_gen(2048);
    Q_ASSERT(pkey != nullptr);

    X509 *x = X509_new();
    Q_ASSERT(x != nullptr);
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_getm_notBefore(x), 0);
    X509_gmtime_adj(X509_getm_notAfter(x), 60L * 60 * 24 * 365);
    X509_set_pubkey(x, pkey);
    X509_NAME *name = X509_get_subject_name(x);
    const QByteArray cnUtf = cn.toUtf8();
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
                                 reinterpret_cast<const unsigned char *>(cnUtf.constData()),
                                 -1, -1, 0);
    X509_set_issuer_name(x, name);
    X509_sign(x, pkey, EVP_sha256());

    unsigned char *derBuf = nullptr;
    const int derLen = i2d_X509(x, &derBuf);
    Q_ASSERT(derLen > 0 && derBuf != nullptr);
    g.der = QByteArray(reinterpret_cast<const char *>(derBuf), derLen);
    OPENSSL_free(derBuf);

    BIO *keyBio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(keyBio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    char *keyData = nullptr;
    const long keyLen = BIO_get_mem_data(keyBio, &keyData);
    g.keyPem = QString::fromLatin1(keyData, static_cast<int>(keyLen));
    BIO_free(keyBio);

    const QByteArray pw = password.toUtf8();
    PKCS12 *p = PKCS12_create(pw.constData(), "test", pkey, x, nullptr,
                                0, 0, 0, 0, 0);
    Q_ASSERT(p != nullptr);
    BIO *p12Bio = BIO_new(BIO_s_mem());
    i2d_PKCS12_bio(p12Bio, p);
    char *p12Data = nullptr;
    const long p12Len = BIO_get_mem_data(p12Bio, &p12Data);
    g.p12 = QByteArray(p12Data, static_cast<int>(p12Len));
    BIO_free(p12Bio);
    PKCS12_free(p);
    X509_free(x);
    EVP_PKEY_free(pkey);
    return g;
}

} // namespace

class TestCertStore : public QObject
{
    Q_OBJECT
private slots:
    void parserFromDerExtractsMetadata();
    void parserFromPemRoundTripsCertAndKey();
    void parserFromPkcs12ExtractsCertAndKey();
    void parserRejectsBadPkcs12Password();
    void storeRoundTripsEntries();
    void storeAddOrReplaceAndRemove();
};

void TestCertStore::parserFromDerExtractsMetadata()
{
    const GeneratedCert g = generateSelfSigned(QStringLiteral("qumesh-test"),
                                                 QStringLiteral("pw"));
    QString err;
    const CertEntry e = CertParser::fromDer(g.der, &err);
    QVERIFY2(!e.certDer.isEmpty(), qPrintable(err));
    QCOMPARE(e.subjectCommonName, QStringLiteral("qumesh-test"));
    QCOMPARE(e.issuerCommonName, QStringLiteral("qumesh-test"));
    QVERIFY(e.notBefore.isValid());
    QVERIFY(e.notAfter > e.notBefore);
    QVERIFY(e.fingerprintSha256.contains(QLatin1Char(':')));
    QCOMPARE(e.id.length(), 64); // sha256 hex
    QVERIFY(!e.hasPrivateKey);
}

void TestCertStore::parserFromPemRoundTripsCertAndKey()
{
    const GeneratedCert g = generateSelfSigned(QStringLiteral("pem-test"),
                                                 QStringLiteral("pw"));
    CertEntry base = CertParser::fromDer(g.der, nullptr);
    base.privateKeyPem = g.keyPem;
    base.hasPrivateKey = true;

    QByteArray pem = CertParser::toPem(base);
    QVERIFY(pem.contains("BEGIN CERTIFICATE"));
    QVERIFY(pem.contains("PRIVATE KEY"));

    QString err;
    const CertEntry parsed = CertParser::fromPem(pem, &err);
    QVERIFY2(!parsed.certDer.isEmpty(), qPrintable(err));
    QCOMPARE(parsed.subjectCommonName, QStringLiteral("pem-test"));
    QVERIFY(parsed.hasPrivateKey);
    QVERIFY(parsed.privateKeyPem.contains(QStringLiteral("PRIVATE KEY")));
}

void TestCertStore::parserFromPkcs12ExtractsCertAndKey()
{
    const GeneratedCert g = generateSelfSigned(QStringLiteral("p12-test"),
                                                 QStringLiteral("hunter2"));
    QString err;
    const CertEntry e = CertParser::fromPkcs12(g.p12, QStringLiteral("hunter2"), &err);
    QVERIFY2(!e.certDer.isEmpty(), qPrintable(err));
    QCOMPARE(e.subjectCommonName, QStringLiteral("p12-test"));
    QVERIFY(e.hasPrivateKey);
    QVERIFY(e.privateKeyPem.contains(QStringLiteral("PRIVATE KEY")));
}

void TestCertStore::parserRejectsBadPkcs12Password()
{
    const GeneratedCert g = generateSelfSigned(QStringLiteral("p12-bad"),
                                                 QStringLiteral("correct"));
    QString err;
    const CertEntry e = CertParser::fromPkcs12(g.p12, QStringLiteral("wrong"), &err);
    QVERIFY(e.certDer.isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestCertStore::storeRoundTripsEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("certificates.json"));

    const GeneratedCert g1 = generateSelfSigned(QStringLiteral("one"),
                                                  QStringLiteral("pw"));
    const GeneratedCert g2 = generateSelfSigned(QStringLiteral("two"),
                                                  QStringLiteral("pw"));

    {
        CertStore s(path);
        QVERIFY(s.load());
        QCOMPARE(s.entries().size(), 0);
        CertEntry e1 = CertParser::fromDer(g1.der, nullptr);
        CertEntry e2 = CertParser::fromDer(g2.der, nullptr);
        e2.privateKeyPem = g2.keyPem;
        e2.hasPrivateKey = true;
        s.addOrReplace(e1);
        s.addOrReplace(e2);
        QVERIFY(s.save());
    }

    CertStore s2(path);
    QString err;
    QVERIFY2(s2.load(&err), qPrintable(err));
    QCOMPARE(s2.entries().size(), 2);
    QCOMPARE(s2.entries().at(0).subjectCommonName, QStringLiteral("one"));
    QCOMPARE(s2.entries().at(1).subjectCommonName, QStringLiteral("two"));
    QVERIFY(s2.entries().at(1).hasPrivateKey);
    QVERIFY(s2.entries().at(1).privateKeyPem.contains(QStringLiteral("PRIVATE KEY")));
    QCOMPARE(s2.entries().at(0).id.length(), 64);
}

void TestCertStore::storeAddOrReplaceAndRemove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("certificates.json"));

    const GeneratedCert g = generateSelfSigned(QStringLiteral("repl-test"),
                                                 QStringLiteral("pw"));
    CertEntry e = CertParser::fromDer(g.der, nullptr);

    CertStore s(path);
    QVERIFY(s.load());
    QCOMPARE(s.addOrReplace(e), 0);
    QCOMPARE(s.entries().size(), 1);
    QCOMPARE(s.addOrReplace(e), 0); // same id -> replace
    QCOMPARE(s.entries().size(), 1);

    QVERIFY(s.removeById(e.id));
    QCOMPARE(s.entries().size(), 0);
    QVERIFY(!s.removeById(e.id));
}

QTEST_GUILESS_MAIN(TestCertStore)
#include "test_cert_store.moc"
