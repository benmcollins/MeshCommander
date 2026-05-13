// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>

namespace qumesh::ider::scsi {

/// SCSI opcodes used by AMT's BIOS-side IDE-R driver. Only a small
/// subset is needed for read-only ISO booting.
constexpr quint8 kTestUnitReady          = 0x00;
constexpr quint8 kRead6                  = 0x08;
constexpr quint8 kWrite6                 = 0x0A;
constexpr quint8 kModeSense6             = 0x1A;
constexpr quint8 kStartStop              = 0x1B;
constexpr quint8 kAllowMediumRemoval     = 0x1E;
constexpr quint8 kReadFormatCapacities   = 0x23;
constexpr quint8 kReadCapacity           = 0x25;
constexpr quint8 kRead10                 = 0x28;
constexpr quint8 kWrite10                = 0x2A;
constexpr quint8 kWriteAndVerify         = 0x2E;
constexpr quint8 kReadToc                = 0x43;
constexpr quint8 kGetConfiguration       = 0x46;
constexpr quint8 kGetEventStatus         = 0x4A;
constexpr quint8 kModeSelect10           = 0x55;
constexpr quint8 kModeSense10            = 0x5A;
constexpr quint8 kReadDiscInfo           = 0x51;

/// SCSI sense keys (subset).
constexpr quint8 kSenseNoSense           = 0x00;
constexpr quint8 kSenseNotReady          = 0x02;
constexpr quint8 kSenseIllegalRequest    = 0x05;
constexpr quint8 kSenseUnitAttention     = 0x06;

/// Pre-canned mode-sense response tables copied byte-for-byte from the
/// legacy `amt-ider-ws` module. AMT BIOS firmware is very particular
/// about the exact bytes here — see the reference comments in the
/// original for what each table represents.
const QByteArray &modeSenseCd1A();
const QByteArray &modeSenseCd1D();
const QByteArray &modeSenseCd2A();
const QByteArray &modeSenseCd3F();
const QByteArray &modeSenseCdErrorRecovery();

/// 0x46 GET CONFIGURATION feature blocks.
const QByteArray &configProfileList();
const QByteArray &configCore();
const QByteArray &configMorphing();
const QByteArray &configRemovable();
const QByteArray &configRandom();
const QByteArray &configRead();
const QByteArray &configPowerManagement();
const QByteArray &configTimeout();

/// Read a SCSI big-endian u16 from a buffer.
inline quint16 readBe16(const QByteArray &b, int off)
{
    return (static_cast<quint16>(static_cast<unsigned char>(b.at(off))) << 8)
         | static_cast<quint16>(static_cast<unsigned char>(b.at(off + 1)));
}

/// Read a SCSI big-endian u32 from a buffer.
inline quint32 readBe32(const QByteArray &b, int off)
{
    return (static_cast<quint32>(static_cast<unsigned char>(b.at(off))) << 24)
         | (static_cast<quint32>(static_cast<unsigned char>(b.at(off + 1))) << 16)
         | (static_cast<quint32>(static_cast<unsigned char>(b.at(off + 2))) << 8)
         | static_cast<quint32>(static_cast<unsigned char>(b.at(off + 3)));
}

/// Pack a big-endian u32 into a 4-byte QByteArray.
inline QByteArray packBe32(quint32 v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>((v >> 24) & 0xFF);
    b[1] = static_cast<char>((v >> 16) & 0xFF);
    b[2] = static_cast<char>((v >> 8) & 0xFF);
    b[3] = static_cast<char>(v & 0xFF);
    return b;
}

} // namespace qumesh::ider::scsi
