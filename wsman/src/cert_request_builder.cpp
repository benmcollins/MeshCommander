// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/cert_request_builder.h"

#include <QHash>
#include <QStringList>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <memory>

namespace qumesh::wsman {

namespace {

// RAII wrappers — OpenSSL handles plain unique_ptr fine with their
// matching `*_free` deleters. Keeps the body below readable instead of
// turning into a forest of goto-cleanup.
struct X509ReqDeleter      { void operator()(X509_REQ *p)  const noexcept { if (p) X509_REQ_free(p); } };
struct X509NameDeleter     { void operator()(X509_NAME *p) const noexcept { if (p) X509_NAME_free(p); } };
struct EvpPkeyDeleter      { void operator()(EVP_PKEY *p)  const noexcept { if (p) EVP_PKEY_free(p); } };
struct AsnBitStringDeleter { void operator()(ASN1_BIT_STRING *p) const noexcept { if (p) ASN1_BIT_STRING_free(p); } };

using X509ReqPtr      = std::unique_ptr<X509_REQ, X509ReqDeleter>;
using X509NamePtr     = std::unique_ptr<X509_NAME, X509NameDeleter>;
using EvpPkeyPtr      = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using AsnBitStringPtr = std::unique_ptr<ASN1_BIT_STRING, AsnBitStringDeleter>;

QString opensslError()
{
    QString out;
    unsigned long e;
    char buf[256];
    while ((e = ERR_get_error()) != 0) {
        ERR_error_string_n(e, buf, sizeof(buf));
        if (!out.isEmpty()) out.append(QStringLiteral("; "));
        out.append(QString::fromLatin1(buf));
    }
    return out;
}

// Parse a Subject DN string in the RFC 4514-ish "CN=Foo, O=Bar"
// shape used by every other AMT cert-bearing call in the project.
// Comma-separated key=value pairs, whitespace trimmed. Returns an
// owned `X509_NAME *` (caller takes ownership via the unique_ptr).
X509NamePtr parseSubjectDn(const QString &dn)
{
    X509NamePtr name(X509_NAME_new());
    if (!name) return {};

    // Splitting on commas is good-enough for the DN shapes operators
    // will paste here (no DNs with embedded commas in CNs in the AMT
    // ecosystem); fancy unescaping is out of scope. Trim once.
    const QStringList parts = dn.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &raw : parts) {
        const QString pair = raw.trimmed();
        const int eq = pair.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        const QByteArray key = pair.left(eq).trimmed().toUtf8();
        const QByteArray val = pair.mid(eq + 1).trimmed().toUtf8();
        if (key.isEmpty() || val.isEmpty()) continue;

        if (X509_NAME_add_entry_by_txt(name.get(),
                                       key.constData(),
                                       MBSTRING_UTF8,
                                       reinterpret_cast<const unsigned char *>(val.constData()),
                                       static_cast<int>(val.size()),
                                       -1, 0) != 1) {
            return {};
        }
    }
    return name;
}

// Map our `signingAlgorithm` enum onto the OpenSSL NID for the
// matching combined-with-RSA signature OID. AMT only supports two:
// 0 = SHA1-RSA (removed in CSME 18.0+), 1 = SHA256-RSA. Others
// fall back to SHA256.
int signatureNidFor(int signingAlgorithm)
{
    switch (signingAlgorithm) {
    case 0: return NID_sha1WithRSAEncryption;
    case 1: return NID_sha256WithRSAEncryption;
    default: return NID_sha256WithRSAEncryption;
    }
}

} // namespace

