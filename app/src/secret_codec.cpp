// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "secret_codec.h"

#include "keystore.h"
#include "migrate/legacy_decrypt.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace qumesh::app {

namespace {

constexpr int kAesKeyLen = 32; // AES-256
constexpr int kAesIvLen = 16;  // AES-CTR IV/counter
constexpr char kV3Prefix[] = "v3:";
constexpr int kV3PrefixLen = 3;
constexpr int kIvHexLen = kAesIvLen * 2;

/// AES-256-CTR with an externally-supplied key. Mirrors the legacy
/// helper in `migrate/src/legacy_decrypt.cpp` but doesn't ship the
/// key inside the envelope.
QByteArray aes256CtrCrypt(const QByteArray &key, const QByteArray &iv,
                          const QByteArray &in, bool encrypt)
{
    if (key.size() != kAesKeyLen || iv.size() != kAesIvLen) return {};

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) return {};

    if (EVP_CipherInit_ex(ctx, EVP_aes_256_ctr(), nullptr,
                          reinterpret_cast<const unsigned char *>(key.constData()),
                          reinterpret_cast<const unsigned char *>(iv.constData()),
                          encrypt ? 1 : 0) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    QByteArray out;
    out.resize(in.size() + kAesIvLen);
    int outLen = 0;
    if (EVP_CipherUpdate(ctx,
                         reinterpret_cast<unsigned char *>(out.data()),
                         &outLen,
                         reinterpret_cast<const unsigned char *>(in.constData()),
                         static_cast<int>(in.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    int finalLen = 0;
    if (EVP_CipherFinal_ex(ctx,
                            reinterpret_cast<unsigned char *>(out.data()) + outLen,
                            &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen + finalLen);
    return out;
}

bool isHex(QByteArrayView v)
{
    for (qsizetype i = 0; i < v.size(); ++i) {
        const char c = v[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
              || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

/// `v3:<iv_hex><ct_hex>`. Key fetched from KeyStore — NOT stored
/// inside the envelope (this is the whole point of v3 vs v2).
QString encryptV3(const QByteArray &plaintext, const QByteArray &key)
{
    QByteArray iv(kAesIvLen, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), kAesIvLen) != 1)
        return {};
    const QByteArray ct = aes256CtrCrypt(key, iv, plaintext, /*encrypt=*/true);
    if (ct.isEmpty() && !plaintext.isEmpty()) return {};
    return QStringLiteral("v3:")
        + QString::fromLatin1(iv.toHex())
        + QString::fromLatin1(ct.toHex());
}

QByteArray decryptV3(QByteArrayView body, const QByteArray &key)
{
    if (body.size() < kIvHexLen) return {};
    QByteArrayView ivHex = body.first(kIvHexLen);
    QByteArrayView ctHex = body.sliced(kIvHexLen);
    if (!isHex(ivHex) || !isHex(ctHex)) return {};
    const QByteArray iv = QByteArray::fromHex(QByteArray(ivHex.data(), ivHex.size()));
    const QByteArray ct = QByteArray::fromHex(QByteArray(ctHex.data(), ctHex.size()));
    if (iv.size() != kAesIvLen) return {};
    return aes256CtrCrypt(key, iv, ct, /*encrypt=*/false);
}

} // namespace

QString SecretCodec::encode(const QString &plaintext)
{
    if (plaintext.isEmpty()) return {};

    // Prefer v3 when the platform keystore is available — keeps the
    // AES key out of `computers.json` so the on-disk file no longer
    // holds the decrypt material. Falls back to v2 (key+IV inline,
    // obfuscation-only) if the keystore returned nothing — that
    // keeps existing-data interoperable on a stripped-down system
    // while still beating plain-text on disk. See #306.
    const QByteArray master = KeyStore::masterKey();
    if (master.size() == kAesKeyLen) {
        QString v3 = encryptV3(plaintext.toUtf8(), master);
        if (!v3.isEmpty()) return v3;
    }

    const QByteArray wrapped = migrate::LegacyCrypto::encryptV2(plaintext.toUtf8());
    return QString::fromLatin1(wrapped);
}

QString SecretCodec::decode(const QString &stored)
{
    if (stored.isEmpty()) return {};

    if (stored.startsWith(QStringLiteral("v3:"))) {
        const QByteArray master = KeyStore::masterKey();
        if (master.size() != kAesKeyLen) return {};
        const QByteArray body = stored.mid(kV3PrefixLen).toLatin1();
        const QByteArray pt = decryptV3(body, master);
        return QString::fromUtf8(pt);
    }

    if (stored.startsWith(QStringLiteral("v2:"))) {
        const auto r = migrate::LegacyCrypto::decrypt(stored.toLatin1());
        if (!r.ok()) return {};
        return QString::fromUtf8(r.plaintext);
    }

    // Pre-codec value (no recognised prefix): trust as-is. The next
    // save will rewrap through encode(), which prefers v3.
    return stored;
}

} // namespace qumesh::app
