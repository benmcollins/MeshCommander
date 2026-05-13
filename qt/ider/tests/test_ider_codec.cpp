// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ider/ider_codec.h"
#include "ider/scsi.h"

#include <QtTest>

using namespace qumesh::ider;

class TestIderCodec : public QObject
{
    Q_OBJECT
private slots:
    void openSessionLayout();
    void openSessionReplyRoundTrip();
    void enableFeaturesLayout();
    void commandWrittenParse();
    void sendDataToHostLayout();
    void commandEndResponseLayout();
    void fixedFramesAndStatusData();
    void scsiBeHelpers();
};

void TestIderCodec::openSessionLayout()
{
    // 8-byte header + 2+2+2 timeouts + 4 byte version = 18.
    QByteArray f = buildOpenSession(7, 30000, 0, 20000, 1);
    QCOMPARE(f.size(), 18);
    QCOMPARE(static_cast<unsigned char>(f.at(0)), quint8(CmdOpenSession));
    QCOMPARE(static_cast<unsigned char>(f.at(4)), quint8(7));
    QCOMPARE(static_cast<unsigned char>(f.at(8)), quint8(30000 & 0xFF));
    QCOMPARE(static_cast<unsigned char>(f.at(9)), quint8((30000 >> 8) & 0xFF));
    // hb_timeout LE
    QCOMPARE(static_cast<unsigned char>(f.at(12)), quint8(20000 & 0xFF));
    QCOMPARE(static_cast<unsigned char>(f.at(13)), quint8((20000 >> 8) & 0xFF));
    // version (last 4 bytes) = 1 LE
    QCOMPARE(static_cast<unsigned char>(f.at(14)), quint8(1));
    QCOMPARE(static_cast<unsigned char>(f.at(15)), quint8(0));
}

void TestIderCodec::openSessionReplyRoundTrip()
{
    QByteArray r(30, '\0');
    r[0] = 0x41;
    r[8] = 0x01; r[9] = 0x02;     // major.minor
    r[10] = 0x03; r[11] = 0x04;   // fwmajor.fwminor
    r[16] = 0x00; r[17] = 0x10;   // readbuffer = 0x1000
    r[18] = 0x00; r[19] = 0x20;   // writebuffer = 0x2000
    r[21] = 0x00;                 // proto = 0
    r[25] = 0x39; r[26] = 0x05; r[27] = 0x00; r[28] = 0x00; // iana
    r[29] = 0x02;                 // oemlen = 2
    r.append(QByteArrayLiteral("OE"));

    SessionInfo info;
    int consumed = -1;
    QVERIFY(tryParseOpenSessionReply(r, &info, &consumed));
    QCOMPARE(consumed, 32);
    QCOMPARE(info.major, quint8(1));
    QCOMPARE(info.minor, quint8(2));
    QCOMPARE(info.fwMajor, quint8(3));
    QCOMPARE(info.fwMinor, quint8(4));
    QCOMPARE(info.readBuffer, quint16(0x1000));
    QCOMPARE(info.writeBuffer, quint16(0x2000));
    QCOMPARE(info.proto, quint8(0));
    QCOMPARE(info.iana, quint32(0x00000539));
    QCOMPARE(info.oemData, QByteArrayLiteral("OE"));
}

void TestIderCodec::enableFeaturesLayout()
{
    QByteArray data(4, '\0');
    data[0] = 0x11; // 0x01 enable | 0x10 Graceful
    QByteArray f = buildEnableFeatures(3, kStatusRegsToggle, data);
    QCOMPARE(static_cast<unsigned char>(f.at(0)), quint8(CmdEnableFeatures));
    QCOMPARE(static_cast<unsigned char>(f.at(4)), quint8(3));
    QCOMPARE(static_cast<unsigned char>(f.at(8)), quint8(kStatusRegsToggle));
    QCOMPARE(static_cast<unsigned char>(f.at(9)), quint8(0x11));
}

void TestIderCodec::commandWrittenParse()
{
    QByteArray f(28, '\0');
    f[0] = 0x50;
    f[9] = 0x01;       // feature register = 1 (DMA)
    f[14] = 0x10;      // device flags → CD-ROM
    // CDB at offset 16, 12 bytes: a READ_10 request
    f[16] = 0x28;
    // LBA (big endian) at offset 2..5 of CDB → offsets 18..21
    f[18] = 0x00; f[19] = 0x00; f[20] = 0x00; f[21] = 0x40; // lba = 64
    // length (big endian) at offset 7..8 of CDB → 23..24
    f[23] = 0x00; f[24] = 0x08; // 8 blocks

    ScsiCommand cmd;
    int consumed = -1;
    QVERIFY(tryParseCommandWritten(f, &cmd, &consumed));
    QCOMPARE(consumed, 28);
    QCOMPARE(cmd.featureRegister, quint8(0x01));
    QCOMPARE(cmd.deviceFlags, quint8(0x10));
    QCOMPARE(cmd.cdb.size(), 12);
    QCOMPARE(static_cast<unsigned char>(cmd.cdb.at(0)), quint8(0x28));
    QCOMPARE(scsi::readBe32(cmd.cdb, 2), quint32(64));
    QCOMPARE(scsi::readBe16(cmd.cdb, 7), quint16(8));
}

