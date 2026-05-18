// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "rmcp/pong.h"

namespace qumesh::rmcp {

namespace {

// ASF/RMCP "presence ping" header. Per IPMI 2.0 section 13.2.3 and the
// ASF 2.0 specification table 3-9, the layout is:
//
//   byte 0  : RMCP version              = 0x06 (ASF 2.0)
//   byte 1  : reserved                  = 0x00
//   byte 2  : sequence number           = 0xFF (no ACK requested)
//   byte 3  : message class             = 0x06 (ASF)
//   byte 4-7: IANA enterprise number    = 0x000011BE (Intel) for the
//             ping; 0x000011BE on AMT's reply too (rather than ASF's
//             0x000011D8) because the responder is Intel firmware
//   byte 8  : message type              = 0x80 (presence ping)
//   byte 9  : message tag (caller-chosen)
//   byte 10 : reserved                  = 0x00
//   byte 11 : data length               = 0x00
//
// MeshCommander's legacy scanner used byte 2 = 0x00 instead of 0xFF and
// AMT replied anyway, so we follow the same shape it shipped with.
constexpr quint8 kRmcpVersion      = 0x06;
constexpr quint8 kRmcpMsgClassAsf  = 0x06;
constexpr quint8 kAsfMsgTypePing   = 0x80;
constexpr quint8 kAsfMsgTypePong   = 0x40;

// Intel's IANA enterprise number in the reply, big-endian.
constexpr quint8 kIntelEnt[4] = { 0x00, 0x00, 0x11, 0xBE };

} // namespace

QByteArray buildPing(quint8 tag)
{
    QByteArray out(12, '\0');
    out[0]  = kRmcpVersion;
    out[1]  = 0x00;
    out[2]  = 0x00;
    out[3]  = kRmcpMsgClassAsf;
    out[4]  = static_cast<char>(kIntelEnt[0]);
    out[5]  = static_cast<char>(kIntelEnt[1]);
    out[6]  = static_cast<char>(kIntelEnt[2]);
    out[7]  = static_cast<char>(kIntelEnt[3]);
    out[8]  = static_cast<char>(kAsfMsgTypePing);
    out[9]  = static_cast<char>(tag);
    out[10] = 0x00;
    out[11] = 0x00;
    return out;
}

std::optional<PongMessage> parsePong(const QByteArray &datagram)
{
    // Pong is 16-byte ASF header + at least a 4-byte body the legacy
    // scanner reads (bytes 16–21 hold open-port, version, prov-state,
    // capability flags). Anything shorter than 22 can't be a valid
    // AMT presence pong.
    if (datagram.size() < 22)
        return std::nullopt;

    const auto *b = reinterpret_cast<const quint8 *>(datagram.constData());

    // Message class must be ASF (0x06) and message type must be 0x40
    // (presence pong). The IANA bytes 4–7 carry Intel's enterprise
    // number on a real AMT reply; reject anything else so we don't
    // pick up generic ASF responders.
    if (b[3] != kRmcpMsgClassAsf)
        return std::nullopt;
    if (b[4] != kIntelEnt[0] || b[5] != kIntelEnt[1]
        || b[6] != kIntelEnt[2] || b[7] != kIntelEnt[3]) {
        return std::nullopt;
    }
    if (b[8] != kAsfMsgTypePong)
        return std::nullopt;

    PongMessage out{};
    out.tag = b[9];
    // bytes 16–17: open-port, big-endian; AMT advertises 16992 here
    // (or 0 when only TLS is listening — the dual-ports bit is the
    // load-bearing TLS signal regardless).
    out.openPort = static_cast<quint16>((b[16] << 8) | b[17]);
    // byte 18: high nibble = major, low nibble = minor.
    out.versionMajor = static_cast<quint8>((b[18] >> 4) & 0x0F);
    out.versionMinor = static_cast<quint8>(b[18] & 0x0F);
    // byte 19: low two bits = provisioning state; bit 2 = both 16992
    // AND 16993 are listening (the legacy "open ports" flag).
    const quint8 prov = static_cast<quint8>(b[19] & 0x03);
    switch (prov) {
        case 0: out.provisioning = ProvisioningState::PreProvisioning;  break;
        case 1: out.provisioning = ProvisioningState::InProcess;        break;
        case 2: out.provisioning = ProvisioningState::PostProvisioning; break;
        default: return std::nullopt; // 3 is reserved — treat as malformed.
    }
    out.dualPorts = (b[19] & 0x04) != 0;
    return out;
}

} // namespace qumesh::rmcp
