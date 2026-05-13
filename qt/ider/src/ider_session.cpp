// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ider/ider_session.h"

#include "ider/scsi.h"
#include "redir/redir_client.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>

namespace meshcommander::ider {

using namespace meshcommander::ider::scsi;
using meshcommander::redir::RedirectionClient;

namespace {

constexpr quint16 kRxTimeoutMs = 30000;
constexpr quint16 kTxTimeoutMs = 0;
constexpr quint16 kHeartbeatMs = 20000;
constexpr quint32 kProtocolVersion = 1;
constexpr qint64  kMaxReadCap = 8192;

QByteArray padOrTruncate(const QByteArray &in, int requested)
{
    if (requested <= 0) return {};
    if (in.size() == requested) return in;
    if (in.size() > requested) return in.left(requested);
    QByteArray out = in;
    out.append(requested - in.size(), '\0');
    return out;
}

} // namespace

IderSession::IderSession(RedirectionClient *client, QObject *parent)
    : QObject(parent), m_client(client)
{
    Q_ASSERT(client != nullptr);
    connect(m_client, &RedirectionClient::rawBytes, this, &IderSession::onRawBytes);
    connect(m_client, &RedirectionClient::failed, this, [this](const QString &reason) {
        fail(reason);
    });
}

IderSession::~IderSession() = default;

void IderSession::start()
{
    if (m_client->state() != RedirectionClient::State::Authenticated) {
        fail(QStringLiteral("client not authenticated"));
        return;
    }
    if (!m_isoPath.isEmpty()) {
        m_isoFile.setFileName(m_isoPath);
        if (!m_isoFile.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("cannot open ISO %1: %2").arg(m_isoPath, m_isoFile.errorString()));
            return;
        }
        m_isoSize = static_cast<quint64>(QFileInfo(m_isoPath).size());
    }

    writeFrame(buildOpenSession(m_outSequence++, kRxTimeoutMs, kTxTimeoutMs,
                                 kHeartbeatMs, kProtocolVersion));
}

void IderSession::onRawBytes(const QByteArray &chunk)
{
    m_bytesReceivedFromAmt += chunk.size();
    m_inbox.append(chunk);
    drainInbox();
    emit statsChanged(m_bytesSentToAmt, m_bytesReceivedFromAmt);
}

void IderSession::drainInbox()
{
    while (!m_inbox.isEmpty()) {
        const quint8 tag = peekTag(m_inbox);
        int consumed = 0;

        switch (tag) {
        case CmdOpenSessionReply: {
            SessionInfo info;
            if (!tryParseOpenSessionReply(m_inbox, &info, &consumed)) return;
            m_inbox.remove(0, consumed);
            onOpenSessionReply(info);
            break;
        }
        case CmdClose:
            if (!tryParseFixed(m_inbox, CmdClose, 8, &consumed)) return;
            m_inbox.remove(0, consumed);
            fail(QStringLiteral("AMT closed the IDE-R session"));
            return;
        case CmdKeepAlivePing:
            if (!tryParseFixed(m_inbox, CmdKeepAlivePing, 8, &consumed)) return;
            m_inbox.remove(0, consumed);
            writeFrame(buildKeepAlivePong(m_outSequence++));
            break;
        case CmdKeepAlivePong:
            if (!tryParseFixed(m_inbox, CmdKeepAlivePong, 8, &consumed)) return;
            m_inbox.remove(0, consumed);
            break;
        case CmdResetOccurred:
            if (!tryParseFixed(m_inbox, CmdResetOccurred, 9, &consumed)) return;
            m_inbox.remove(0, consumed);
            writeFrame(buildResetResponse(m_outSequence++));
            break;
        case CmdStatusData: {
            StatusData sd;
            if (!tryParseStatusData(m_inbox, &sd, &consumed)) return;
            m_inbox.remove(0, consumed);
            onStatusData(sd.type, sd.value);
            break;
        }
        case CmdErrorOccurred:
            if (!tryParseFixed(m_inbox, CmdErrorOccurred, 11, &consumed)) return;
            m_inbox.remove(0, consumed);
            // The legacy driver logs but does not abort — most "errors"
            // here are recoverable status reports from AMT.
            break;
        case CmdHeartbeat:
            if (!tryParseFixed(m_inbox, CmdHeartbeat, 8, &consumed)) return;
            m_inbox.remove(0, consumed);
            break;
        case CmdCommandWritten: {
            ScsiCommand cmd;
            if (!tryParseCommandWritten(m_inbox, &cmd, &consumed)) return;
            m_inbox.remove(0, consumed);
            handleScsi(cmd);
            break;
        }
        default:
            fail(QStringLiteral("unknown IDE-R command 0x%1")
                     .arg(tag, 2, 16, QLatin1Char('0')));
            return;
        }
    }
}

