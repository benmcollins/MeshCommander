// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace meshcommander::certs {

/// One row in the local certificate store. Mirrors the legacy
/// MeshCommander entry layout closely enough that migrated data lands
/// unchanged: at minimum, `certbin` (base64 DER) plus optional
/// `privateKeyBin` (PEM). Everything else is derived metadata cached
/// in the JSON for quick listing.
struct CertEntry {
    QString id;                  ///< Stable handle (SHA-256 of DER, hex).
    QString subjectCommonName;
    QString issuerCommonName;
    QString serial;
    QDateTime notBefore;
    QDateTime notAfter;
    QString fingerprintSha256;   ///< Hex, colon-separated.
    bool hasPrivateKey = false;

    QByteArray certDer;          ///< Raw bytes; ready to write as .cer.
    QString privateKeyPem;       ///< PEM-encoded private key if present.

    [[nodiscard]] QJsonObject toJson() const;
    static CertEntry fromJson(const QJsonObject &obj);
};

} // namespace meshcommander::certs
