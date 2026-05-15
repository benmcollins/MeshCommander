// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QString>
#include <QtTypes>

namespace qumesh::redir {

/// Sub-protocols multiplexed over Intel's AMT Redirection port (16994/16995).
enum class Protocol : quint8 {
    Sol = 1,
    Kvm = 2,
    Ider = 3,
};

/// 8-byte StartRedirectionSession (`0x10`) for the chosen sub-protocol.
[[nodiscard]] QByteArray buildSelector(Protocol p);

/// Status codes returned in `0x11 StartRedirectionSessionReply`.
enum class StartSessionStatus : quint8 {
    Success = 0,
    Busy = 2,
    Unsupported = 3,
    UnknownError = 0xFF,
};

struct StartSessionReply
{
    StartSessionStatus status = StartSessionStatus::UnknownError;
    QByteArray oemData;
};

[[nodiscard]] bool tryParseStartSessionReply(QByteArrayView buffer,
                                              StartSessionReply *reply, int *consumed);

// -- Authentication (0x13 / 0x14) -----------------------------------------

/// Auth types the AMT firmware understands inside `0x13`/`0x14`.
enum class AuthType : quint8 {
    Query = 0,    ///< Client asks "what auth do you support?"
    Basic = 1,    ///< Plaintext (avoid)
    Kerberos = 2,
    DigestNoQop = 3, ///< RFC 2069-style (without cnonce/nc/qop). AMT calls it "Bad Digest".
    DigestWithQop = 4, ///< RFC 2617-style with cnonce/nc/qop. "Good Digest".
};

/// Bitmap returned in a `0x14` query response listing supported auth types.
struct AuthCapabilities
{
    bool hasBasic = false;
    bool hasKerberos = false;
    bool hasDigestNoQop = false;
    bool hasDigestWithQop = false;
};

/// Decoded `0x14 AuthenticateSessionReply` payload. Either `caps` is set
/// (after step 2 of the handshake) or `challenge` is set (after step 4).
struct AuthReply
{
    /// 0 = success, 1 = challenge/needs-credentials, other = error.
    quint8 status = 0xFF;
    AuthType authType = AuthType::Query;
    AuthCapabilities caps;
    QString realm;
    QString nonce;
    QString qop;
};

/// Build `0x13` with `authType=0`: the initial "what do you support?" query.
[[nodiscard]] QByteArray buildAuthQuery();

/// Build `0x13` `authType=4` carrying only `<user>` and `<authuri>` so the
/// server replies with a challenge. Other 6 fields are sent empty.
[[nodiscard]] QByteArray buildDigestQuery(const QString &user, const QString &authUri);

/// Build the final `0x13` `authType=4` with all 8 fields populated using
/// the digest the caller already computed via `computeDigest()`.
[[nodiscard]] QByteArray buildDigestResponse(const QString &user, const QString &realm,
                                              const QString &nonce, const QString &authUri,
                                              const QString &cnonce, const QString &nc,
                                              const QString &response, const QString &qop);

[[nodiscard]] bool tryParseAuthReply(QByteArrayView buffer, AuthReply *reply, int *consumed);

/// RFC 2617 digest with qop="auth":
///
///     HA1 = MD5(user:realm:pass)
///     HA2 = MD5("POST:" + authUri)
///     response = MD5(HA1:nonce:nc:cnonce:qop:HA2)
///
/// All MD5 outputs are lowercase hex. The legacy AMT firmware uses MD5
/// exclusively; we do not implement MD5-sess or other variants.
[[nodiscard]] QString computeDigest(const QString &user, const QString &realm,
                                    const QString &pass, const QString &nonce,
                                    const QString &nc, const QString &cnonce,
                                    const QString &qop, const QString &authUri);

/// Generate a 32-character lowercase-hex client nonce. Sourced from the
/// platform CSPRNG via `QRandomGenerator::securelySeeded()`.
[[nodiscard]] QString makeClientNonce();

// -- SOL (Serial-over-LAN) framing ---------------------------------------

/// Knobs the firmware accepts in a `0x20` Open. Defaults match the legacy
/// NW.js client's hardcoded values; reasonable for interactive terminals.
struct SolOpenParams
{
    quint16 maxTxBuffer = 10000;
    quint16 txTimeoutMs = 100;
    quint16 txOverflowTimeoutMs = 0;
    quint16 rxTimeoutMs = 10000;
    quint16 rxFlushTimeoutMs = 100;
    quint16 heartbeatMs = 0;
};

/// Build a `0x20 SerialSession.Open` (24 bytes).
[[nodiscard]] QByteArray buildSolOpen(quint32 sequence, const SolOpenParams &params = {});

/// Decoded `0x21 SerialSession.OpenReply` (23 bytes; status byte at offset 9).
struct SolOpenReply
{
    quint8 status = 0xFF;
};
[[nodiscard]] bool tryParseSolOpenReply(QByteArrayView buffer, SolOpenReply *reply,
                                          int *consumed);

/// Build `0x27 SerialSession.Settings` (14 bytes). The 6 control bytes are
/// the same fixed values the legacy client used.
[[nodiscard]] QByteArray buildSolSettings(quint32 sequence);

/// Build `0x28 SerialDataToHost` carrying user-typed bytes. Length is u16,
/// callers must chunk large inputs (max 65535 bytes per frame).
[[nodiscard]] QByteArray buildSolDataToHost(QByteArrayView data);

/// Streaming-friendly `0x2A IncomingDisplayData` parser. Returns true and
/// populates `*data` (raw terminal bytes) when a full frame is present.
[[nodiscard]] bool tryParseSolDisplayData(QByteArrayView buffer, QByteArray *data,
                                           int *consumed);

/// Build a `0x2B Keepalive` (8 bytes) the client emits every ~2 s.
[[nodiscard]] QByteArray buildSolKeepalive();

} // namespace qumesh::redir