void IderSession::onOpenSessionReply(const SessionInfo &info)
{
    if (info.proto != 0) {
        fail(QStringLiteral("unsupported IDE-R proto %1").arg(info.proto));
        return;
    }
    if (info.readBuffer > kMaxReadCap || info.writeBuffer > kMaxReadCap) {
        fail(QStringLiteral("illegal IDE-R buffer sizes (read=%1 write=%2)")
                 .arg(info.readBuffer).arg(info.writeBuffer));
        return;
    }
    m_info = info;
    m_opened = true;
    emit sessionOpened(info);

    // Enable + start with the configured trigger. The 4 byte payload is
    // a LE-encoded mask: 0x01 (enable) | startOption.
    QByteArray payload(4, '\0');
    const quint32 v = 0x01u | static_cast<quint32>(m_startOption);
    payload[0] = static_cast<char>(v & 0xFF);
    payload[1] = static_cast<char>((v >> 8) & 0xFF);
    payload[2] = static_cast<char>((v >> 16) & 0xFF);
    payload[3] = static_cast<char>((v >> 24) & 0xFF);
    writeFrame(buildEnableFeatures(m_outSequence++, kStatusRegsToggle, payload));
}

void IderSession::onStatusData(quint8 type, quint32 value)
{
    switch (type) {
    case kStatusRegsAvail:
        if (value & 0x01) {
            // BIOS is now ready — re-arm with the start option.
            QByteArray payload(4, '\0');
            const quint32 v = 0x01u | static_cast<quint32>(m_startOption);
            payload[0] = static_cast<char>(v & 0xFF);
            payload[1] = static_cast<char>((v >> 8) & 0xFF);
            payload[2] = static_cast<char>((v >> 16) & 0xFF);
            payload[3] = static_cast<char>((v >> 24) & 0xFF);
            writeFrame(buildEnableFeatures(m_outSequence++, kStatusRegsToggle, payload));
        }
        break;
    case kStatusRegsStatus: {
        const bool en = (value & 0x02) != 0;
        if (en != m_enabled) {
            m_enabled = en;
            emit enabledChanged(en);
        }
        break;
    }
    case kStatusRegsToggle:
        // value 1 = success, anything else = toggle failure (ignored).
        break;
    default:
        break;
    }
}

void IderSession::handleScsi(const ScsiCommand &cmd)
{
    const QByteArray &cdb = cmd.cdb;
    if (cdb.isEmpty()) return;
    const quint8 opcode = static_cast<unsigned char>(cdb.at(0));
    const quint8 dev = (cmd.deviceFlags & 0x10) ? kDeviceCdRom : kDeviceFloppy;
    const quint8 fr = cmd.featureRegister;

    switch (opcode) {
    case kTestUnitReady: scsiTestUnitReady(dev, fr); break;
    case kModeSense6:    scsiModeSense6(cdb, dev, fr); break;
    case kModeSense10:   scsiModeSense10(cdb, dev, fr); break;
    case kAllowMediumRemoval: scsiAllowMediumRemoval(dev, fr); break;
    case kStartStop:     scsiStartStop(dev, fr); break;
    case kReadCapacity:  scsiReadCapacity(dev, cmd.deviceFlags, fr); break;
    case kRead6:         scsiRead(cdb, dev, fr, false); break;
    case kRead10:        scsiRead(cdb, dev, fr, true); break;
    case kReadToc:       scsiReadToc(cdb, dev, fr); break;
    case kGetConfiguration: scsiGetConfiguration(cdb, dev, fr); break;
    case kGetEventStatus:   scsiGetEventStatus(cdb, dev, fr); break;
    case kReadFormatCapacities: scsiReadFormatCapacities(cdb, dev, fr); break;
    case kWrite6:
    case kWrite10:
    case kWriteAndVerify:
        // Read-only: report no medium.
        sendError(dev, kSenseNotReady, 0x3A, 0x00);
        break;
    case kReadDiscInfo:
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        break;
    case kModeSelect10:
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        break;
    default:
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        break;
    }
}

// --- SCSI handlers -----------------------------------------------------

