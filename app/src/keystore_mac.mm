// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "keystore.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <QByteArray>
#include <QString>

namespace qumesh::app {

namespace {

NSString *const kService = @"qumesh.secret-codec";
NSString *const kAccount = @"v3-master-key";

NSDictionary *baseQuery()
{
    return @{
        (id)kSecClass:       (id)kSecClassGenericPassword,
        (id)kSecAttrService: kService,
        (id)kSecAttrAccount: kAccount,
    };
}

} // namespace

QByteArray KeyStore::platformLoad()
{
    NSMutableDictionary *q = [baseQuery() mutableCopy];
    q[(id)kSecReturnData] = @YES;
    q[(id)kSecMatchLimit] = (id)kSecMatchLimitOne;

    CFTypeRef out = NULL;
    OSStatus rc = SecItemCopyMatching((CFDictionaryRef)q, &out);
    if (rc != errSecSuccess || out == NULL) return {};

    NSData *data = (__bridge_transfer NSData *)out;
    return QByteArray(static_cast<const char *>([data bytes]),
                       static_cast<qsizetype>([data length]));
}

bool KeyStore::platformStore(const QByteArray &key)
{
    NSData *blob = [NSData dataWithBytes:key.constData() length:key.size()];

    NSMutableDictionary *q = [baseQuery() mutableCopy];
    q[(id)kSecValueData] = blob;
    // kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly is the
    // standard accessibility class for app-managed secrets that
    // shouldn't migrate via iCloud Keychain and don't need to be
    // available before first unlock after reboot. Equivalent to the
    // "device-bound, requires-login" tier other Mac apps use.
    q[(id)kSecAttrAccessible] = (id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;

    OSStatus rc = SecItemAdd((CFDictionaryRef)q, NULL);
    if (rc == errSecSuccess) return true;
    if (rc == errSecDuplicateItem) {
        // Stale item from an earlier failed init — overwrite.
        NSDictionary *update = @{ (id)kSecValueData: blob };
        rc = SecItemUpdate((CFDictionaryRef)baseQuery(),
                            (CFDictionaryRef)update);
        return rc == errSecSuccess;
    }
    return false;
}

} // namespace qumesh::app
