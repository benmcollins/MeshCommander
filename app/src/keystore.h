// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>

namespace qumesh::app {

/// Per-user master key holder. Backs the v3 SecretCodec envelope.
///
/// On first call, generates a random 32-byte AES key and stashes it
/// in the platform's keystore (Keychain on macOS, DPAPI-wrapped file
/// on Windows, 0600 file under XDG_DATA_HOME on Linux). Subsequent
/// calls return the cached key without round-tripping to the
/// platform store again.
///
/// An empty return means the platform store is unavailable (which
/// can happen on a stripped-down Linux desktop with no writable XDG
/// home, or if the user clears the Keychain item, etc.). Callers
/// fall back to the legacy v2 envelope so secrets stay readable.
/// See #306.
class KeyStore
{
public:
    /// Returns the cached 32-byte master key, or generates+stores a
    /// fresh one on first call. Empty on platform failure.
    [[nodiscard]] static QByteArray masterKey();

    /// Test hook — drops the cached key without touching the platform
    /// store. Forces the next `masterKey()` call to round-trip the
    /// store, useful for testing the load+generate paths.
    static void resetCacheForTest();

private:
    /// Platform-specific. Returns the stored key, or empty if no
    /// item exists yet (and not as an error condition — that's
    /// distinguished from real failure by writing back via
    /// `platformStore` after generating).
    [[nodiscard]] static QByteArray platformLoad();

    /// Platform-specific. Returns true on success.
    [[nodiscard]] static bool platformStore(const QByteArray &key);
};

} // namespace qumesh::app
