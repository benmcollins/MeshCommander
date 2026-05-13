#include "migrate/legacy_decrypt.h"

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <QCryptographicHash>

namespace meshcommander::migrate {

namespace {

constexpr int kAesKeyLen = 32; // AES-256
constexpr int kAesIvLen = 16;  // AES-CTR IV/counter is 16 bytes
constexpr int kV2KeyHexLen = kAesKeyLen * 2;
constexpr int kV2IvHexLen = kAesIvLen * 2;
constexpr int kLegacyPasswordHexLen = 512; // 256 raw bytes of password

constexpr char kV2Prefix[] = "v2:";
constexpr int kV2PrefixLen = 3;

QString opensslError()
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

/// Run AES-256-CTR decrypt/encrypt with the supplied key and IV.
/// Returns an empty QByteArray on failure (with `error` populated).
QByteArray aes256CtrCrypt(QByteArrayView key, QByteArrayView iv, QByteArrayView in,
                          bool encrypt, QString *error)
{
    Q_ASSERT(key.size() == kAesKeyLen);
    Q_ASSERT(iv.size() == kAesIvLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        if (error != nullptr) *error = QStringLiteral("EVP_CIPHER_CTX_new: ") + opensslError();
        return {};
    }

    const int init = EVP_CipherInit_ex(
        ctx, EVP_aes_256_ctr(), nullptr,
        reinterpret_cast<const unsigned char *>(key.data()),
        reinterpret_cast<const unsigned char *>(iv.data()),
        encrypt ? 1 : 0);
    if (init != 1) {
        if (error != nullptr) *error = QStringLiteral("EVP_CipherInit_ex: ") + opensslError();
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    QByteArray out;
    out.resize(static_cast<int>(in.size()) + kAesIvLen); // CTR is stream-like; reserve a block
    int outLen = 0;
    if (EVP_CipherUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()), &outLen,
                         reinterpret_cast<const unsigned char *>(in.data()),
                         static_cast<int>(in.size())) != 1) {
        if (error != nullptr) *error = QStringLiteral("EVP_CipherUpdate: ") + opensslError();
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int finalLen = 0;
    if (EVP_CipherFinal_ex(ctx, reinterpret_cast<unsigned char *>(out.data()) + outLen,
                           &finalLen) != 1) {
        if (error != nullptr) *error = QStringLiteral("EVP_CipherFinal_ex: ") + opensslError();
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
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!ok) return false;
    }
    return true;
}

QByteArray fromHex(QByteArrayView v)
{
    return QByteArray::fromHex(QByteArray(v.data(), v.size()));
}

} // namespace

std::pair<QByteArray, QByteArray> LegacyCrypto::evpBytesToKeyMd5(
    QByteArrayView password, int keyLen, int ivLen)
{
    QByteArray out;
    out.reserve(keyLen + ivLen);

    QByteArray last; // empty on first round
    while (out.size() < keyLen + ivLen) {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(last);
        md5.addData(QByteArray(password.data(), password.size()));
        last = md5.result();
        out.append(last);
    }

    QByteArray key = out.left(keyLen);
    QByteArray iv = out.mid(keyLen, ivLen);
    return {std::move(key), std::move(iv)};
}

LegacyCrypto::Format LegacyCrypto::detectFormat(QByteArrayView ciphertext)
{
    if (ciphertext.size() >= kV2PrefixLen &&
        std::memcmp(ciphertext.data(), kV2Prefix, kV2PrefixLen) == 0) {
        return Format::V2;
    }
    if (ciphertext.size() > kLegacyPasswordHexLen &&
        isHex(ciphertext.first(kLegacyPasswordHexLen))) {
        return Format::Legacy;
    }
    return Format::Unknown;
}

LegacyCrypto::DecryptResult LegacyCrypto::decrypt(QByteArrayView ciphertext)
{
    DecryptResult result;
    result.format = detectFormat(ciphertext);

    if (result.format == Format::V2) {
        QByteArrayView rest = ciphertext.sliced(kV2PrefixLen);
        if (rest.size() < kV2KeyHexLen + kV2IvHexLen) {
            result.error = QStringLiteral("v2 envelope is too short to contain key+iv");
            return result;
        }
        QByteArray key = fromHex(rest.first(kV2KeyHexLen));
        QByteArray iv = fromHex(rest.sliced(kV2KeyHexLen, kV2IvHexLen));
        QByteArray ct = fromHex(rest.sliced(kV2KeyHexLen + kV2IvHexLen));
        if (key.size() != kAesKeyLen || iv.size() != kAesIvLen) {
            result.error = QStringLiteral("v2 envelope has malformed hex key/iv");
            return result;
        }
        result.plaintext = aes256CtrCrypt(key, iv, ct, /*encrypt=*/false, &result.error);
        return result;
    }

    if (result.format == Format::Legacy) {
        QByteArrayView pwdHex = ciphertext.first(kLegacyPasswordHexLen);
        QByteArrayView ctHex = ciphertext.sliced(kLegacyPasswordHexLen);
        if (!isHex(ctHex)) {
            result.error = QStringLiteral("legacy envelope ciphertext is not valid hex");
            return result;
        }
        auto [key, iv] = evpBytesToKeyMd5(pwdHex.toByteArray(), kAesKeyLen, kAesIvLen);
        QByteArray ct = fromHex(ctHex);
        result.plaintext = aes256CtrCrypt(key, iv, ct, /*encrypt=*/false, &result.error);
        return result;
    }

    result.error = QStringLiteral("ciphertext does not match any known MeshCommander envelope");
    return result;
}

QByteArray LegacyCrypto::encryptV2(QByteArrayView plaintext)
{
    QByteArray key(kAesKeyLen, Qt::Uninitialized);
    QByteArray iv(kAesIvLen, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(key.data()), kAesKeyLen) != 1 ||
        RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), kAesIvLen) != 1) {
        return {};
    }

    QString err;
    QByteArray ct = aes256CtrCrypt(key, iv, plaintext, /*encrypt=*/true, &err);
    if (ct.isEmpty() && !plaintext.isEmpty()) return {};

    QByteArray out;
    out.reserve(kV2PrefixLen + kV2KeyHexLen + kV2IvHexLen + ct.size() * 2);
    out.append(kV2Prefix, kV2PrefixLen);
    out.append(key.toHex());
    out.append(iv.toHex());
    out.append(ct.toHex());
    return out;
}

} // namespace meshcommander::migrate