void TestIderCodec::sendDataToHostLayout()
{
    QByteArray data = QByteArrayLiteral("HELLO");
    QByteArray f = buildSendDataToHost(5, kDeviceCdRom, true, data, false);
    QCOMPARE(static_cast<unsigned char>(f.at(0)), quint8(CmdSendDataToHost));
    QCOMPARE(static_cast<unsigned char>(f.at(3)), quint8(0x02)); // completed bit
    QCOMPARE(static_cast<unsigned char>(f.at(4)), quint8(5));    // sequence
    // Header body data length at body offset 1..2
    QCOMPARE(static_cast<unsigned char>(f.at(8 + 1)), quint8(data.size() & 0xFF));
    QCOMPARE(static_cast<unsigned char>(f.at(8 + 4)), quint8(0xB5)); // PIO
    QCOMPARE(static_cast<unsigned char>(f.at(8 + 10)), quint8(kDeviceCdRom));
    QCOMPARE(f.mid(8 + 26), data);

    // DMA case
    QByteArray fd = buildSendDataToHost(6, kDeviceCdRom, false, data, true);
    QCOMPARE(static_cast<unsigned char>(fd.at(3)), quint8(0x01));
    QCOMPARE(static_cast<unsigned char>(fd.at(8 + 4)), quint8(0xB4));
}

void TestIderCodec::commandEndResponseLayout()
{
    QByteArray ok = buildCommandEndResponse(2, false, scsi::kSenseNoSense, kDeviceCdRom, 0, 0);
    QCOMPARE(ok.size(), 8 + 23);
    QCOMPARE(static_cast<unsigned char>(ok.at(0)), quint8(CmdCommandEndResponse));
    QCOMPARE(static_cast<unsigned char>(ok.at(3)), quint8(0x02));
    QCOMPARE(static_cast<unsigned char>(ok.at(8 + 12)), quint8(0x87));
    QCOMPARE(static_cast<unsigned char>(ok.at(8 + 18)), quint8(kDeviceCdRom));
    QCOMPARE(static_cast<unsigned char>(ok.at(8 + 19)), quint8(0x51));

    QByteArray err = buildCommandEndResponse(2, true, 0, kDeviceCdRom, 0, 0);
    QCOMPARE(err.size(), 8 + 23);
    QCOMPARE(static_cast<unsigned char>(err.at(8 + 12)), quint8(0xC5));
    QCOMPARE(static_cast<unsigned char>(err.at(8 + 19)), quint8(0x50));
}

void TestIderCodec::fixedFramesAndStatusData()
{
    QByteArray heartbeat(8, '\0');
    heartbeat[0] = 0x4B;
    int consumed = -1;
    QVERIFY(tryParseFixed(heartbeat, CmdHeartbeat, 8, &consumed));
    QCOMPARE(consumed, 8);

    QByteArray reset(9, '\0');
    reset[0] = 0x46;
    QVERIFY(tryParseFixed(reset, CmdResetOccurred, 9, &consumed));
    QCOMPARE(consumed, 9);

    QByteArray status(13, '\0');
    status[0] = 0x49;
    status[8] = 0x02;          // type = REGS_STATUS
    status[9] = 0x03; status[10] = 0x00; status[11] = 0x00; status[12] = 0x00;
    StatusData sd;
    QVERIFY(tryParseStatusData(status, &sd, &consumed));
    QCOMPARE(consumed, 13);
    QCOMPARE(sd.type, quint8(2));
    QCOMPARE(sd.value, quint32(3));
}

void TestIderCodec::scsiBeHelpers()
{
    QByteArray b(4, '\0');
    b[0] = 0x12; b[1] = 0x34; b[2] = 0x56; b[3] = 0x78;
    QCOMPARE(scsi::readBe32(b, 0), quint32(0x12345678));
    QCOMPARE(scsi::readBe16(b, 0), quint16(0x1234));
    QByteArray packed = scsi::packBe32(0xDEADBEEF);
    QCOMPARE(static_cast<unsigned char>(packed.at(0)), quint8(0xDE));
    QCOMPARE(static_cast<unsigned char>(packed.at(3)), quint8(0xEF));
}

QTEST_GUILESS_MAIN(TestIderCodec)
#include "test_ider_codec.moc"