void IderSession::scsiTestUnitReady(quint8 dev, quint8 /*feature*/)
{
    if (dev != kDeviceCdRom || !m_isoFile.isOpen()) {
        sendError(dev, kSenseNotReady, 0x3A, 0x00);
        return;
    }
    if (!m_cdReady) {
        m_cdReady = true;
        sendError(dev, kSenseUnitAttention, 0x28, 0x00); // "media changed"
        return;
    }
    sendOk(dev);
}

void IderSession::scsiModeSense6(const QByteArray &cdb, quint8 dev, quint8 feature)
{
    if (cdb.size() < 4) {
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        return;
    }
    const quint8 page = static_cast<unsigned char>(cdb.at(2));
    const quint8 sub  = static_cast<unsigned char>(cdb.at(3));
    if (page == 0x3F && sub == 0x00) {
        quint8 a = 0, b = 0;
        if (dev == kDeviceCdRom) {
            if (!m_isoFile.isOpen()) { sendError(dev, kSenseNotReady, 0x3A, 0x00); return; }
            a = 0x05; b = 0x80;
        } else {
            sendError(dev, kSenseNotReady, 0x3A, 0x00);
            return;
        }
        QByteArray r(4, '\0');
        r[1] = static_cast<char>(a);
        r[2] = static_cast<char>(b);
        sendData(dev, true, r, (feature & 0x01) != 0);
        return;
    }
    sendError(dev, kSenseIllegalRequest, 0x24, 0x00);
}

void IderSession::scsiModeSense10(const QByteArray &cdb, quint8 dev, quint8 feature)
{
    if (cdb.size() < 8) {
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        return;
    }
    const quint16 buflen = readBe16(cdb, 7);
    if (buflen == 0) {
        QByteArray r = packBe32(0x003C0000) + packBe32(0x00000008);
        sendData(dev, true, r, (feature & 0x01) != 0);
        return;
    }
    const quint8 pageCode = static_cast<unsigned char>(cdb.at(2)) & 0x3F;
    QByteArray r;
    if (dev == kDeviceCdRom) {
        switch (pageCode) {
        case 0x01: r = modeSenseCdErrorRecovery(); break;
        case 0x1A: r = modeSenseCd1A(); break;
        case 0x1D: r = modeSenseCd1D(); break;
        case 0x2A: r = modeSenseCd2A(); break;
        case 0x3F: r = modeSenseCd3F(); break;
        default: break;
        }
    }
    if (r.isEmpty()) {
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        return;
    }
    if (r.size() > buflen) r = r.left(buflen);
    sendData(dev, true, r, (feature & 0x01) != 0);
}

void IderSession::scsiAllowMediumRemoval(quint8 dev, quint8 /*feature*/)
{
    if (dev == kDeviceCdRom && !m_isoFile.isOpen()) {
        sendError(dev, kSenseNotReady, 0x3A, 0x00);
        return;
    }
    sendOk(dev);
}

void IderSession::scsiStartStop(quint8 dev, quint8 /*feature*/)
{
    sendOk(dev);
}

void IderSession::scsiReadCapacity(quint8 dev, quint8 /*deviceFlags*/, quint8 feature)
{
    if (dev != kDeviceCdRom || !m_isoFile.isOpen() || m_isoSize < 2048) {
        sendError(dev, kSenseNotReady, 0x3A, 0x00);
        return;
    }
    // Number of 2048-byte sectors minus one (READ CAPACITY returns last LBA).
    const quint32 lastLba = static_cast<quint32>((m_isoSize >> 11) - 1);
    QByteArray body = packBe32(lastLba);
    body.append(char(0x00));
    body.append(char(0x00));
    body.append(char(0x08));
    body.append(char(0x00));
    sendData(dev, true, body, (feature & 0x01) != 0);
}

