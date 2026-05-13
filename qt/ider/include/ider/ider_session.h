// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QString>

#include "ider/ider_codec.h"

class QTimer;

namespace qumesh::redir {
class RedirectionClient;
}

namespace qumesh::ider {

/// Drives an IDE-R session on top of an authenticated
/// `RedirectionClient`. The driver serves a single read-only ISO file
/// as the CD-ROM device; floppy emulation is not exposed in v1 (BIOS
/// boots almost universally select the CD path).
///
/// State after construction:
///
///   start()  → sends 0x40 OpenSession.
///   on 0x41  → records SessionInfo, sends 0x48 EnableFeatures(type=3,
///              0x01 | startOption) to arm the device.
///   on 0x49  → routes by type (1=avail, 2=status, 3=toggle).
///   on 0x50  → parses SCSI CDB, replies with 0x54 SendDataToHost or
///              0x51 CommandEndResponse as appropriate.
///   on 0x46  → sends 0x47 ResetResponse.
///   on 0x44  → sends 0x45 KeepAlivePong.
///   on 0x4B  → no-op (server heartbeat).
class IderSession : public QObject
{
    Q_OBJECT
public:
    enum class StartOption : quint8 {
        OnReboot   = kStartOnReboot,
        Graceful   = kStartGraceful,
        Immediate  = kStartImmediate,
    };

    explicit IderSession(qumesh::redir::RedirectionClient *client,
                          QObject *parent = nullptr);
    ~IderSession() override;

    /// Configure before calling `start()`. The session takes ownership
    /// of the open QFile (or its absence).
    void setIsoPath(const QString &path) { m_isoPath = path; }
    void setStartOption(StartOption option) { m_startOption = option; }

    [[nodiscard]] QString isoPath() const { return m_isoPath; }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] quint64 bytesSentToAmt() const { return m_bytesSentToAmt; }
    [[nodiscard]] quint64 bytesReceivedFromAmt() const { return m_bytesReceivedFromAmt; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

    /// Send the OpenSession command. Only legal once the underlying
    /// client is at `Authenticated`.
    void start();

signals:
    /// Emitted when AMT acknowledges the OpenSession with valid
    /// SessionInfo. Consumers can treat this as "the channel is open
    /// but the BIOS hasn't requested anything yet".
    void sessionOpened(const qumesh::ider::SessionInfo &info);

    /// Emitted when AMT toggles features in response to our
    /// EnableFeatures. `enabled=true` means the redirected device is
    /// armed and AMT will begin SCSI traffic on reboot.
    void enabledChanged(bool enabled);

    /// Emitted as bytes-to/from-AMT counters move. Connect rate-limited.
    void statsChanged(quint64 toAmt, quint64 fromAmt);

    /// Emitted on protocol failure or an explicit AMT close.
    void closed(const QString &reason);

private:
    void onRawBytes(const QByteArray &chunk);
    void drainInbox();
    void onOpenSessionReply(const SessionInfo &info);
    void onStatusData(quint8 type, quint32 value);
    void handleScsi(const ScsiCommand &cmd);

    // SCSI handlers
    void scsiTestUnitReady(quint8 dev, quint8 feature);
    void scsiModeSense6(const QByteArray &cdb, quint8 dev, quint8 feature);
    void scsiModeSense10(const QByteArray &cdb, quint8 dev, quint8 feature);
    void scsiAllowMediumRemoval(quint8 dev, quint8 feature);
    void scsiStartStop(quint8 dev, quint8 feature);
    void scsiReadCapacity(quint8 dev, quint8 deviceFlags, quint8 feature);
    void scsiRead(const QByteArray &cdb, quint8 dev, quint8 feature, bool isRead10);
    void scsiReadToc(const QByteArray &cdb, quint8 dev, quint8 feature);
    void scsiGetConfiguration(const QByteArray &cdb, quint8 dev, quint8 feature);
    void scsiGetEventStatus(const QByteArray &cdb, quint8 dev, quint8 feature);
    void scsiReadFormatCapacities(const QByteArray &cdb, quint8 dev, quint8 feature);

    void sendData(quint8 dev, bool completed, QByteArrayView data, bool dma);
    void sendOk(quint8 dev);
    void sendError(quint8 dev, quint8 sense, quint8 asc, quint8 asq);
    void writeFrame(const QByteArray &frame);

    void fail(QString reason);

    qumesh::redir::RedirectionClient *m_client;
    QString m_isoPath;
    QFile m_isoFile;
    quint64 m_isoSize = 0;

    StartOption m_startOption = StartOption::Graceful;
    SessionInfo m_info;

    QByteArray m_inbox;
    quint32 m_outSequence = 0;
    quint32 m_inSequence  = 0;
    quint64 m_bytesSentToAmt = 0;
    quint64 m_bytesReceivedFromAmt = 0;

    bool m_opened = false;
    bool m_enabled = false;
    bool m_cdReady = false;
    quint64 m_remainingReadBytes = 0;
    QString m_lastError;
};

} // namespace qumesh::ider
