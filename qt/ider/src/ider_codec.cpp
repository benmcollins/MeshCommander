// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ider/ider_codec.h"

namespace meshcommander::ider {

namespace {

QByteArray pack16Le(quint16 v)
{
    QByteArray b(2, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    return b;
}

QByteArray pack32Le(quint32 v)
{
    QByteArray b(4, '\0');
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    return b;
}

quint16 read16Le(QByteArrayView v, int off)
{
    return static_cast<quint16>(static_cast<unsigned char>(v[off]))
         | static_cast<quint16>(static_cast<unsigned char>(v[off + 1])) << 8;
}

quint32 read32Le(QByteArrayView v, int off)
{
    return static_cast<quint32>(static_cast<unsigned char>(v[off]))
         | static_cast<quint32>(static_cast<unsigned char>(v[off + 1])) << 8
         | static_cast<quint32>(static_cast<unsigned char>(v[off + 2])) << 16
         | static_cast<quint32>(static_cast<unsigned char>(v[off + 3])) << 24;
}

} // namespace

QByteArray packHeader(Command id, quint32 sequence, quint8 attrs)
{
    QByteArray b;
    b.reserve(8);
    b.append(static_cast<char>(id));
    b.append(static_cast<char>(0x00));
    b.append(static_cast<char>(0x00));
    b.append(static_cast<char>(attrs));
    b.append(pack32Le(sequence));
    return b;
}

quint32 readSequence(QByteArrayView frame)
{
    if (frame.size() < 8) return 0;
    return read32Le(frame, 4);
}

QByteArray buildOpenSession(quint32 sequence, quint16 rxTimeoutMs,
                             quint16 txTimeoutMs, quint16 hbTimeoutMs,
                             quint32 version)
{
    QByteArray f = packHeader(CmdOpenSession, sequence);
    f.append(pack16Le(rxTimeoutMs));
    f.append(pack16Le(txTimeoutMs));
    f.append(pack16Le(hbTimeoutMs));
    f.append(pack32Le(version));
    return f;
}

QByteArray buildKeepAlivePong(quint32 sequence)
{
    return packHeader(CmdKeepAlivePong, sequence);
}

QByteArray buildResetResponse(quint32 sequence)
{
    return packHeader(CmdResetResponse, sequence);
}

QByteArray buildEnableFeatures(quint32 sequence, quint8 type, const QByteArray &data)
{
    QByteArray f = packHeader(CmdEnableFeatures, sequence);
    f.append(static_cast<char>(type));
    f.append(data);
    return f;
}

QByteArray buildHeartbeat(quint32 sequence)
{
    return packHeader(CmdHeartbeat, sequence);
}

QByteArray buildCommandEndResponse(quint32 sequence,
                                    bool error,
                                    quint8 sense,
                                    quint8 device,
                                    quint8 asc,
                                    quint8 asq)
{
    // The 0x51 frame's "completed" bit is always set in the wire format
    // (attrs=0x02). Body is a fixed 23-byte status descriptor.
    QByteArray f = packHeader(CmdCommandEndResponse, sequence, 0x02);
    static constexpr int kBodySize = 23;
    QByteArray body(kBodySize, '\0');
    if (error) {
        body[12] = static_cast<char>(0xC5);
        body[14] = 0x03;
        body[18] = static_cast<char>(device);
        body[19] = 0x50;
    } else {
        body[12] = static_cast<char>(0x87);
        body[13] = static_cast<char>((sense << 4) & 0xFF);
        body[14] = 0x03;
        body[18] = static_cast<char>(device);
        body[19] = 0x51;
        body[20] = static_cast<char>(sense);
        body[21] = static_cast<char>(asc);
        body[22] = static_cast<char>(asq);
    }
    f.append(body);
    return f;
}

QByteArray buildSendDataToHost(quint32 sequence,
                                quint8 device,
                                bool completed,
                                QByteArrayView data,
                                bool dma)
{
    quint8 attrs = completed ? 0x02 : 0x00;
    if (dma) attrs |= 0x01;
    QByteArray f = packHeader(CmdSendDataToHost, sequence, attrs);

    // 26-byte SCSI response header. Refer to amt-ider-ws SendDataToHost:
    //   [0]    = 0
    //   [1..2] = data length (LE) — for the SCSI byte count
    //   [3]    = 0
    //   [4]    = 0xb4 (DMA) or 0xb5 (PIO)
    //   [5]    = 0
    //   [6]    = 2
    //   [7]    = 0
    //   [8..9] = DMA length (LE): 0 for DMA, data length otherwise
    //   [10]   = device (0xA0 or 0xB0)
    //   [11]   = 0x58
    //   [12..] = SCSI status word (only when completed=true)
    QByteArray body(26, '\0');
    const quint16 n = static_cast<quint16>(data.size());
    body[1] = static_cast<char>(n & 0xFF);
    body[2] = static_cast<char>((n >> 8) & 0xFF);
    body[4] = static_cast<char>(dma ? 0xB4 : 0xB5);
    body[6] = 0x02;
    const quint16 dmalen = dma ? 0 : n;
    body[8] = static_cast<char>(dmalen & 0xFF);
    body[9] = static_cast<char>((dmalen >> 8) & 0xFF);
    body[10] = static_cast<char>(device);
    body[11] = 0x58;
    if (completed) {
        body[12] = static_cast<char>(0x85);
        body[14] = 0x03;
        body[18] = static_cast<char>(device);
        body[19] = 0x50;
    }
    f.append(body);
    f.append(data.data(), static_cast<int>(data.size()));
    return f;
}

quint8 peekTag(QByteArrayView buffer)
{
    if (buffer.isEmpty()) return 0;
    return static_cast<quint8>(static_cast<unsigned char>(buffer[0]));
}

bool tryParseOpenSessionReply(QByteArrayView buffer,
                               SessionInfo *info,
                               int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < 30) return false;
    if (peekTag(buffer) != CmdOpenSessionReply) return false;
    const int oemLen = static_cast<unsigned char>(buffer[29]);
    if (buffer.size() < 30 + oemLen) return false;
    if (info != nullptr) {
        info->major       = static_cast<unsigned char>(buffer[8]);
        info->minor       = static_cast<unsigned char>(buffer[9]);
        info->fwMajor     = static_cast<unsigned char>(buffer[10]);
        info->fwMinor     = static_cast<unsigned char>(buffer[11]);
        info->readBuffer  = read16Le(buffer, 16);
        info->writeBuffer = read16Le(buffer, 18);
        info->proto       = static_cast<unsigned char>(buffer[21]);
        info->iana        = read32Le(buffer, 25);
        info->oemData     = QByteArray(buffer.data() + 30, oemLen);
    }
    if (consumed != nullptr) *consumed = 30 + oemLen;
    return true;
}

bool tryParseCommandWritten(QByteArrayView buffer,
                             ScsiCommand *cmd,
                             int *consumed)
{
    constexpr int kSize = 28;
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < kSize) return false;
    if (peekTag(buffer) != CmdCommandWritten) return false;
    if (cmd != nullptr) {
        cmd->featureRegister = static_cast<unsigned char>(buffer[9]);
        cmd->deviceFlags     = static_cast<unsigned char>(buffer[14]);
        cmd->cdb             = QByteArray(buffer.data() + 16, 12);
    }
    if (consumed != nullptr) *consumed = kSize;
    return true;
}

bool tryParseFixed(QByteArrayView buffer,
                    quint8 expectedTag,
                    int totalSize,
                    int *consumed)
{
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < totalSize) return false;
    if (peekTag(buffer) != expectedTag) return false;
    if (consumed != nullptr) *consumed = totalSize;
    return true;
}

bool tryParseStatusData(QByteArrayView buffer, StatusData *out, int *consumed)
{
    constexpr int kSize = 13;
    if (consumed != nullptr) *consumed = 0;
    if (buffer.size() < kSize) return false;
    if (peekTag(buffer) != CmdStatusData) return false;
    if (out != nullptr) {
        out->type  = static_cast<unsigned char>(buffer[8]);
        out->value = read32Le(buffer, 9);
    }
    if (consumed != nullptr) *consumed = kSize;
    return true;
}

} // namespace meshcommander::ider