void IderSession::scsiRead(const QByteArray &cdb, quint8 dev, quint8 feature, bool isRead10)
{
    if (dev != kDeviceCdRom || !m_isoFile.isOpen()) {
        sendError(dev, kSenseNotReady, 0x3A, 0x00);
        return;
    }
    quint32 lba = 0;
    quint32 len = 0;
    if (isRead10) {
        if (cdb.size() < 9) { sendError(dev, kSenseIllegalRequest, 0x20, 0x00); return; }
        lba = readBe32(cdb, 2);
        len = readBe16(cdb, 7);
    } else {
        if (cdb.size() < 5) { sendError(dev, kSenseIllegalRequest, 0x20, 0x00); return; }
        lba = (static_cast<quint32>(static_cast<unsigned char>(cdb.at(1)) & 0x1F) << 16)
            | (static_cast<quint32>(static_cast<unsigned char>(cdb.at(2))) << 8)
            | static_cast<quint32>(static_cast<unsigned char>(cdb.at(3)));
        len = static_cast<unsigned char>(cdb.at(4));
        if (len == 0) len = 256;
    }
    const quint64 totalBlocks = m_isoSize >> 11;
    if (static_cast<quint64>(lba) + len > totalBlocks) {
        sendError(dev, kSenseIllegalRequest, 0x21, 0x00);
        return;
    }
    if (len == 0) {
        sendOk(dev);
        return;
    }

    quint64 byteOffset = static_cast<quint64>(lba) << 11;
    quint64 remaining  = static_cast<quint64>(len) << 11;

    const quint64 chunk = m_info.readBuffer > 0 ? m_info.readBuffer : kMaxReadCap;
    if (!m_isoFile.seek(static_cast<qint64>(byteOffset))) {
        sendError(dev, kSenseIllegalRequest, 0x21, 0x00);
        return;
    }
    // Send chunks one at a time, yielding back to the event loop between
    // each so the socket flushes and inbound heartbeats keep flowing for
    // multi-megabyte reads.
    auto sendNext = [this, dev, feature, chunk]() mutable -> bool {
        if (m_remainingReadBytes == 0) return false;
        const qint64 want = static_cast<qint64>(qMin(m_remainingReadBytes, chunk));
        QByteArray data = m_isoFile.read(want);
        if (data.size() != want) {
            sendError(dev, kSenseIllegalRequest, 0x21, 0x00);
            m_remainingReadBytes = 0;
            return false;
        }
        m_remainingReadBytes -= want;
        const bool completed = (m_remainingReadBytes == 0);
        sendData(dev, completed, data, (feature & 0x01) != 0);
        return m_remainingReadBytes > 0;
    };
    m_remainingReadBytes = remaining;
    while (sendNext()) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
        if (!m_isoFile.isOpen()) return; // teardown while yielding
    }
}

void IderSession::scsiReadToc(const QByteArray &cdb, quint8 dev, quint8 feature)
{
    if (dev != kDeviceCdRom) {
        sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
        return;
    }
    const bool msf = (cdb.size() > 1) && ((static_cast<unsigned char>(cdb.at(1)) & 0x02) != 0);
    quint8 format = (cdb.size() > 2) ? static_cast<unsigned char>(cdb.at(2)) & 0x07 : 0;
    if (format == 0 && cdb.size() > 9) format = static_cast<unsigned char>(cdb.at(9)) >> 6;

    if (format == 1) {
        static constexpr unsigned char kData[] = {
            0x00, 0x0A, 0x01, 0x01, 0x00, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        };
        sendData(dev, true,
                 QByteArrayView(reinterpret_cast<const char *>(kData), sizeof(kData)),
                 (feature & 0x01) != 0);
        return;
    }
    if (format == 0) {
        if (msf) {
            static constexpr unsigned char kData[] = {
                0x00, 0x12, 0x01, 0x01, 0x00, 0x14, 0x01, 0x00,
                0x00, 0x00, 0x02, 0x00, 0x00, 0x14, 0xAA, 0x00,
                0x00, 0x00, 0x34, 0x13,
            };
            sendData(dev, true,
                     QByteArrayView(reinterpret_cast<const char *>(kData), sizeof(kData)),
                     (feature & 0x01) != 0);
        } else {
            static constexpr unsigned char kData[] = {
                0x00, 0x12, 0x01, 0x01, 0x00, 0x14, 0x01, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0xAA, 0x00,
                0x00, 0x00, 0x00, 0x00,
            };
            sendData(dev, true,
                     QByteArrayView(reinterpret_cast<const char *>(kData), sizeof(kData)),
                     (feature & 0x01) != 0);
        }
        return;
    }
    sendError(dev, kSenseIllegalRequest, 0x20, 0x00);
}

