#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

namespace meshcommander::migrate {

/// Encryption envelope used by the legacy NW.js MeshCommander to wrap the
/// `computers` and `certificates` localStorage entries.
///
/// Two on-disk formats exist:
///
///  * **Legacy** (NW.js < createCipheriv patch): 512 hex characters of
///    random password followed by the AES-256-CTR ciphertext (also hex).
///    The password is run through OpenSSL's EVP_BytesToKey with MD5, no
///    salt, one iteration to derive the 32-byte key + 16-byte IV.
///
///  * **v2** (after the createCipheriv patch in `source/Commander.htm`):
///    ASCII prefix `v2:` followed by 64 hex (key) + 32 hex (IV) + the
///    AES-256-CTR ciphertext (hex). The key and IV come from a CSPRNG.
class LegacyCrypto
{
public:
    enum class Format {
        Unknown,
        Legacy,
        V2,
    };

    struct DecryptResult {
        QByteArray plaintext;
        Format format = Format::Unknown;
        QString error;

        [[nodiscard]] bool ok() const { return error.isEmpty(); }
    };

    /// Detect the envelope format of an opaque stored value without
    /// attempting decryption.
    [[nodiscard]] static Format detectFormat(QByteArrayView ciphertext);

    /// Decrypt either envelope. Failure populates `error`; success populates
    /// `plaintext` (UTF-8) and `format`.
    [[nodiscard]] static DecryptResult decrypt(QByteArrayView ciphertext);

    /// Encrypt `plaintext` into the v2 envelope. Used for round-trip tests;
    /// the migration tool itself only reads.
    [[nodiscard]] static QByteArray encryptV2(QByteArrayView plaintext);

    /// Exposed for testability. Replicates OpenSSL's EVP_BytesToKey with the
    /// MD5 digest, no salt, one iteration: feeds `password` through MD5
    /// repeatedly until `keyLen + ivLen` bytes are produced.
    [[nodiscard]] static std::pair<QByteArray, QByteArray> evpBytesToKeyMd5(
        QByteArrayView password, int keyLen, int ivLen);

private:
    LegacyCrypto() = delete;
};

} // namespace meshcommander::migrate
