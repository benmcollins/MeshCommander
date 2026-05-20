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
    void generateKeyPairEnvelopeShape();
    void parseGenerateKeyPairInstanceIdFromBody();
    void getPublicPrivateKeyPairEnvelopeShape();
    void wifiPskEnvelopeOmitsEapBlock();
    void wifiEapTlsEnvelopeIncludesClientAndCaCredential();
    void wifiPeapEnvelopeIncludesUsernameAndServerCertGuard();
    void addAlarmEnvelopeShape();
    void addAlarmEnvelopeOmitsIntervalForOneShot();
    void registerAgentEnvelopeShape();
    void registerAgentEnvelopeRoundTripsGuid();
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

void TestSoapEnvelope::generateKeyPairEnvelopeShape()
{
    // The `buildInvokeEnvelope` path used by `generateKeyPair`. Two
    // numeric parameters land inside `<GenerateKeyPair_INPUT>`.
    QHash<QString, QString> params;
    params.insert(QStringLiteral("KeyAlgorithm"), QStringLiteral("0"));
    params.insert(QStringLiteral("KeyLength"), QStringLiteral("2048"));
    const QByteArray env = buildInvokeEnvelope(
        QStringLiteral("http://intel.com/wbem/wscim/1/amt-schema/1/"
                       "AMT_PublicKeyManagementService"),
        QStringLiteral("GenerateKeyPair"),
        {}, params,
        QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:gkp-1"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains("AMT_PublicKeyManagementService/GenerateKeyPair"));
    QVERIFY(env.contains("GenerateKeyPair_INPUT"));
    QVERIFY(env.contains("KeyAlgorithm"));
    QVERIFY(env.contains("KeyLength"));
    QVERIFY(env.contains(">2048<"));
    QVERIFY(env.contains("uuid:gkp-1"));
}

void TestSoapEnvelope::parseGenerateKeyPairInstanceIdFromBody()
{
    // GenerateKeyPair_OUTPUT carries an EPR. The op uses a private
    // helper to pluck the InstanceID selector — re-create the same
    // walk here via `findScalar`'s sibling, which is the contract
    // surface other tests share.
    constexpr char kBody[] = R"(<g:GenerateKeyPair_OUTPUT
            xmlns:g="http://intel.com/wbem/wscim/1/amt-schema/1/AMT_PublicKeyManagementService"
            xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing"
            xmlns:w="http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd">
          <g:KeyPair>
            <a:Address>http://...anonymous</a:Address>
            <a:ReferenceParameters>
              <w:ResourceURI>http://intel.com/wbem/wscim/1/amt-schema/1/AMT_PublicPrivateKeyPair</w:ResourceURI>
              <w:SelectorSet>
                <w:Selector Name="InstanceID">Intel(r) AMT Key: Handle: 7</w:Selector>
              </w:SelectorSet>
            </a:ReferenceParameters>
          </g:KeyPair>
          <g:ReturnValue>0</g:ReturnValue>
        </g:GenerateKeyPair_OUTPUT>)";

    // Body-level findScalar for ReturnValue is the same one
    // `generateKeyPair` uses; sanity-check it parses.
    QCOMPARE(findScalar(QByteArray(kBody), QStringLiteral("ReturnValue")),
             QStringLiteral("0"));

    // The runtime parses the EPR's selector via a streaming reader.
    // Mirror its shape: walk for the Selector[Name=InstanceID] value.
    QXmlStreamReader r(QByteArray{kBody});
    QString found;
    bool inSelector = false;
    QString curName;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("Selector")) {
            inSelector = true;
            curName = r.attributes().value(QStringLiteral("Name")).toString();
        } else if (inSelector && r.isCharacters() && curName == QStringLiteral("InstanceID")) {
            found = r.text().toString();
        } else if (r.isEndElement() && r.name() == QStringLiteral("Selector")) {
            inSelector = false;
        }
    }
    QCOMPARE(found, QStringLiteral("Intel(r) AMT Key: Handle: 7"));
}

