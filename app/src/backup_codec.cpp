// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "backup_codec.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

namespace qumesh::app {

namespace {

constexpr int kAesKeyLen = 32;     // AES-256
constexpr int kGcmIvLen  = 12;     // NIST-recommended GCM nonce
constexpr int kGcmTagLen = 16;     // GCM auth tag
constexpr int kSaltLen   = 32;     // PBKDF2 salt
constexpr int kEnvelopeVersion = 1;
constexpr const char *kTypeMarker  = "qumesh-backup";
constexpr const char *kEnvelopeTag = "v1";
constexpr const char *kKdfName     = "pbkdf2-sha256";
constexpr const char *kCipherName  = "aes-256-gcm";
constexpr const char *kSchema      = "qumesh.computers/1";

QByteArray b64(const QByteArray &raw)
{
    return raw.toBase64(QByteArray::Base64Encoding);
}

QByteArray unb64(const QString &s, bool *ok)
{
    const auto r = QByteArray::fromBase64Encoding(
        s.toLatin1(), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    if (ok != nullptr) *ok = r.decodingStatus == QByteArray::Base64DecodingStatus::Ok;
    return r.decoded;
}

/// PBKDF2-HMAC-SHA256. OpenSSL native; no need to pull in libargon2.
/// Returns 32 bytes (AES-256 key) on success, empty on failure.
QByteArray derivePbkdf2(const QString &password, const QByteArray &salt, int iterations)
{
    if (password.isEmpty() || salt.size() != kSaltLen || iterations <= 0) return {};
    const QByteArray pw = password.toUtf8();
    QByteArray out(kAesKeyLen, Qt::Uninitialized);
    const int rc = PKCS5_PBKDF2_HMAC(
        pw.constData(), pw.size(),
        reinterpret_cast<const unsigned char *>(salt.constData()), salt.size(),
        iterations, EVP_sha256(),
        kAesKeyLen, reinterpret_cast<unsigned char *>(out.data()));
    if (rc != 1) return {};
    return out;
}

/// AES-256-GCM encrypt. `tag` is filled in with the 16-byte auth tag.
/// `iv` must be 12 bytes; `key` must be 32 bytes. Returns the
/// ciphertext (same length as plaintext) on success, empty on failure
/// (with `*tag` left untouched).
QByteArray gcmEncrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &plaintext, QByteArray *tag)
{
    if (key.size() != kAesKeyLen || iv.size() != kGcmIvLen || tag == nullptr) return {};

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) return {};

    QByteArray ct(plaintext.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int outLen = 0;
    int finalLen = 0;

    auto fail = [&]() {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray{};
    };

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return fail();
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kGcmIvLen, nullptr) != 1) return fail();
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                            reinterpret_cast<const unsigned char *>(key.constData()),
                            reinterpret_cast<const unsigned char *>(iv.constData())) != 1)
        return fail();
    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(ct.data()), &outLen,
                          reinterpret_cast<const unsigned char *>(plaintext.constData()),
                          static_cast<int>(plaintext.size())) != 1)
        return fail();
    if (EVP_EncryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char *>(ct.data()) + outLen,
                            &finalLen) != 1)
        return fail();

    QByteArray tagOut(kGcmTagLen, Qt::Uninitialized);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kGcmTagLen, tagOut.data()) != 1)
        return fail();

    EVP_CIPHER_CTX_free(ctx);
    ct.resize(outLen + finalLen);
    *tag = tagOut;
    return ct;
}

enum class GcmDecryptStatus { Ok, TagFailed, OpenSslError };

