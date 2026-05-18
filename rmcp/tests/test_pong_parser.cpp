// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "rmcp/pong.h"

#include <QtTest>

using namespace qumesh::rmcp;

class TestPongParser : public QObject
{
    Q_OBJECT
private slots:
    void pingHasIntelEnterpriseAndTag();
    void validAmtPongIsParsed();
    void dualPortsBitMapsToTlsCapable();
    void provisioningStateNibble();
    void shortDatagramRejected();
    void wrongEnterpriseRejected();
    void wrongMessageTypeRejected();
    void wrongMessageClassRejected();
    void reservedProvisioningStateRejected();
};

namespace {

// A minimal, hand-built presence-pong reply. AMT firmware sends 28 bytes
// in the wild but only the first 22 carry the load-bearing fields the
// parser cares about — extra payload after byte 21 is ignored.
QByteArray makePong(quint8 tag, quint8 versionByte, quint8 stateByte,
                    quint16 openPort = 16992)
{
    QByteArray b(22, '\0');
    b[0]  = 0x06; // RMCP version
    b[1]  = 0x00;
    b[2]  = 0x00;
    b[3]  = 0x06; // ASF message class
    b[4]  = 0x00;
    b[5]  = 0x00;
    b[6]  = 0x11; // Intel enterprise high
    b[7]  = static_cast<char>(0xBE); // Intel enterprise low
    b[8]  = 0x40; // presence pong
    b[9]  = static_cast<char>(tag);
    b[10] = 0x00;
    b[11] = 0x10; // data length (16 bytes follow)
    b[16] = static_cast<char>((openPort >> 8) & 0xFF);
    b[17] = static_cast<char>(openPort & 0xFF);
    b[18] = static_cast<char>(versionByte);
    b[19] = static_cast<char>(stateByte);
    b[20] = 0x00;
    b[21] = 0x00;
    return b;
}

} // namespace

void TestPongParser::pingHasIntelEnterpriseAndTag()
{
    const QByteArray p = buildPing(0x42);
    QCOMPARE(p.size(), 12);
    QCOMPARE(static_cast<quint8>(p[0]), quint8(0x06));
    QCOMPARE(static_cast<quint8>(p[3]), quint8(0x06));
    // Intel IANA = 0x000011BE.
    QCOMPARE(static_cast<quint8>(p[4]), quint8(0x00));
    QCOMPARE(static_cast<quint8>(p[5]), quint8(0x00));
    QCOMPARE(static_cast<quint8>(p[6]), quint8(0x11));
    QCOMPARE(static_cast<quint8>(p[7]), quint8(0xBE));
    QCOMPARE(static_cast<quint8>(p[8]), quint8(0x80)); // ping message type
    QCOMPARE(static_cast<quint8>(p[9]), quint8(0x42)); // tag echoed
}

void TestPongParser::validAmtPongIsParsed()
{
    // Version 16.0, post-provisioning, single port only.
    const QByteArray dg = makePong(0x42, 0xA0, 0x02);
    const auto pong = parsePong(dg);
    QVERIFY(pong.has_value());
    QCOMPARE(pong->tag, quint8(0x42));
    QCOMPARE(pong->versionMajor, quint8(0x0A));
    QCOMPARE(pong->versionMinor, quint8(0x00));
    QCOMPARE(pong->provisioning, ProvisioningState::PostProvisioning);
    QVERIFY(!pong->dualPorts);
    QCOMPARE(pong->openPort, quint16(16992));
}

void TestPongParser::dualPortsBitMapsToTlsCapable()
{
    // bit 2 of byte 19 set ⇒ both 16992 and 16993 listening.
    const QByteArray dg = makePong(0x10, 0xB0, 0x06 /* dualports | postprov */);
    const auto pong = parsePong(dg);
    QVERIFY(pong.has_value());
    QVERIFY(pong->dualPorts);
    QCOMPARE(pong->provisioning, ProvisioningState::PostProvisioning);
    QCOMPARE(pong->versionMajor, quint8(0x0B));
}

void TestPongParser::provisioningStateNibble()
{
    QVERIFY(parsePong(makePong(0x01, 0x00, 0x00))->provisioning
            == ProvisioningState::PreProvisioning);
    QVERIFY(parsePong(makePong(0x01, 0x00, 0x01))->provisioning
            == ProvisioningState::InProcess);
    QVERIFY(parsePong(makePong(0x01, 0x00, 0x02))->provisioning
            == ProvisioningState::PostProvisioning);
}

void TestPongParser::shortDatagramRejected()
{
    QVERIFY(!parsePong(QByteArray(15, '\0')).has_value());
    QVERIFY(!parsePong(QByteArray()).has_value());
}

void TestPongParser::wrongEnterpriseRejected()
{
    QByteArray dg = makePong(0x42, 0xA0, 0x02);
    // Stomp Intel's enterprise to something else.
    dg[7] = static_cast<char>(0xBF);
    QVERIFY(!parsePong(dg).has_value());
}

void TestPongParser::wrongMessageTypeRejected()
{
    QByteArray dg = makePong(0x42, 0xA0, 0x02);
    // ASF "open session" instead of "presence pong" — must reject.
    dg[8] = 0x41;
    QVERIFY(!parsePong(dg).has_value());
}

void TestPongParser::wrongMessageClassRejected()
{
    QByteArray dg = makePong(0x42, 0xA0, 0x02);
    dg[3] = 0x07; // Not ASF.
    QVERIFY(!parsePong(dg).has_value());
}

void TestPongParser::reservedProvisioningStateRejected()
{
    // prov bits = 3 is reserved in the spec; we treat as malformed.
    QVERIFY(!parsePong(makePong(0x01, 0x00, 0x03)).has_value());
}

QTEST_APPLESS_MAIN(TestPongParser)
#include "test_pong_parser.moc"
