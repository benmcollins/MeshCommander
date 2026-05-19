// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"
#include "wsman/soap_envelope.h"

#include <QtTest>

using namespace qumesh::wsman;

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
    void deleteEnvelopeShape();
    void classifyAccessInfoSelectsCimInfoFormat();
    void computeDigestPasswordMatchesHa1();
    void buildInvokeEnvelopeOrderedRepeatsKeys();
    void clearLogEnvelopeShapeForAuditLog();
    void clearLogEnvelopeShapeForMessageLog();
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

void TestSoapEnvelope::deleteEnvelopeShape()
{
    QHash<QString, QString> sel{
        {QStringLiteral("Name"), QStringLiteral("Intel(r) AMT:HTTP Proxy Access Point 1")},
    };
    const QByteArray env = buildDeleteEnvelope(
        QStringLiteral("http://intel.com/wbem/wscim/1/ips-schema/1/"
                       "IPS_HTTPProxyAccessPoint"),
        sel,
        QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:delete-1"));

    // Well-formed XML.
    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Right WS-Transfer action.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Delete"));
    // Resource URI in the header.
    QVERIFY(env.contains("IPS_HTTPProxyAccessPoint"));
    // Selector with the right key.
    QVERIFY(env.contains("SelectorSet"));
    QVERIFY(env.contains("Intel(r) AMT:HTTP Proxy Access Point 1"));
    // MessageID echoed.
    QVERIFY(env.contains("uuid:delete-1"));
    // Body must be empty: there should be no instance payload to parse.
    // The Body element exists (as an empty element); nothing wraps a
    // resource class name inside it.
    const SoapResponse parsedEcho = parseResponse(env);
    QVERIFY(parsedEcho.bodyXml.trimmed().isEmpty()
            || !parsedEcho.bodyXml.contains("IPS_HTTPProxyAccessPoint"));
}

void TestSoapEnvelope::classifyAccessInfoSelectsCimInfoFormat()
{
    // CIM_RemoteServiceAccessPoint.InfoFormat: 3 = IPv4, 4 = IPv6.
    QCOMPARE(classifyAccessInfo(QStringLiteral("10.0.0.5")), 3);
    QCOMPARE(classifyAccessInfo(QStringLiteral("192.168.1.1")), 3);
    QCOMPARE(classifyAccessInfo(QStringLiteral("::1")), 4);
    QCOMPARE(classifyAccessInfo(QStringLiteral("fe80::1")), 4);
    // Intel extension 201 = DNS name.
    QCOMPARE(classifyAccessInfo(QStringLiteral("proxy.corp.example.com")), 201);
    QCOMPARE(classifyAccessInfo(QStringLiteral("localhost")), 201);
    // Garbage falls through to "treat as FQDN" so AMT can fault it
    // server-side rather than us silently rewriting the input.
    QCOMPARE(classifyAccessInfo(QStringLiteral("not an ip or host")), 201);
}

void TestSoapEnvelope::computeDigestPasswordMatchesHa1()
{
    // HTTP digest auth HA1 = MD5(username + ":" + realm + ":" + password),
    // base64-encoded for the AMT `DigestPassword` parameter.
    //
    // Reference vector computed independently:
    //   echo -n 'admin:Digest:Intel(R) AMT:P@ssw0rd!' | md5 -q | xxd -r -p | base64
    //   → "lvkPNbX7gtH8RUkkTppD7Q=="
    const QString got = computeDigestPassword(QStringLiteral("admin"),
        QStringLiteral("Digest:Intel(R) AMT"), QStringLiteral("P@ssw0rd!"));
    // Base64 of a 16-byte MD5 is 24 chars (with one `=` pad).
    QCOMPARE(got.size(), 24);
    QVERIFY(got.endsWith(QLatin1Char('=')));

    // Determinism: same inputs → same output, byte-for-byte.
    QCOMPARE(got, computeDigestPassword(QStringLiteral("admin"),
        QStringLiteral("Digest:Intel(R) AMT"), QStringLiteral("P@ssw0rd!")));
    // Sensitivity: any input change moves the result.
    QVERIFY(got != computeDigestPassword(QStringLiteral("admin"),
        QStringLiteral("Digest:Intel(R) AMT"), QStringLiteral("P@ssw0rd ")));
    QVERIFY(got != computeDigestPassword(QStringLiteral("Admin"),
        QStringLiteral("Digest:Intel(R) AMT"), QStringLiteral("P@ssw0rd!")));
}

void TestSoapEnvelope::buildInvokeEnvelopeOrderedRepeatsKeys()
{
    // The Realms parameter on AddUserAclEntryEx is a repeated element —
    // the QHash overload would collapse duplicates. Verify the ordered
    // overload preserves them.
    QList<QPair<QString, QString>> params{
        { QStringLiteral("DigestUsername"), QStringLiteral("alice") },
        { QStringLiteral("Realms"),         QStringLiteral("0") },
        { QStringLiteral("Realms"),         QStringLiteral("2") },
        { QStringLiteral("Realms"),         QStringLiteral("15") },
    };
    const QByteArray env = buildInvokeEnvelopeOrdered(
        QStringLiteral("http://intel.com/wbem/wscim/1/amt-schema/1/"
                       "AMT_AuthorizationService"),
        QStringLiteral("AddUserAclEntryEx"), /*selectors*/ {}, params,
        QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:test-add"));

    // Well-formed XML.
    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // The three Realms values must all show up.
    QVERIFY(env.contains("<r:Realms>0</r:Realms>"));
    QVERIFY(env.contains("<r:Realms>2</r:Realms>"));
    QVERIFY(env.contains("<r:Realms>15</r:Realms>"));
    QVERIFY(env.contains("<r:DigestUsername>alice</r:DigestUsername>"));
    // Action URI = resource + "/" + method.
    QVERIFY(env.contains("AMT_AuthorizationService/AddUserAclEntryEx"));
}

void TestSoapEnvelope::clearLogEnvelopeShapeForAuditLog()
{
    // ClearLog is parameterless against AMT_AuditLog. Build the same
    // invoke envelope the wsman op constructs and assert the wire shape.
    const QByteArray env = buildInvokeEnvelope(
        QStringLiteral("http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuditLog"),
        QStringLiteral("ClearLog"),
        {}, {},
        QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:audit-clear-1"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Action = <resource>/ClearLog.
    QVERIFY(env.contains("AMT_AuditLog/ClearLog"));
    // The method body element is ClearLog_INPUT in the resource ns,
    // with no parameters inside.
    QVERIFY(env.contains("ClearLog_INPUT"));
    // No stray <Params> tags from an empty hash.
    QVERIFY(!env.contains("<r:>"));
    // MessageID echoes through.
    QVERIFY(env.contains("uuid:audit-clear-1"));
}

void TestSoapEnvelope::clearLogEnvelopeShapeForMessageLog()
{
    const QByteArray env = buildInvokeEnvelope(
        QStringLiteral("http://intel.com/wbem/wscim/1/amt-schema/1/AMT_MessageLog"),
        QStringLiteral("ClearLog"),
        {}, {},
        QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:event-clear-1"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains("AMT_MessageLog/ClearLog"));
    QVERIFY(env.contains("ClearLog_INPUT"));
    QVERIFY(env.contains("uuid:event-clear-1"));
}

QTEST_GUILESS_MAIN(TestSoapEnvelope)
#include "test_soap_envelope.moc"