GcmDecryptStatus gcmDecrypt(const QByteArray &key, const QByteArray &iv,
                            const QByteArray &ciphertext, const QByteArray &tag,
                            QByteArray *plaintext)
{
    if (key.size() != kAesKeyLen || iv.size() != kGcmIvLen
        || tag.size() != kGcmTagLen || plaintext == nullptr) {
        return GcmDecryptStatus::OpenSslError;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) return GcmDecryptStatus::OpenSslError;

    QByteArray pt(ciphertext.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int outLen = 0;
    int finalLen = 0;

    auto fail = [&](GcmDecryptStatus s) {
        EVP_CIPHER_CTX_free(ctx);
        return s;
    };

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        return fail(GcmDecryptStatus::OpenSslError);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kGcmIvLen, nullptr) != 1)
        return fail(GcmDecryptStatus::OpenSslError);
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                            reinterpret_cast<const unsigned char *>(key.constData()),
                            reinterpret_cast<const unsigned char *>(iv.constData())) != 1)
        return fail(GcmDecryptStatus::OpenSslError);
    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(pt.data()), &outLen,
                          reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                          static_cast<int>(ciphertext.size())) != 1)
        return fail(GcmDecryptStatus::OpenSslError);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kGcmTagLen,
                             const_cast<char *>(tag.constData())) != 1)
        return fail(GcmDecryptStatus::OpenSslError);

    // EVP_DecryptFinal_ex returns 0 specifically when the tag check
    // fails, distinct from -1 / other negatives reserved for hard
    // errors. That's the wrong-password signal we surface to the UI.
    const int finalRc = EVP_DecryptFinal_ex(
        ctx, reinterpret_cast<unsigned char *>(pt.data()) + outLen, &finalLen);
    if (finalRc <= 0) return fail(GcmDecryptStatus::TagFailed);

    EVP_CIPHER_CTX_free(ctx);
    pt.resize(outLen + finalLen);
    *plaintext = pt;
    return GcmDecryptStatus::Ok;
}

} // namespace

BackupCodec::EncodeResult BackupCodec::encode(const QJsonArray &computers,
                                              const QString &password)
{
    EncodeResult r;
    if (password.isEmpty()) {
        r.error = QStringLiteral("password is required");
        return r;
    }
    if (password.size() > kMaxPasswordChars) {
        r.error = QStringLiteral("password is unreasonably long");
        return r;
    }

    QByteArray salt(kSaltLen, Qt::Uninitialized);
    QByteArray iv(kGcmIvLen, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(salt.data()), kSaltLen) != 1
        || RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), kGcmIvLen) != 1) {
        r.error = QStringLiteral("openssl RAND_bytes failed");
        return r;
    }

    const QByteArray key = derivePbkdf2(password, salt, kPbkdf2Iterations);
    if (key.size() != kAesKeyLen) {
        r.error = QStringLiteral("PBKDF2 derivation failed");
        return r;
    }

    const QJsonDocument payloadDoc(QJsonObject{
        {QStringLiteral("schema"), QString::fromLatin1(kSchema)},
        {QStringLiteral("computers"), computers},
    });
    const QByteArray plaintext = payloadDoc.toJson(QJsonDocument::Compact);

    QByteArray tag;
    const QByteArray ct = gcmEncrypt(key, iv, plaintext, &tag);
    if (ct.size() != plaintext.size() || tag.size() != kGcmTagLen) {
        r.error = QStringLiteral("AES-GCM encrypt failed");
        return r;
    }

    const QJsonObject envelope{
        {QStringLiteral("type"), QString::fromLatin1(kTypeMarker)},
        {QStringLiteral("version"), kEnvelopeVersion},
        {QStringLiteral("createdAt"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("envelope"), QString::fromLatin1(kEnvelopeTag)},
        {QStringLiteral("kdf"), QJsonObject{
            {QStringLiteral("name"), QString::fromLatin1(kKdfName)},
            {QStringLiteral("iterations"), kPbkdf2Iterations},
            {QStringLiteral("salt"), QString::fromLatin1(b64(salt))},
        }},
        {QStringLiteral("cipher"), QJsonObject{
            {QStringLiteral("name"), QString::fromLatin1(kCipherName)},
            {QStringLiteral("iv"), QString::fromLatin1(b64(iv))},
            {QStringLiteral("tag"), QString::fromLatin1(b64(tag))},
        }},
        {QStringLiteral("ciphertext"), QString::fromLatin1(b64(ct))},
    };

    r.bytes = QJsonDocument(envelope).toJson(QJsonDocument::Indented);
    return r;
}

