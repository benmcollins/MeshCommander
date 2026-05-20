// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QString>

namespace qumesh::app {

/// Wrap a secret string (AMT password, SSH password, SSH key
/// passphrase) before writing it to `computers.json`, and unwrap on
/// read. Three envelope formats are recognised on the read path; the
/// write path picks the strongest available.
///
/// - **v3** (new in #306): `v3:<iv_hex><ct_hex>`. AES-256-CTR with
///   a per-user master key held in the platform keystore (macOS
///   Keychain, Windows DPAPI-wrapped file, Linux 0600 file under
///   `$XDG_DATA_HOME/qumesh/`). The on-disk file no longer holds
///   the AES key — moving `computers.json` to another user or
///   machine renders the secrets undecryptable.
/// - **v2** (legacy): `v2:<key_hex><iv_hex><ct_hex>`. AES-256-CTR
///   with the key inline. Obfuscation only; anyone with read access
///   to the file can decrypt. Preserved for round-trip compatibility
///   with the legacy NW.js MeshCommander; new code only emits this
///   when the platform keystore is unavailable (rare — would need a
///   stripped-down system with no writable XDG home).
/// - **bare**: pre-codec data without any prefix. Returned verbatim
///   from `decode` so hand-edited configs from before SecretCodec
///   existed still load. The next save through the codec rewraps in
///   v3 / v2.
///
/// `encode` returns the empty string when given the empty string, so
/// optional fields don't pay encryption cost.
///
/// Defense-in-depth: pairs with #273 (computers.json is 0600 on POSIX)
/// so even the weakest fallback (v2) limits the attack surface to the
/// file owner.
class SecretCodec
{
public:
    [[nodiscard]] static QString encode(const QString &plaintext);
    [[nodiscard]] static QString decode(const QString &stored);
};

} // namespace qumesh::app
