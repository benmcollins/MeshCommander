// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QString>
#include <QtTypes>

namespace meshcommander::redir {

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

} // namespace meshcommander::redir
