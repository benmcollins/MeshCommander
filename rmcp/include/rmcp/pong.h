// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QtGlobal>
#include <optional>

namespace qumesh::rmcp {

/// Intel AMT firmware answers an IPMI/RMCP "Presence Ping" on UDP/623
/// with an "ASF Presence Pong" that identifies itself with the Intel
/// enterprise number (0x11BE) and reports its provisioning state and a
/// pair of listening ports (16992 / 16993). The byte layout used here is
/// the same one MeshCommander's `amt-scanner-0.1.0.js` decoded.
enum class ProvisioningState : quint8 {
    PreProvisioning = 0,
    InProcess = 1,
    PostProvisioning = 2,
};

struct PongMessage
{
    /// Caller tag echoed from the ping (byte 9). Used to drop replies
    /// that aren't ours when multiple scans share the same socket.
    quint8 tag = 0;
    quint8 versionMajor = 0; ///< AMT major version, byte 18 high nibble.
    quint8 versionMinor = 0; ///< AMT minor version, byte 18 low nibble.
    ProvisioningState provisioning = ProvisioningState::PreProvisioning;
    /// True when bit 2 of byte 19 is set: both 16992 (plain) and 16993
    /// (TLS) are listening. We treat that as "TLS-capable" for the
    /// add-to-fleet default.
    bool dualPorts = false;
    /// Port the responder advertises as currently open, bytes 16–17 big-endian.
    quint16 openPort = 0;
};

/// Build the 12-byte ASF presence-ping. `tag` lets the caller correlate
/// replies; the field is echoed back in `PongMessage::tag`.
[[nodiscard]] QByteArray buildPing(quint8 tag);

/// Decode a pong datagram. Returns nullopt unless every validating field
/// matches Intel's presence-pong layout (length ≥ 20 bytes, IANA bytes
/// 12–15 = `00 00 11 BE`, "presence ping reply" message-type at byte 4
/// = 0x40, and the "AMT supported" bit set in byte 21).
[[nodiscard]] std::optional<PongMessage> parsePong(const QByteArray &datagram);

} // namespace qumesh::rmcp
