// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include <QtTest>

#include <array>

using namespace qumesh::wsman;

namespace {

/// Build a 21-byte AMT event record from its decoded fields.
QByteArray makeRecord(quint32 timestamp, quint8 deviceAddress, quint8 sensorType,
                      quint8 eventType, quint8 offset, quint8 sourceType,
                      quint8 severity, quint8 sensorNumber, quint8 entity,
                      quint8 entityInstance, std::array<quint8, 8> data)
{
    QByteArray raw(21, '\0');
    auto *b = reinterpret_cast<quint8 *>(raw.data());
    b[0] = quint8(timestamp & 0xff);
    b[1] = quint8((timestamp >> 8) & 0xff);
    b[2] = quint8((timestamp >> 16) & 0xff);
    b[3] = quint8((timestamp >> 24) & 0xff);
    b[4] = deviceAddress;
    b[5] = sensorType;
    b[6] = eventType;
    b[7] = offset;
    b[8] = sourceType;
    b[9] = severity;
    b[10] = sensorNumber;
    b[11] = entity;
    b[12] = entityInstance;
    for (int i = 0; i < 8; ++i) b[13 + i] = data[i];
    return raw;
}

} // namespace

class TestEventLogDecoder : public QObject
{
    Q_OBJECT
private slots:
    void rejectsShortRecord();
    void rejectsZeroTimestamp();
    void rejectsSentinelTimestamp();
    void firmwareError();
    void firmwareProgress();
    void firmwareStarted();
    void caseIntrusion();
    void solSessionEstablished();
    void authenticationFailure();
    void noBootableMedia();
    void unknownSensor();
    void severityIsDecimalString();
    void timestampIsFormatted();
    void entityLabelIsResolved();
};

void TestEventLogDecoder::rejectsShortRecord()
{
    EventLogEntry e = decodeEventRecord(QByteArray(10, '\0'));
    QVERIFY(e.timestamp.isEmpty());
    QVERIFY(e.message.isEmpty());
}

void TestEventLogDecoder::rejectsZeroTimestamp()
{
    QByteArray raw = makeRecord(0, 0, 15, 0, 0, 0, 0, 0, 0, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QVERIFY(e.timestamp.isEmpty());
}

void TestEventLogDecoder::rejectsSentinelTimestamp()
{
    QByteArray raw = makeRecord(0xFFFFFFFFu, 0, 15, 0, 0, 0, 0, 0, 0, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QVERIFY(e.timestamp.isEmpty());
}

void TestEventLogDecoder::firmwareError()
{
    // sensorType=15, offset=0 → kSystemFirmwareError[data[1]]
    // data[1]=8 → "Removable boot media not found."
    QByteArray raw = makeRecord(1700000000u, 0x20, 15, 0, 0, 6, 5, 0, 34, 0,
                                {0, 8, 0, 0, 0, 0, 0, 0});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message, QStringLiteral("Removable boot media not found."));
    QCOMPARE(e.severity, QStringLiteral("5"));
}

void TestEventLogDecoder::firmwareProgress()
{
    // sensorType=15, offset≠0/3/5 → kSystemFirmwareProgress[data[1]]
    // data[1]=19 → "Starting operating system boot process"
    QByteArray raw = makeRecord(1700000000u, 0x20, 15, 0, 1, 6, 1, 0, 34, 0,
                                {0, 19, 0, 0, 0, 0, 0, 0});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message, QStringLiteral("Starting operating system boot process"));
}

void TestEventLogDecoder::firmwareStarted()
{
    QByteArray raw = makeRecord(1700000000u, 0x20, 37, 0, 0, 6, 1, 0, 34, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message,
             QStringLiteral("System firmware started (at least one CPU is properly executing)."));
}

void TestEventLogDecoder::caseIntrusion()
{
    // sensorType=5 offset=0
    QByteArray raw = makeRecord(1700000000u, 0x20, 5, 0, 0, 6, 4, 0, 23, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message, QStringLiteral("Case intrusion"));
}

void TestEventLogDecoder::solSessionEstablished()
{
    // sensorType=192 offset=0 data=[0xAA, 0x30, 0, …]
    QByteArray raw = makeRecord(1700000000u, 0x20, 192, 0, 0, 6, 2, 0, 34, 0,
                                {0xAA, 0x30, 0x00, 0, 0, 0, 0, 0});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message,
             QStringLiteral("A remote Serial Over LAN session was established."));
}

void TestEventLogDecoder::authenticationFailure()
{
    // sensorType=6 → "Authentication failed (d[1]|d[2]<<8) times…"
    QByteArray raw = makeRecord(1700000000u, 0x20, 6, 0, 0, 6, 4, 0, 34, 0,
                                {0, 7, 0, 0, 0, 0, 0, 0});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message,
             QStringLiteral("Authentication failed 7 times. The system may be under attack."));
}

void TestEventLogDecoder::noBootableMedia()
{
    QByteArray raw = makeRecord(1700000000u, 0x20, 30, 0, 0, 6, 5, 0, 34, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message, QStringLiteral("No bootable media"));
}

void TestEventLogDecoder::unknownSensor()
{
    QByteArray raw = makeRecord(1700000000u, 0x20, 250, 0, 0, 6, 4, 0, 34, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.message, QStringLiteral("Unknown Sensor Type #250"));
}

void TestEventLogDecoder::severityIsDecimalString()
{
    QByteArray raw = makeRecord(1700000000u, 0x20, 37, 0, 0, 6, 4, 0, 34, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.severity, QStringLiteral("4"));
}

void TestEventLogDecoder::timestampIsFormatted()
{
    // 1700000000 → 2023-11-14 22:13:20 UTC
    QByteArray raw = makeRecord(1700000000u, 0x20, 37, 0, 0, 6, 1, 0, 34, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.timestamp, QStringLiteral("2023-11-14 22:13:20"));
}

void TestEventLogDecoder::entityLabelIsResolved()
{
    // entity=34 → "BIOS"
    QByteArray raw = makeRecord(1700000000u, 0x20, 37, 0, 0, 6, 1, 0, 34, 0, {});
    EventLogEntry e = decodeEventRecord(raw);
    QCOMPARE(e.entityLabel, QStringLiteral("BIOS"));
}

QTEST_MAIN(TestEventLogDecoder)
#include "test_event_log_decoder.moc"