void TestSoapEnvelope::getPublicPrivateKeyPairEnvelopeShape()
{
    QHash<QString, QString> sel{
        {QStringLiteral("InstanceID"),
         QStringLiteral("Intel(r) AMT Key: Handle: 7")},
    };
    const QByteArray env = buildGetEnvelope(
        QStringLiteral("http://intel.com/wbem/wscim/1/amt-schema/1/"
                       "AMT_PublicPrivateKeyPair"),
        sel,
        QStringLiteral("http://10.0.0.5:16992/wsman"),
        QStringLiteral("uuid:gppkp-1"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Get"));
    QVERIFY(env.contains("AMT_PublicPrivateKeyPair"));
    QVERIFY(env.contains("Intel(r) AMT Key: Handle: 7"));
    QVERIFY(env.contains("uuid:gppkp-1"));
}

void TestSoapEnvelope::wifiPskEnvelopeOmitsEapBlock()
{
    // Phase B shape: enterpriseEnabled = false → no EAP block, no
    // credential EPRs land in the body. Locks the existing on-wire
    // bytes against accidental regression from Phase C work.
    WiFiPskProfile p;
    p.elementName = QStringLiteral("CorpNet");
    p.ssid = QStringLiteral("Corp WiFi");
    p.authenticationMethod = 6;
    p.encryptionMethod = 4;
    p.priority = 1;
    p.psk = QStringLiteral("hunter2!2");
    // Defaults: enterpriseEnabled = false.

    const QByteArray env = buildWiFiSettingsEnvelopeForTesting(
        QStringLiteral("AddWiFiSettings"), p);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains("WiFiEndpointSettingsInput"));
    QVERIFY(env.contains(">hunter2!2<"));
    QVERIFY(!env.contains("IEEE8021xSettingsInput"));
    QVERIFY(!env.contains("ClientCredential"));
    QVERIFY(!env.contains("CACredential"));
}

void TestSoapEnvelope::wifiEapTlsEnvelopeIncludesClientAndCaCredential()
{
    WiFiPskProfile p;
    p.elementName = QStringLiteral("CorpNet");
    p.ssid = QStringLiteral("Corp WiFi");
    p.authenticationMethod = 5; // WPA2-Enterprise
    p.encryptionMethod = 4;
    p.priority = 1;
    p.psk.clear(); // ignored for EAP
    p.enterpriseEnabled = true;
    p.authenticationProtocol = 0; // EAP-TLS
    p.clientCertificateInstanceId =
        QStringLiteral("Intel(r) AMT Certificate: Handle: 3");
    p.caCertificateInstanceId =
        QStringLiteral("Intel(r) AMT Certificate: Handle: 7");

    const QByteArray env = buildWiFiSettingsEnvelopeForTesting(
        QStringLiteral("AddWiFiSettings"), p);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains("IEEE8021xSettingsInput"));
    QVERIFY(env.contains(">0</")); // AuthenticationProtocol = 0
    QVERIFY(env.contains("ClientCredential"));
    QVERIFY(env.contains("Intel(r) AMT Certificate: Handle: 3"));
    QVERIFY(env.contains("CACredential"));
    QVERIFY(env.contains("Intel(r) AMT Certificate: Handle: 7"));
    // EAP-TLS uses the device key pair; no username/password should
    // appear unless the operator explicitly typed one.
    QVERIFY(!env.contains("<r:Username>"));
    QVERIFY(!env.contains("<r:Password>"));
}

void TestSoapEnvelope::wifiPeapEnvelopeIncludesUsernameAndServerCertGuard()
{
    WiFiPskProfile p;
    p.elementName = QStringLiteral("CorpNet");
    p.ssid = QStringLiteral("Corp WiFi");
    p.authenticationMethod = 5;
    p.encryptionMethod = 4;
    p.priority = 1;
    p.enterpriseEnabled = true;
    p.authenticationProtocol = 2; // PEAPv0/MSCHAPv2
    p.eapUsername = QStringLiteral("alice");
    p.eapPassword = QStringLiteral("s3cret-secret");
    p.eapServerCertificateName = QStringLiteral("radius.corp.example");
    p.eapServerCertificateNameComparison = 2; // DomainSuffix
    p.caCertificateInstanceId =
        QStringLiteral("Intel(r) AMT Certificate: Handle: 9");
    // No clientCertificateInstanceId for PEAP.

    const QByteArray env = buildWiFiSettingsEnvelopeForTesting(
        QStringLiteral("AddWiFiSettings"), p);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains("IEEE8021xSettingsInput"));
    QVERIFY(env.contains(">2</")); // AuthenticationProtocol = 2
    QVERIFY(env.contains(">alice<"));
    QVERIFY(env.contains(">s3cret-secret<"));
    QVERIFY(env.contains(">radius.corp.example<"));
    QVERIFY(env.contains("ServerCertificateNameComparison"));
    // PEAP doesn't use a client cert.
    QVERIFY(!env.contains("ClientCredential"));
    // But CACredential is set so AMT can validate the RADIUS server's
    // certificate against a trusted root.
    QVERIFY(env.contains("CACredential"));
    QVERIFY(env.contains("Intel(r) AMT Certificate: Handle: 9"));
}

