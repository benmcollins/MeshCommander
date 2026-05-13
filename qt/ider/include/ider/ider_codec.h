// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QByteArrayView>

namespace meshcommander::ider {

/// IDE-R wire commands. Every frame starts with an 8-byte header:
///     [cmdId:1] [0x00:1] [0x00:1] [attrs:1] [sequence:4 LE]
/// followed by command-specific data.
enum Command : quint8 {
    CmdOpenSession         = 0x40,
    CmdOpenSessionReply    = 0x41,
    CmdClose               = 0x43,
    CmdKeepAlivePing       = 0x44,
    CmdKeepAlivePong       = 0x45,
    CmdResetOccurred       = 0x46,
    CmdResetResponse       = 0x47,
    CmdEnableFeatures      = 0x48,
    CmdStatusData          = 0x49,
    CmdErrorOccurred       = 0x4A,
    CmdHeartbeat           = 0x4B,
    CmdCommandWritten      = 0x50,
    CmdCommandEndResponse  = 0x51,
    CmdGetDataFromHost     = 0x52,
    CmdDataFromHost        = 0x53,
    CmdSendDataToHost      = 0x54,
};

/// AMT device identifiers reported in the SCSI command frame.
constexpr quint8 kDeviceFloppy = 0xA0;
constexpr quint8 kDeviceCdRom  = 0xB0;

/// Subset of StatusData reply types used by AMT.
constexpr quint8 kStatusRegsAvail  = 1;
constexpr quint8 kStatusRegsStatus = 2;
constexpr quint8 kStatusRegsToggle = 3;

/// Enable-features start options: bits or'd with 0x01 (enable).
constexpr quint8 kStartOnReboot   = 0x08;
constexpr quint8 kStartGraceful   = 0x10;
constexpr quint8 kStartImmediate  = 0x18;

/// Parsed 0x41 OpenSessionReply.
struct SessionInfo {
    quint8 major     = 0;
    quint8 minor     = 0;
    quint8 fwMajor   = 0;
    quint8 fwMinor   = 0;
    quint16 readBuffer  = 0;
    quint16 writeBuffer = 0;
    quint8 proto     = 0;
    quint32 iana     = 0;
    QByteArray oemData;
};

/// Parsed 0x50 CommandWritten frame: an ATAPI / SCSI command from AMT.
struct ScsiCommand {
    quint8 deviceFlags = 0;
    quint8 featureRegister = 0;
    QByteArray cdb;        // 12 bytes
};

/// Common helpers
QByteArray packHeader(Command id, quint32 sequence, quint8 attrs = 0);
quint32 readSequence(QByteArrayView frame);

/// Outbound builders
QByteArray buildOpenSession(quint32 sequence, quint16 rxTimeoutMs,
                             quint16 txTimeoutMs, quint16 hbTimeoutMs,
                             quint32 version);
QByteArray buildKeepAlivePong(quint32 sequence);
QByteArray buildResetResponse(quint32 sequence);
QByteArray buildEnableFeatures(quint32 sequence, quint8 type, const QByteArray &data);
QByteArray buildHeartbeat(quint32 sequence);

/// Build a 0x51 CommandEndResponse. `error=true` reports failure with the
/// supplied sense; otherwise reports success with the supplied sense.
QByteArray buildCommandEndResponse(quint32 sequence,
                                    bool error,
                                    quint8 sense,
                                    quint8 device,
                                    quint8 asc,
                                    quint8 asq);

/// Build a 0x54 SendDataToHost. `completed=true` is the last chunk of a
/// SCSI response (sets the SCSI status code); intermediate chunks pass
/// `completed=false`.
QByteArray buildSendDataToHost(quint32 sequence,
                                quint8 device,
                                bool completed,
                                QByteArrayView data,
                                bool dma);

/// Inbound parsers — each returns true and writes `*consumed` to the
/// total bytes (header + payload) covered by the frame. They return false
/// and write 0 to `*consumed` if more bytes are needed.
bool tryParseOpenSessionReply(QByteArrayView buffer,
                               SessionInfo *info,
                               int *consumed);
bool tryParseCommandWritten(QByteArrayView buffer,
                             ScsiCommand *cmd,
                             int *consumed);

/// `tryParseFixed` covers the simple fixed-length frames whose only
/// useful field is the tag/sequence: 0x43 Close (8), 0x44 KeepAlivePing
/// (8), 0x45 KeepAlivePong (8), 0x46 ResetOccurred (9), 0x49 StatusData
/// (13), 0x4A ErrorOccurred (11), 0x4B Heartbeat (8).
bool tryParseFixed(QByteArrayView buffer,
                    quint8 expectedTag,
                    int totalSize,
                    int *consumed);

/// `tryParseStatusData` reads the (type, value) pair from a 0x49 frame
/// after the caller has confirmed the tag matches.
struct StatusData {
    quint8 type = 0;
    quint32 value = 0;
};
bool tryParseStatusData(QByteArrayView buffer, StatusData *out, int *consumed);

/// Peek at the next inbound frame's tag without consuming. Returns 0 if
/// the buffer is empty.
quint8 peekTag(QByteArrayView buffer);

} // namespace meshcommander::ider
