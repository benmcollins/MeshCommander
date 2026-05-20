// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "keystore.h"

#include <openssl/rand.h>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

namespace qumesh::app {

namespace {

constexpr int kKeyLenBytes = 32; // AES-256

/// Process-wide cache so subsequent encode/decode calls don't round-
/// trip to the platform store. Guarded by a small mutex — callers
/// can hit `masterKey()` from any thread.
QByteArray g_cached;
bool g_attempted = false;
QMutex g_mutex;

QByteArray generate()
{
    QByteArray out(kKeyLenBytes, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(out.data()),
                    kKeyLenBytes) != 1) {
        return {};
    }
    return out;
}

} // namespace

QByteArray KeyStore::masterKey()
{
    QMutexLocker lock(&g_mutex);
    if (g_attempted) return g_cached;
    g_attempted = true;

    // Test/CI escape hatch. CI runners hit the Keychain prompt on
    // macOS for unsigned test binaries (and block waiting for a
    // password); same risk on any sandboxed environment without
    // user interaction. Setting QUMESH_NO_KEYSTORE makes the codec
    // fall back to the v2 envelope so the suite runs unattended.
    // Production launches don't set this; release binaries are
    // signed and the Keychain prompt resolves cleanly.
    if (!qgetenv("QUMESH_NO_KEYSTORE").isEmpty()) return {};

    QByteArray k = platformLoad();
    if (k.size() == kKeyLenBytes) {
        g_cached = std::move(k);
        return g_cached;
    }

    // No existing key — generate, persist, cache. If persistence
    // fails we cache an empty key (callers fall back to v2) so we
    // don't repeatedly retry the failing store on every secret
    // encode.
    QByteArray fresh = generate();
    if (fresh.isEmpty()) return {};
    if (!platformStore(fresh)) return {};

    g_cached = std::move(fresh);
    return g_cached;
}

void KeyStore::resetCacheForTest()
{
    QMutexLocker lock(&g_mutex);
    g_cached.clear();
    g_attempted = false;
}

} // namespace qumesh::app
