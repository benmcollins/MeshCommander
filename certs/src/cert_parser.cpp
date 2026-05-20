// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "certs/cert_parser.h"

#include <QCryptographicHash>
#include <QSslCertificate>
#include <QSslKey>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

namespace qumesh::certs {

namespace {

QString commonNameFromList(const QStringList &cns)
{
    return cns.isEmpty() ? QString() : cns.first();
}

QString hexColonFingerprint(const QByteArray &hash)
{
    QString out;
    out.reserve(hash.size() * 3);
    for (int i = 0; i < hash.size(); ++i) {
        if (i > 0) out += QLatin1Char(':');
        const quint8 b = static_cast<quint8>(hash.at(i));
        out += QString::asprintf("%02X", b);
    }
    return out;
}

} // namespace

QString CertParser::idForDer(const QByteArray &der)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(der, QCryptographicHash::Sha256).toHex());
}

bool CertParser::populateMetadata(CertEntry *entry, QString *error)
{
    Q_ASSERT(entry != nullptr);
    if (entry->certDer.isEmpty()) {
        if (error) *error = QStringLiteral("empty DER blob");
        return false;
    }
    const QSslCertificate qcert(entry->certDer, QSsl::Der);
    if (qcert.isNull()) {
        if (error) *error = QStringLiteral("not a valid X.509 certificate");
        return false;
    }
    entry->id = idForDer(entry->certDer);
    entry->subjectCommonName = commonNameFromList(qcert.subjectInfo(QSslCertificate::CommonName));
    entry->issuerCommonName  = commonNameFromList(qcert.issuerInfo(QSslCertificate::CommonName));
    entry->serial            = QString::fromLatin1(qcert.serialNumber());
    entry->notBefore         = qcert.effectiveDate();
    entry->notAfter          = qcert.expiryDate();
    entry->fingerprintSha256 = hexColonFingerprint(
        QCryptographicHash::hash(entry->certDer, QCryptographicHash::Sha256));
    return true;
}

CertEntry CertParser::fromDer(const QByteArray &der, QString *error)
{
    CertEntry e;
    e.certDer = der;
    if (!populateMetadata(&e, error)) return CertEntry{};
    return e;
}

CertEntry CertParser::fromPem(const QByteArray &pem, QString *error)
{
    const QList<QSslCertificate> chain = QSslCertificate::fromData(pem, QSsl::Pem);
    if (chain.isEmpty()) {
        if (error) *error = QStringLiteral("no PEM certificate found");
        return CertEntry{};
    }
    CertEntry e;
    e.certDer = chain.first().toDer();
    if (!populateMetadata(&e, error)) return CertEntry{};

    // Look for a private key in the same PEM blob. Try the common
    // algorithms in order; QSslKey returns null on a mismatch so we
    // can probe cheaply.
    static constexpr QSsl::KeyAlgorithm kAlgs[] = {
        QSsl::Rsa, QSsl::Ec, QSsl::Dsa, QSsl::Dh,
    };
    for (QSsl::KeyAlgorithm alg : kAlgs) {
        QSslKey k(pem, alg, QSsl::Pem);
        if (!k.isNull()) {
            e.privateKeyPem = QString::fromLatin1(k.toPem());
            e.hasPrivateKey = true;
            break;
        }
    }
    return e;
}

CertEntry CertParser::fromPkcs12(const QByteArray &p12, const QString &password,
                                  QString *error)
{
    CertEntry e;
    BIO *bio = BIO_new_mem_buf(p12.constData(), static_cast<int>(p12.size()));
    if (bio == nullptr) {
        if (error) *error = QStringLiteral("OpenSSL BIO_new_mem_buf failed");
        return e;
    }
    PKCS12 *pkcs = d2i_PKCS12_bio(bio, nullptr);
    BIO_free(bio);
    if (pkcs == nullptr) {
        if (error) *error = QStringLiteral("not a valid PKCS#12 bundle");
        return e;
    }

    EVP_PKEY *pkey = nullptr;
    X509 *cert = nullptr;
    STACK_OF(X509) *ca = nullptr;
    const QByteArray pw = password.toUtf8();
    const int parsed = PKCS12_parse(pkcs, pw.constData(), &pkey, &cert, &ca);
    PKCS12_free(pkcs);
    if (parsed == 0 || cert == nullptr) {
        if (error) {
            // OpenSSL's error stack may hold one (typically "wrong
            // password") or several entries; the bare ERR_get_error()
            // pop returns 0 when the queue is empty, and
            // ERR_error_string_n(0, …) yields a meaningless "no error"
            // — that's what made "wrong PKCS#12 password" especially
            // hard to diagnose pre-#292. Peek first; drain everything
            // if anything is queued; otherwise fall back to a generic
            // message.
            if (ERR_peek_last_error() != 0) {
                QStringList msgs;
                unsigned long e2 = 0;
                while ((e2 = ERR_get_error()) != 0) {
                    char buf[256];
                    ERR_error_string_n(e2, buf, sizeof(buf));
                    msgs << QString::fromLatin1(buf);
                }
                *error = QStringLiteral("PKCS#12 parse failed: %1")
                             .arg(msgs.join(QStringLiteral("; ")));
            } else {
                *error = QStringLiteral("PKCS#12 parse failed "
                                        "(wrong password?)");
            }
        }
        if (pkey) EVP_PKEY_free(pkey);
        if (cert) X509_free(cert);
        if (ca) sk_X509_pop_free(ca, X509_free);
        return e;
    }

    // Serialise cert to DER.
    unsigned char *derBuf = nullptr;
    const int derLen = i2d_X509(cert, &derBuf);
    if (derLen > 0 && derBuf != nullptr) {
        e.certDer = QByteArray(reinterpret_cast<const char *>(derBuf), derLen);
        OPENSSL_free(derBuf);
    }
    X509_free(cert);

    // Serialise private key as PEM.
    if (pkey != nullptr) {
        BIO *keyBio = BIO_new(BIO_s_mem());
        if (keyBio != nullptr
            && PEM_write_bio_PrivateKey(keyBio, pkey, nullptr, nullptr, 0, nullptr, nullptr)
                  > 0) {
            char *data = nullptr;
            const long n = BIO_get_mem_data(keyBio, &data);
            if (n > 0 && data != nullptr) {
                e.privateKeyPem = QString::fromLatin1(data, static_cast<int>(n));
                e.hasPrivateKey = true;
            }
        }
        if (keyBio) BIO_free(keyBio);
        EVP_PKEY_free(pkey);
    }
    if (ca) sk_X509_pop_free(ca, X509_free);

    if (!populateMetadata(&e, error)) return CertEntry{};
    return e;
}

QByteArray CertParser::toPem(const CertEntry &entry)
{
    if (entry.certDer.isEmpty()) return {};
    QSslCertificate qcert(entry.certDer, QSsl::Der);
    if (qcert.isNull()) return {};
    QByteArray out = qcert.toPem();
    if (!entry.privateKeyPem.isEmpty()) {
        if (!out.endsWith('\n')) out += '\n';
        out += entry.privateKeyPem.toLatin1();
    }
    return out;
}

} // namespace qumesh::certs