QByteArray buildNullSignedPkcs10Csr(const QByteArray &publicKeyDer,
                                     const QString &subjectDn,
                                     int signingAlgorithm,
                                     QString *error)
{
    if (publicKeyDer.isEmpty()) {
        if (error) *error = QStringLiteral("empty public key");
        return {};
    }

    // Decode the SPKI bytes into an EVP_PKEY. `d2i_PUBKEY` advances
    // its input pointer; we have to take a const-correct ref to the
    // buffer's start since we don't actually own the underlying
    // memory.
    const unsigned char *p = reinterpret_cast<const unsigned char *>(publicKeyDer.constData());
    EvpPkeyPtr pubKey(d2i_PUBKEY(nullptr, &p,
                                   static_cast<long>(publicKeyDer.size())));
    if (!pubKey) {
        if (error) *error = QStringLiteral("d2i_PUBKEY failed: %1").arg(opensslError());
        return {};
    }

    X509ReqPtr req(X509_REQ_new());
    if (!req) {
        if (error) *error = QStringLiteral("X509_REQ_new failed");
        return {};
    }

    // PKCS#10 v1 is version 0 (zero-indexed). AMT firmware rejects
    // anything else.
    if (X509_REQ_set_version(req.get(), 0) != 1) {
        if (error) *error = QStringLiteral("X509_REQ_set_version failed");
        return {};
    }

    X509NamePtr subject = parseSubjectDn(subjectDn);
    if (!subject) {
        if (error) *error = QStringLiteral("invalid Subject DN: %1").arg(subjectDn);
        return {};
    }
    // AMT rejects requests with an empty Subject DN. Catch that here
    // instead of building a useless template the firmware will fault on.
    if (X509_NAME_entry_count(subject.get()) <= 0) {
        if (error) *error = QStringLiteral(
            "Subject DN parsed to zero attributes: \"%1\"").arg(subjectDn);
        return {};
    }
    if (X509_REQ_set_subject_name(req.get(), subject.get()) != 1) {
        if (error) *error = QStringLiteral("X509_REQ_set_subject_name failed");
        return {};
    }

    if (X509_REQ_set_pubkey(req.get(), pubKey.get()) != 1) {
        if (error) *error = QStringLiteral("X509_REQ_set_pubkey failed");
        return {};
    }

    // Set the signature algorithm field. OpenSSL's `X509_REQ_sign`
    // would do this for us — but it would also produce a real
    // signature with a key we don't have. Instead we reach inside the
    // CertificationRequest and set the algorithm identifier directly,
    // then leave the signature bits at their default empty value;
    // AMT firmware fills those in with the matching private key.
    //
    // The fields we touch are accessor-exposed since OpenSSL 1.1 —
    // older OpenSSL is below our supported floor (Qt 6.6 needs 1.1+
    // anyway).
    {
        ASN1_OBJECT *algObj = OBJ_nid2obj(signatureNidFor(signingAlgorithm));
        if (algObj == nullptr) {
            if (error) *error = QStringLiteral("OBJ_nid2obj failed");
            return {};
        }
        const X509_ALGOR *sigAlgConst = nullptr;
        const ASN1_BIT_STRING *sigConst = nullptr;
        X509_REQ_get0_signature(req.get(), &sigConst, &sigAlgConst);
        if (sigAlgConst == nullptr) {
            // Some OpenSSL builds (LibreSSL etc.) return null here
            // until the request has been serialized once. Fall back
            // to a no-op signature set to flush the lazy state.
            // OBJ_nid2obj returned a static pointer — no free needed.
        }
        // Drop const because X509_ALGOR_set0 takes a non-const pointer.
        // OpenSSL keeps the field allocation on the req; updating
        // through the non-const ref is safe and matches what
        // X509_REQ_sign does internally.
        X509_ALGOR *sigAlg = const_cast<X509_ALGOR *>(sigAlgConst);
        if (sigAlg != nullptr) {
            // X509_ALGOR_set0 takes ownership of the OID object; pass
            // a fresh copy so we don't double-free across req destroy.
            ASN1_OBJECT *algObjCopy = OBJ_dup(algObj);
            if (algObjCopy == nullptr) {
                if (error) *error = QStringLiteral("OBJ_dup failed");
                return {};
            }
            if (X509_ALGOR_set0(sigAlg, algObjCopy, V_ASN1_NULL, nullptr) != 1) {
                ASN1_OBJECT_free(algObjCopy);
                if (error) *error = QStringLiteral("X509_ALGOR_set0 failed");
                return {};
            }
        }
    }

    // Serialize. The signature field is left as the empty BIT STRING
    // OpenSSL writes by default; AMT firmware accepts that as the
    // null-signed marker and replaces it with the real signature when
    // it processes GeneratePKCS10RequestEx.
    unsigned char *out = nullptr;
    const int len = i2d_X509_REQ(req.get(), &out);
    if (len <= 0 || out == nullptr) {
        if (error) *error = QStringLiteral("i2d_X509_REQ failed: %1").arg(opensslError());
        if (out != nullptr) OPENSSL_free(out);
        return {};
    }
    QByteArray result(reinterpret_cast<const char *>(out), len);
    OPENSSL_free(out);
    return result;
}

} // namespace qumesh::wsman
