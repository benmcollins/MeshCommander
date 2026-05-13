// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/soap_envelope.h"

#include <QtTest>

using namespace meshcommander::wsman;

class TestSoapEnvelope : public QObject
{
    Q_OBJECT
private slots:
    void identifyEnvelopeIsValidXml();
    void identifyEnvelopeContainsIdentifyElement();
    void getEnvelopeIncludesResourceUriAndMessageId();
    void getEnvelopeWithSelectors();
    void parseIdentifyResponseExtractsBody();
    void parseGetResponseHeaders();
    void parseFault();
    void findScalarExtractsPowerState();
};

namespace {

constexpr char kIdentifyResponse[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
            xmlns:wsmid="http://schemas.dmtf.org/wbem/wsman/identity/1/wsmanidentity.xsd">
  <s:Header/>
  <s:Body>
    <wsmid:IdentifyResponse>
      <wsmid:ProtocolVersion>http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd</wsmid:ProtocolVersion>
      <wsmid:ProductVendor>Intel(r) AMT</wsmid:ProductVendor>
      <wsmid:ProductVersion>16.0.0</wsmid:ProductVersion>
    </wsmid:IdentifyResponse>
  </s:Body>
</s:Envelope>)";

constexpr char kPowerStateResponse[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
            xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing"
            xmlns:cim="http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_AssociatedPowerManagementService">
  <s:Header>
    <a:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/GetResponse</a:Action>
    <a:RelatesTo>uuid:42</a:RelatesTo>
  </s:Header>
  <s:Body>
    <cim:CIM_AssociatedPowerManagementService>
      <cim:PowerState>2</cim:PowerState>
      <cim:RequestedPowerState>2</cim:RequestedPowerState>
    </cim:CIM_AssociatedPowerManagementService>
  </s:Body>
</s:Envelope>)";

constexpr char kFaultResponse[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope">
  <s:Header/>
  <s:Body>
    <s:Fault>
      <s:Code><s:Value>s:Sender</s:Value></s:Code>
      <s:Reason><s:Text>access denied</s:Text></s:Reason>
    </s:Fault>
  </s:Body>
</s:Envelope>)";

} // namespace

void TestSoapEnvelope::identifyEnvelopeIsValidXml()
{
    const QByteArray env = buildIdentifyEnvelope();
    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));
}

void TestSoapEnvelope::identifyEnvelopeContainsIdentifyElement()
{
    const QByteArray env = buildIdentifyEnvelope();
    QVERIFY(env.contains(":Identify"));
    QVERIFY(env.contains("Envelope"));
}

void TestSoapEnvelope::getEnvelopeIncludesResourceUriAndMessageId()
{
    const QByteArray env = buildGetEnvelope(
        QStringLiteral("http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
                       "CIM_AssociatedPowerManagementService"),
        {}, QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:test-1"));
    QVERIFY(env.contains("CIM_AssociatedPowerManagementService"));
    QVERIFY(env.contains("uuid:test-1"));
    QVERIFY(env.contains("http://10.0.0.5:16992/wsman"));
}

void TestSoapEnvelope::getEnvelopeWithSelectors()
{
    QHash<QString, QString> sel{
        {QStringLiteral("InstanceID"), QStringLiteral("amt:42")},
    };
    const QByteArray env = buildGetEnvelope(
        QStringLiteral("urn:example"), sel,
        QStringLiteral("http://device/wsman"), QStringLiteral("uuid:test-2"));
    QVERIFY(env.contains("SelectorSet"));
    QVERIFY(env.contains("InstanceID"));
    QVERIFY(env.contains("amt:42"));
}

void TestSoapEnvelope::parseIdentifyResponseExtractsBody()
{
    const SoapResponse r = parseResponse(QByteArray(kIdentifyResponse));
    QVERIFY(!r.isFault());
    QVERIFY(r.bodyXml.contains("IdentifyResponse"));
    QVERIFY(r.bodyXml.contains("Intel(r) AMT"));
}

void TestSoapEnvelope::parseGetResponseHeaders()
{
    const SoapResponse r = parseResponse(QByteArray(kPowerStateResponse));
    QVERIFY(!r.isFault());
    QCOMPARE(r.headerAction,
             QStringLiteral("http://schemas.xmlsoap.org/ws/2004/09/transfer/GetResponse"));
    QCOMPARE(r.headerRelatesTo, QStringLiteral("uuid:42"));
}

void TestSoapEnvelope::parseFault()
{
    const SoapResponse r = parseResponse(QByteArray(kFaultResponse));
    QVERIFY(r.isFault());
    QVERIFY(r.fault.contains("access denied"));
}

void TestSoapEnvelope::findScalarExtractsPowerState()
{
    const SoapResponse r = parseResponse(QByteArray(kPowerStateResponse));
    QCOMPARE(findScalar(r.bodyXml, QStringLiteral("PowerState")), QStringLiteral("2"));
}

QTEST_GUILESS_MAIN(TestSoapEnvelope)
#include "test_soap_envelope.moc"
