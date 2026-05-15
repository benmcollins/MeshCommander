// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QString>

namespace qumesh::app {

/// Wrap a secret string (AMT password, SSH password, SSH key passphrase)
/// in the legacy v2 encryption envelope before writing it to
/// `computers.json`, and unwrap on read. This is not protection against
/// file theft — the key + IV ride along with the ciphertext — but it
/// stops the secret from showing up in plain ripgrep / Spotlight
/// indexing of the user's `Application Support` directory, and matches
/// the format the legacy NW.js app used so a config that's been
/// round-tripped through the migrate tool stays interoperable.
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
