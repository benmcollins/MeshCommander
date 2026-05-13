// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QString>

#include "certs/cert_entry.h"

namespace qumesh::certs {

/// Stateless utilities to populate a `CertEntry` from on-disk bytes.
class CertParser
{
public:
    /// Pull a single PEM-encoded certificate (and optional private key)
    /// out of `pem`. Returns an empty CertEntry's `certDer` if no cert
    /// block was found. The private key, if present, is preserved
    /// verbatim — we never re-encode it.
    static CertEntry fromPem(const QByteArray &pem, QString *error);

    /// Build a CertEntry from a DER blob (typical `.cer` file).
    static CertEntry fromDer(const QByteArray &der, QString *error);

    /// Decrypt a PKCS#12 bundle and return the first cert + key. The
    /// implementation uses OpenSSL directly because QSslCertificate /
    /// QSslKey do not expose PKCS#12.
    static CertEntry fromPkcs12(const QByteArray &p12, const QString &password,
                                  QString *error);

    /// Reverse: serialise a CertEntry as a PEM bundle (cert + optional
    /// private key concatenated).
    static QByteArray toPem(const CertEntry &entry);

    /// Compute the canonical id (SHA-256 of DER, lowercase hex) for a
    /// given DER blob.
    static QString idForDer(const QByteArray &der);

    /// Fill the metadata fields (subject CN, issuer CN, validity,
    /// fingerprint, etc.) from the DER blob already in `entry->certDer`.
    /// Leaves `privateKeyPem` and `hasPrivateKey` untouched. Returns
    /// false if the DER isn't a recognisable X.509 certificate.
    static bool populateMetadata(CertEntry *entry, QString *error);
};

} // namespace qumesh::certs