void IderSession::scsiGetConfiguration(const QByteArray &cdb, quint8 dev, quint8 feature)
{
    if (cdb.size() < 9) { sendError(dev, kSenseIllegalRequest, 0x20, 0x00); return; }
    const bool sendAll = static_cast<unsigned char>(cdb.at(1)) != 2;
    const quint16 firstCode = readBe16(cdb, 2);
    const quint16 buflen = readBe16(cdb, 7);

    if (buflen == 0) {
        QByteArray r = packBe32(0x003C0000) + packBe32(0x00000008);
        sendData(dev, true, r, (feature & 0x01) != 0);
        return;
    }

    // Header: u32 length (filled in below) + u32 current profile (0x0008 = CD-ROM).
    QByteArray body = packBe32(0x00000008);

    if (firstCode == 0) body += configProfileList();
    if (firstCode == 0x1   || (sendAll && firstCode < 0x1))   body += configCore();
    if (firstCode == 0x2   || (sendAll && firstCode < 0x2))   body += configMorphing();
    if (firstCode == 0x3   || (sendAll && firstCode < 0x3))   body += configRemovable();
    if (firstCode == 0x10  || (sendAll && firstCode < 0x10))  body += configRandom();
    if (firstCode == 0x1E  || (sendAll && firstCode < 0x1E))  body += configRead();
    if (firstCode == 0x100 || (sendAll && firstCode < 0x100)) body += configPowerManagement();
    if (firstCode == 0x105 || (sendAll && firstCode < 0x105)) body += configTimeout();

    QByteArray r = packBe32(static_cast<quint32>(body.size())) + body;
    if (r.size() > buflen) r = r.left(buflen);
    sendData(dev, true, r, (feature & 0x01) != 0);
}

void IderSession::scsiGetEventStatus(const QByteArray &cdb, quint8 dev, quint8 feature)
{
    if (cdb.size() < 10
        || (static_cast<unsigned char>(cdb.at(1)) != 0x01
            && static_cast<unsigned char>(cdb.at(4)) != 0x10)) {
        sendError(dev, kSenseIllegalRequest, 0x26, 0x01);
        return;
    }
    quint8 present = 0x00;
    if (dev == kDeviceCdRom && m_isoFile.isOpen()) present = 0x02;
    QByteArray r(4, '\0');
    r[1] = static_cast<char>(present);
    r[2] = static_cast<char>(0x80);
    sendData(dev, true, r, (feature & 0x01) != 0);
}

void IderSession::scsiReadFormatCapacities(const QByteArray &cdb, quint8 dev, quint8 feature)
{
    if (dev != kDeviceCdRom || !m_isoFile.isOpen() || m_isoSize == 0) {
        sendError(dev, kSenseIllegalRequest, 0x24, 0x00);
        return;
    }
    Q_UNUSED(cdb);
    static constexpr unsigned char kPayload[] = {
        0x00, 0x00, 0x00, 0x08,             // capacity list header
        0x00, 0x00, 0x0B, 0x40,             // number of blocks (placeholder)
        0x02, 0x00, 0x02, 0x00,             // descriptor type + block length
    };
    QByteArray r(reinterpret_cast<const char *>(kPayload), sizeof(kPayload));
    sendData(dev, true, r, (feature & 0x01) != 0);
}

// --- low-level senders -------------------------------------------------

void IderSession::sendData(quint8 dev, bool completed, QByteArrayView data, bool dma)
{
    QByteArray frame = buildSendDataToHost(m_outSequence++, dev, completed, data, dma);
    writeFrame(frame);
}

void IderSession::sendOk(quint8 dev)
{
    QByteArray frame = buildCommandEndResponse(m_outSequence++, false,
                                                 kSenseNoSense, dev, 0x00, 0x00);
    writeFrame(frame);
}

void IderSession::sendError(quint8 dev, quint8 sense, quint8 asc, quint8 asq)
{
    QByteArray frame = buildCommandEndResponse(m_outSequence++, false,
                                                 sense, dev, asc, asq);
    writeFrame(frame);
}

void IderSession::writeFrame(const QByteArray &frame)
{
    if (m_client->writeRaw(frame) <= 0) {
        fail(QStringLiteral("write failed: %1").arg(m_client->lastError()));
        return;
    }
    m_bytesSentToAmt += frame.size();
    emit statsChanged(m_bytesSentToAmt, m_bytesReceivedFromAmt);
}

void IderSession::fail(QString reason)
{
    m_lastError = std::move(reason);
    if (m_isoFile.isOpen()) m_isoFile.close();
    if (m_opened || m_enabled) {
        m_opened = false;
        m_enabled = false;
        emit enabledChanged(false);
    }
    emit closed(m_lastError);
}

} // namespace meshcommander::ider
