// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "secret_codec.h"

#include "migrate/legacy_decrypt.h"

namespace qumesh::app {

QString SecretCodec::encode(const QString &plaintext)
{
    if (plaintext.isEmpty()) return {};
    const QByteArray wrapped = migrate::LegacyCrypto::encryptV2(plaintext.toUtf8());
    return QString::fromLatin1(wrapped);
}

QString SecretCodec::decode(const QString &stored)
{
    if (stored.isEmpty()) return {};
    if (!stored.startsWith(QStringLiteral("v2:"))) {
        // Pre-codec value: trust it as-is. The next save will rewrap.
        return stored;
    }
    const auto r = migrate::LegacyCrypto::decrypt(stored.toLatin1());
    if (!r.ok()) return {};
    return QString::fromUtf8(r.plaintext);
}

} // namespace qumesh::app