BackupCodec::DecodeResult BackupCodec::decode(QByteArrayView fileBytes,
                                              const QString &password)
{
    DecodeResult r;
    auto fail = [&](FailReason why, const char *msg) {
        r.reason = why;
        r.error = QString::fromLatin1(msg);
        return r;
    };

    if (password.isEmpty()) return fail(FailReason::BadPassword, "password is required");
    if (password.size() > kMaxPasswordChars)
        return fail(FailReason::BadPassword, "password is unreasonably long");
    if (fileBytes.size() > kMaxFileBytes)
        return fail(FailReason::BadFormat, "backup file is too large to be a qumesh-backup");

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray(fileBytes.data(), fileBytes.size()), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return fail(FailReason::BadFormat, "file is not a JSON object");

    const QJsonObject env = doc.object();
    if (env.value(QStringLiteral("type")).toString() != QLatin1String(kTypeMarker))
        return fail(FailReason::BadFormat, "not a qumesh-backup file");
    if (env.value(QStringLiteral("version")).toInt(0) != kEnvelopeVersion)
        return fail(FailReason::BadFormat, "unsupported backup envelope version");
    if (env.value(QStringLiteral("envelope")).toString() != QLatin1String(kEnvelopeTag))
        return fail(FailReason::BadFormat, "unsupported envelope tag");

    const QJsonObject kdf = env.value(QStringLiteral("kdf")).toObject();
    if (kdf.value(QStringLiteral("name")).toString() != QLatin1String(kKdfName))
        return fail(FailReason::BadFormat, "unsupported KDF");
    const int iterations = kdf.value(QStringLiteral("iterations")).toInt(0);
    if (iterations <= 0)
        return fail(FailReason::BadFormat, "KDF iterations missing or invalid");
    // Cap the cost we'll honor from an untrusted file. The encoder
    // ships exactly `kPbkdf2Iterations` and we accept up to ~10x
    // that. Anything higher is treated as tampering — protects against
    // a malicious file that sets `iterations` to INT_MAX and freezes
    // the UI thread inside PBKDF2 for hours.
    if (iterations > kPbkdf2MaxIterations)
        return fail(FailReason::BadFormat, "KDF iteration count is unreasonable");

    bool okSalt = false, okIv = false, okTag = false, okCt = false;
    const QByteArray salt = unb64(kdf.value(QStringLiteral("salt")).toString(), &okSalt);

    const QJsonObject cipher = env.value(QStringLiteral("cipher")).toObject();
    if (cipher.value(QStringLiteral("name")).toString() != QLatin1String(kCipherName))
        return fail(FailReason::BadFormat, "unsupported cipher");
    const QByteArray iv  = unb64(cipher.value(QStringLiteral("iv")).toString(),  &okIv);
    const QByteArray tag = unb64(cipher.value(QStringLiteral("tag")).toString(), &okTag);
    const QByteArray ct  = unb64(env.value(QStringLiteral("ciphertext")).toString(), &okCt);

    if (!okSalt || !okIv || !okTag || !okCt
        || salt.size() != kSaltLen || iv.size() != kGcmIvLen || tag.size() != kGcmTagLen)
        return fail(FailReason::Corrupt, "backup file is malformed (bad base64 or sizes)");

    const QByteArray key = derivePbkdf2(password, salt, iterations);
    if (key.size() != kAesKeyLen)
        return fail(FailReason::Internal, "PBKDF2 derivation failed");

    QByteArray plaintext;
    switch (gcmDecrypt(key, iv, ct, tag, &plaintext)) {
    case GcmDecryptStatus::TagFailed:
        // GCM can't tell wrong-password from tampered file — both
        // collapse to a failed tag check. Surface as BadPassword
        // because that's overwhelmingly the common case; the UI's
        // wording handles both.
        return fail(FailReason::BadPassword, "incorrect password or tampered file");
    case GcmDecryptStatus::OpenSslError:
        return fail(FailReason::Internal, "AES-GCM decrypt failed");
    case GcmDecryptStatus::Ok:
        break;
    }

    QJsonParseError ptErr;
    const QJsonDocument ptDoc = QJsonDocument::fromJson(plaintext, &ptErr);
    if (ptErr.error != QJsonParseError::NoError || !ptDoc.isObject())
        return fail(FailReason::Corrupt, "decrypted payload is not JSON");
    const QJsonObject ptObj = ptDoc.object();
    if (ptObj.value(QStringLiteral("schema")).toString() != QLatin1String(kSchema))
        return fail(FailReason::BadFormat, "unsupported payload schema");

    r.computers = ptObj.value(QStringLiteral("computers")).toArray();
    return r;
}

} // namespace qumesh::app
