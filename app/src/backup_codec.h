// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonArray>
#include <QString>

namespace qumesh::app {

/// File-level encryption for `.qumesh-backup` exports of the machine
/// list. Pure crypto and parsing — no I/O, no model knowledge — so it
/// can be unit-tested without standing up a `ConfigStore`.
///
/// File format (UTF-8, JSON):
///
/// ```
/// {
///   "type": "qumesh-backup",
///   "version": 1,
///   "createdAt": "<ISO 8601>",
///   "envelope": "v1",
///   "kdf":    { "name": "pbkdf2-sha256", "iterations": 600000, "salt": "<32 B base64>" },
///   "cipher": { "name": "aes-256-gcm", "iv": "<12 B base64>", "tag": "<16 B base64>" },
///   "ciphertext": "<base64 of AES-256-GCM(plaintext)>"
/// }
/// ```
///
/// Plaintext after decrypt:
///
/// ```
/// { "schema": "qumesh.computers/1", "computers": [ /* Computer::toPortableJson */ ] }
/// ```
///
/// Two design choices worth flagging:
///
/// - **AES-256-GCM** rather than CTR. The auth tag lets the caller
///   distinguish wrong-password (tag check fails on decrypt) from
///   tampering (same) from a malformed file (JSON or base64 fails)
///   so the UI can render the right error.
/// - **Secrets travel in plaintext inside the envelope.** The v3
///   `SecretCodec` envelope uses a per-install KeyStore key — that
///   key doesn't survive a transfer. The outer GCM envelope is what
///   protects secrets on disk; once unsealed they're handed straight
///   to `Computer::fromPortableJson` and re-wrapped by the local
///   `SecretCodec` at import time.
class BackupCodec
{
public:
    /// `error` empty on success; on failure it carries a developer-
    /// readable message that does NOT include the password.
    struct EncodeResult {
        QByteArray bytes;
        QString error;
        [[nodiscard]] bool ok() const { return error.isEmpty(); }
    };

    enum class FailReason {
        Ok,
        BadFormat,    ///< Not a recognized envelope (wrong type, version, missing fields).
        BadPassword,  ///< GCM tag check failed — usually the password is wrong.
        Corrupt,      ///< Tag check failed AFTER a sentinel said the format was otherwise valid.
        Internal,     ///< OpenSSL failure that isn't an input problem.
    };

    struct DecodeResult {
        QJsonArray computers; ///< Each element is the portable per-machine JSON.
        QString error;
        FailReason reason = FailReason::Ok;
        [[nodiscard]] bool ok() const { return reason == FailReason::Ok; }
    };

    /// `computers` should already be in portable form
    /// (`Computer::toPortableJson`). `password` must be non-empty
    /// (8+ chars is the recommended UI gate, not enforced here so
    /// tests can use shorter values).
    [[nodiscard]] static EncodeResult encode(const QJsonArray &computers,
                                              const QString &password);

    /// Inverse of `encode`. Returns `BadFormat` if the file isn't a
    /// QuMesh backup, `BadPassword` if GCM authentication fails,
    /// `Corrupt` if a known-good envelope is internally inconsistent.
    [[nodiscard]] static DecodeResult decode(QByteArrayView fileBytes,
                                              const QString &password);

    /// Hard-coded for v1 envelopes. Exposed so tests can sanity-check
    /// the cost in the JSON header without parsing the file twice.
    static constexpr int kPbkdf2Iterations = 600000;

    /// Hard ceiling on the iteration count we'll accept from an
    /// imported envelope. Caps a DoS where a tampered file sets
    /// `kdf.iterations` to INT_MAX and freezes the UI thread for
    /// hours inside PBKDF2.
    static constexpr int kPbkdf2MaxIterations = 10'000'000;

    /// Hard ceiling on the file size `decode` will consider.
    /// `BackupController` also enforces this before reading the
    /// file, but the codec re-checks so it's safe to call from any
    /// future code path. A 1000-machine backup is well under 1 MiB.
    static constexpr int kMaxFileBytes = 64 * 1024 * 1024;

    /// Hard ceiling on password length we hand to PBKDF2. Defense
    /// against an accidental multi-MB paste in the password field.
    static constexpr int kMaxPasswordChars = 1024;
};

} // namespace qumesh::app
