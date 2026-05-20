// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QString>

namespace qumesh::app {

/// Wrap a secret string (AMT password, SSH password, SSH key passphrase)
/// in the legacy v2 encryption envelope before writing it to
/// `computers.json`, and unwrap on read.
///
/// **SECURITY**: this is obfuscation, not encryption. The v2 envelope
/// is `v2:<key_hex><iv_hex><ciphertext_hex>` — the AES key is right
/// there in the file. Anyone with read access to `computers.json` can
/// decrypt the contents in three lines of code. The only protection
/// gained is:
///
///   - Secrets don't show up in plain ripgrep / Spotlight indexing.
///   - File permissions are 0600 (#273), so on POSIX the limit is the
///     file owner. On Windows `%APPDATA%` is already user-isolated.
///
/// The format is preserved (and intentionally identical to the legacy
/// NW.js MeshCommander v2 envelope) so a config that's round-tripped
/// through the migrate tool stays interoperable. The real fix — a
/// platform-keystore-derived key (Keychain / DPAPI / libsecret) — is
/// tracked as a follow-up to #274.
///
/// `encode` returns the empty string when given the empty string, so
/// optional fields don't pay encryption cost. `decode` is lenient: if
/// the input doesn't have the `v2:` prefix it's returned verbatim, on
/// the assumption that it was written before this codec existed.
class SecretCodec
{
public:
    [[nodiscard]] static QString encode(const QString &plaintext);
    [[nodiscard]] static QString decode(const QString &stored);
};

} // namespace qumesh::app
