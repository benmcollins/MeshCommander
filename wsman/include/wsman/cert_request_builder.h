// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QString>

namespace qumesh::wsman {

/// Build a null-signed PKCS#10 CertificationRequest suitable for
/// `AMT_PublicKeyManagementService.GeneratePKCS10RequestEx`'s
/// `NullSignedCertificateRequest` parameter.
///
/// AMT's variant of the PKCS#10 generation flow is intentionally
/// split: the firmware generates the key pair (so the private key
/// never leaves the box) but it does not assemble the request — the
/// caller hands AMT a fully-formed template that just lacks a valid
/// signature, and AMT signs the template with the matching private
/// key it's holding. The template needs to embed:
///
///   * the desired Subject DN, in RFC 4514 form (e.g.
///     "CN=Intel(R) AMT, O=Intel"),
///   * the public-key bytes from the freshly generated
///     `AMT_PublicPrivateKeyPair.DERKey`, parseable as a
///     SubjectPublicKeyInfo by OpenSSL's `d2i_PUBKEY`,
///   * a placeholder signature value (the firmware overwrites this).
///
/// `signingAlgorithmOid` picks the OID written into the signature
/// AlgorithmIdentifier; AMT only honors the one matching the
/// `SigningAlgorithm` enum value sent on the same Invoke call (0 =
/// SHA1-RSA, 1 = SHA256-RSA — SHA1 is removed in CSME 18.0+). Default
/// is SHA256-RSA.
///
/// Returns the DER-encoded null-signed CertificationRequest on
/// success, or an empty `QByteArray` on failure (parse error on the
/// pubkey, OpenSSL out-of-memory, etc.). `*error` is set to a
/// human-readable message on failure when non-null.
[[nodiscard]] QByteArray buildNullSignedPkcs10Csr(
    const QByteArray &publicKeyDer,
    const QString &subjectDn,
    int signingAlgorithm = 1,
    QString *error = nullptr);

} // namespace qumesh::wsman