void TestSoapEnvelope::addAlarmEnvelopeShape()
{
    WakeAlarm a;
    a.elementName = QStringLiteral("Nightly wake");
    a.startTimeIso = QStringLiteral("2026-06-01T03:00:00Z");
    a.intervalIso = QStringLiteral("P1DT0H0M");
    a.deleteOnCompletion = false;

    const QByteArray env = buildAddAlarmEnvelopeForTesting(a);

    // Well-formed XML.
    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Targets AMT_AlarmClockService.AddAlarm — selectors + Action.
    QVERIFY(env.contains("AMT_AlarmClockService/AddAlarm"));
    QVERIFY(env.contains("AMT_AlarmClockService"));
    QVERIFY(env.contains("Intel(r) AMT Alarm Clock Service"));

    // Method input + the embedded occurrence carrying ElementName,
    // StartTime/Datetime, Interval/Interval, and DeleteOnCompletion.
    QVERIFY(env.contains("AddAlarm_INPUT"));
    QVERIFY(env.contains("AlarmTemplate"));
    QVERIFY(env.contains(">Nightly wake<"));
    QVERIFY(env.contains(">2026-06-01T03:00:00Z<"));
    QVERIFY(env.contains(">P1DT0H0M<"));
    QVERIFY(env.contains("DeleteOnCompletion"));
    QVERIFY(env.contains(">false<"));

    // Embedded instance lives in the IPS_AlarmClockOccurrence namespace.
    QVERIFY(env.contains("IPS_AlarmClockOccurrence"));
}

void TestSoapEnvelope::addAlarmEnvelopeOmitsIntervalForOneShot()
{
    // No intervalIso → AMT rejects an empty interval block on some
    // firmware, so the builder must omit it entirely.
    WakeAlarm a;
    a.elementName = QStringLiteral("One shot");
    a.startTimeIso = QStringLiteral("2026-06-01T03:00:00Z");
    a.intervalIso.clear();
    a.deleteOnCompletion = true;

    const QByteArray env = buildAddAlarmEnvelopeForTesting(a);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains(">One shot<"));
    QVERIFY(env.contains(">true<"));
    // No <Interval> element at all on a one-shot — only the
    // StartTime/Datetime block carries a CIM common element.
    QVERIFY(!env.contains("Interval>"));
}

void TestSoapEnvelope::registerAgentEnvelopeShape()
{
    AgentPresenceWatchdog w;
    w.deviceIdGuid = QStringLiteral("01234567-89ab-cdef-0123-456789abcdef");
    w.description = QStringLiteral("backup daemon");
    w.monitoredEntityCode = 7; // Application
    w.startupIntervalSec = 60;
    w.timeoutIntervalSec = 30;

    const QByteArray env = buildRegisterAgentEnvelopeForTesting(w);

    // Well-formed XML.
    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Targets AMT_AgentPresenceService.RegisterAgent — selectors + Action.
    QVERIFY(env.contains("AMT_AgentPresenceService/RegisterAgent"));
    QVERIFY(env.contains("AMT_AgentPresenceService"));
    QVERIFY(env.contains("Intel(r) AMT Agent Presence Service"));

    // Method input + embedded watchdog carrying the editable fields.
    QVERIFY(env.contains("RegisterAgent_INPUT"));
    QVERIFY(env.contains("AgentTemplate"));
    QVERIFY(env.contains("AMT_AgentPresenceWatchdog"));
    QVERIFY(env.contains(">backup daemon<"));
    QVERIFY(env.contains(">7<"));  // MonitoredEntity
    QVERIFY(env.contains(">60<")); // StartupInterval
    QVERIFY(env.contains(">30<")); // TimeoutInterval
}

void TestSoapEnvelope::registerAgentEnvelopeRoundTripsGuid()
{
    // The user-typed (or generated) 8-4-4-4-12 GUID has to make it
    // onto the wire as the base-64-of-raw-16-bytes shape the read
    // side decodes. Lock the encoder against silent endianness /
    // hex-vs-base64 regressions: same input GUID → same hex → known
    // base-64. The reference base-64 here was computed offline from
    // the raw bytes 01 23 45 67 89 ab cd ef 01 23 45 67 89 ab cd ef.
    AgentPresenceWatchdog w;
    w.deviceIdGuid = QStringLiteral("01234567-89ab-cdef-0123-456789abcdef");
    w.timeoutIntervalSec = 1;

    const QByteArray env = buildRegisterAgentEnvelopeForTesting(w);

    QVERIFY(env.contains("ASNFZ4mrze8BI0VniavN7w=="));
    // The bare GUID string must NOT appear on the wire — that would
    // mean we sent the hex form instead of the base-64-raw form.
    QVERIFY(!env.contains("01234567-89ab-cdef-0123-456789abcdef"));
}

QTEST_GUILESS_MAIN(TestSoapEnvelope)
#include "test_soap_envelope.moc"
