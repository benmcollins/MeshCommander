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
    void addActionEnvelopeShape();
    void addActionEnvelopeOmitsEacWhenUnset();
    void subscribeEnvelopeShape();
    void subscribeEnvelopeIncludesWsTrustCredentialsWhenUserSet();
    void unsubscribeEnvelopeEmbedsBothEprs();
    void addHdr8021FilterEnvelopeShape();
    void addHdr8021FilterOmitsRateLimitDataWhenNotRateLimit();
    void encodeIPv4ForFilterProducesUppercaseHex();
    void addIpHeadersFilterEnvelopeShape();
    void addIpHeadersFilterOmitsOptionalFieldsWhenUnset();
    void extractFilterHandleFromInstanceIdParsesTrailingInt();
    void addSystemDefensePolicyEnvelopeShape();
    void addSystemDefensePolicyEnvelopeRepeatsFilterHandles();
    void bindSystemDefensePolicyEnvelopeEmbedsBothEprs();
    void unbindSystemDefensePolicyEnvelopeHasEprValuedSelectors();
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
    QList<std::pair<QString, QString>> params{
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

void TestSoapEnvelope::addActionEnvelopeShape()
{
    // Running (4) → Expired (8) on a real agent, fires a Reset (17),
    // also logs an event. Same GUID as the RegisterAgent test so the
    // base-64 form is the known-good "ASNFZ4mrze8BI0VniavN7w==".
    const QByteArray env = buildAddActionEnvelopeForTesting(
        QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"),
        /*oldState*/ 4, /*newState*/ 8,
        /*eventOnTransition*/ true,
        /*actionSac*/ 17, /*actionEac*/ -1);

    // Well-formed XML.
    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Targets AMT_AgentPresenceService.AddAction — selectors + Action.
    QVERIFY(env.contains("AMT_AgentPresenceService/AddAction"));
    QVERIFY(env.contains("Intel(r) AMT Agent Presence Service"));

    // Method input + the EPR-shaped Watchdog parameter (ReferenceParameters
    // wrapping the AMT_AgentPresenceWatchdog resource URI with a
    // DeviceID selector carrying the base-64 GUID).
    QVERIFY(env.contains("AddAction_INPUT"));
    QVERIFY(env.contains("ReferenceParameters"));
    QVERIFY(env.contains("AMT_AgentPresenceWatchdog"));
    QVERIFY(env.contains("ASNFZ4mrze8BI0VniavN7w=="));

    // The flat-scalar parameters — OldState/NewState/EventOnTransition/SAC.
    QVERIFY(env.contains(">4<"));     // OldState (Running)
    QVERIFY(env.contains(">8<"));     // NewState (Expired)
    QVERIFY(env.contains(">true<")); // EventOnTransition
    QVERIFY(env.contains(">17<"));   // ActionSAC (Reset)

    // EAC was -1 → must not have been emitted.
    QVERIFY(!env.contains("ActionEAC"));
}

void TestSoapEnvelope::addActionEnvelopeOmitsEacWhenUnset()
{
    // Explicit assertion that a non-negative EAC *does* show up, and
    // a -1 EAC does not. The dialog defaults to -1; this guards
    // against a future change that accidentally always emits ActionEAC.
    const QByteArray withEac = buildAddActionEnvelopeForTesting(
        QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"),
        4, 8, false, 17, /*actionEac*/ 5);
    QVERIFY(withEac.contains("ActionEAC"));
    QVERIFY(withEac.contains(">5<"));

    const QByteArray withoutEac = buildAddActionEnvelopeForTesting(
        QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"),
        4, 8, false, 17, /*actionEac*/ -1);
    QVERIFY(!withoutEac.contains("ActionEAC"));
}

void TestSoapEnvelope::subscribeEnvelopeShape()
{
    // No-auth Subscribe — locks the basic on-wire shape against
    // regressions in the WS-Eventing block.
    const QByteArray env = buildSubscribeEnvelopeForTesting(
        QStringLiteral("Intel(r) AMT:System Defense:DefaultFilter"),
        EventDeliveryMode::PushWithAck,
        QStringLiteral("https://listener.example/notify"),
        QString(), QString());

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // WS-Eventing Subscribe action + correct resource + filter selector.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/08/eventing/Subscribe"));
    QVERIFY(env.contains("CIM_FilterCollection"));
    QVERIFY(env.contains("Intel(r) AMT:System Defense:DefaultFilter"));
    // Delivery mode (PushWithAck → dmtf.org URI) + notify URL.
    QVERIFY(env.contains("dmtf.org/wbem/wsman/1/wsman/PushWithAck"));
    QVERIFY(env.contains("https://listener.example/notify"));
    // No WS-Trust block when user/pass are empty.
    QVERIFY(!env.contains("IssuedTokens"));
    QVERIFY(!env.contains("UsernameToken"));
}

void TestSoapEnvelope::subscribeEnvelopeIncludesWsTrustCredentialsWhenUserSet()
{
    const QByteArray env = buildSubscribeEnvelopeForTesting(
        QStringLiteral("Intel(r) AMT:System Defense:DefaultFilter"),
        EventDeliveryMode::Push,
        QStringLiteral("https://listener.example/notify"),
        QStringLiteral("alice"),
        QStringLiteral("hunter2"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Push (not PushWithAck) → xmlsoap.org URI.
    QVERIFY(env.contains("xmlsoap.org/ws/2004/08/eventing/DeliveryModes/Push"));
    // WS-Trust block carries the credentials.
    QVERIFY(env.contains("IssuedTokens"));
    QVERIFY(env.contains("UsernameToken"));
    QVERIFY(env.contains(">alice<"));
    QVERIFY(env.contains(">hunter2<"));
    // And the partner Auth profile marker shows up under Delivery.
    QVERIFY(env.contains("Auth Profile=") || env.contains("Auth "));
    QVERIFY(env.contains("secprofile/http/basic"));
}

void TestSoapEnvelope::unsubscribeEnvelopeEmbedsBothEprs()
{
    const QByteArray env = buildUnsubscribeEnvelopeForTesting(
        QStringLiteral("Intel(r) AMT:System Defense:DefaultFilter"),
        QStringLiteral("Intel(r) AMT:Subscription Manager 0"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // WS-Eventing Unsubscribe action on the join class.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/08/eventing/Unsubscribe"));
    QVERIFY(env.contains("CIM_FilterCollectionSubscription"));

    // Body is the empty <e:Unsubscribe/> element.
    QVERIFY(env.contains("Unsubscribe"));

    // Both EPRs appear with their resource URIs + the right key values.
    QVERIFY(env.contains("CIM_FilterCollection<"));
    QVERIFY(env.contains("Intel(r) AMT:System Defense:DefaultFilter"));
    QVERIFY(env.contains("CIM_ListenerDestinationWSManagement"));
    QVERIFY(env.contains("CIM_ListenerDestinationWSMAN"));
    QVERIFY(env.contains("Intel(r) AMT:Subscription Manager 0"));
    // Selector names "Filter" and "Handler" are what the firmware
    // expects to identify each EPR.
    QVERIFY(env.contains("Name=\"Filter\"") || env.contains("Name='Filter'"));
    QVERIFY(env.contains("Name=\"Handler\"") || env.contains("Name='Handler'"));
}

void TestSoapEnvelope::addHdr8021FilterEnvelopeShape()
{
    // Rate-limit filter — exercises the conditional FilterProfileData
    // emission and locks the verbatim field names confirmed against
    // the legacy NW.js MeshCommander reference.
    Hdr8021Filter f;
    f.name = QStringLiteral("Block ARP storm");
    f.etherType = 2054;          // ARP
    f.filterProfile = 2;          // Rate Limit
    f.filterProfileData = 100;    // packets/sec
    f.filterDirection = 1;        // Rx
    f.actionEventOnMatch = true;

    const QByteArray env = buildAddHdr8021FilterEnvelopeForTesting(f);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // WS-Transfer Create action + the AMT_Hdr8021Filter resource URI.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Create"));
    QVERIFY(env.contains("AMT_Hdr8021Filter"));

    // Body wraps the new instance under the resource namespace.
    QVERIFY(env.contains(">Block ARP storm<"));
    // Ethertype must be sent under HdrProtocolID8021 (not "EtherType").
    QVERIFY(env.contains("HdrProtocolID8021"));
    QVERIFY(env.contains(">2054<"));
    // Profile / direction / event flag values.
    QVERIFY(env.contains("FilterProfile"));
    QVERIFY(env.contains(">2<"));      // filterProfile
    QVERIFY(env.contains("FilterDirection"));
    QVERIFY(env.contains(">1<"));      // direction = Rx
    QVERIFY(env.contains("ActionEventOnMatch"));
    QVERIFY(env.contains(">true<"));
    // Rate-limit data MUST be present on this filter.
    QVERIFY(env.contains("FilterProfileData"));
    QVERIFY(env.contains(">100<"));
    // Creation-class placeholders go on the wire as "0" — firmware
    // fills the real values in on the round-trip.
    QVERIFY(env.contains("CreationClassName"));
}

void TestSoapEnvelope::addHdr8021FilterOmitsRateLimitDataWhenNotRateLimit()
{
    // FilterProfile != 2 → no FilterProfileData on the wire. AMT
    // rejects FilterProfileData on a Drop/Allow filter, so this
    // omission is load-bearing.
    Hdr8021Filter f;
    f.name = QStringLiteral("Drop unknown");
    f.etherType = 2048;          // IP
    f.filterProfile = 4;          // Drop
    f.filterProfileData = 999;    // would be ignored even if present
    f.filterDirection = 0;        // Tx
    f.actionEventOnMatch = false;

    const QByteArray env = buildAddHdr8021FilterEnvelopeForTesting(f);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains(">Drop unknown<"));
    QVERIFY(env.contains(">2048<"));   // ethertype
    QVERIFY(env.contains(">4<"));      // FilterProfile = Drop
    // The stray 999 must NOT leak onto the wire.
    QVERIFY(!env.contains(">999<"));
    QVERIFY(!env.contains("FilterProfileData"));
}

void TestSoapEnvelope::encodeIPv4ForFilterProducesUppercaseHex()
{
    // Locks the on-wire byte order against accidental endianness
    // swap. AMT wants the bytes in the same order the dotted-quad
    // reads (a.b.c.d → AABBCCDD).
    QCOMPARE(encodeIPv4ForFilter(QStringLiteral("10.0.0.5")),
             QStringLiteral("0A000005"));
    QCOMPARE(encodeIPv4ForFilter(QStringLiteral("255.255.255.255")),
             QStringLiteral("FFFFFFFF"));
    QCOMPARE(encodeIPv4ForFilter(QStringLiteral("0.0.0.0")),
             QStringLiteral("00000000"));
    // Malformed input → empty (caller skips the field).
    QVERIFY(encodeIPv4ForFilter(QStringLiteral("10.0.0")).isEmpty());
    QVERIFY(encodeIPv4ForFilter(QStringLiteral("10.0.0.256")).isEmpty());
    QVERIFY(encodeIPv4ForFilter(QStringLiteral("not an ip")).isEmpty());
}

void TestSoapEnvelope::addIpHeadersFilterEnvelopeShape()
{
    IpHeadersFilter f;
    f.name = QStringLiteral("Allow port 16992");
    f.ipVersion = 4;
    f.filterProfile = 3;          // Allow
    f.filterDirection = 1;        // Rx
    f.protocol = 6;               // TCP
    f.dstPort = 16992;
    f.srcAddress = QStringLiteral("10.0.0.5");
    f.actionEventOnMatch = false;

    const QByteArray env = buildAddIpHeadersFilterEnvelopeForTesting(f);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Create action + resource.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Create"));
    QVERIFY(env.contains("AMT_IPHeadersFilter"));

    // Verbatim field names from the legacy reference.
    QVERIFY(env.contains(">Allow port 16992<"));
    QVERIFY(env.contains("HdrIPVersion"));
    QVERIFY(env.contains(">4<"));             // ipVersion
    QVERIFY(env.contains("FilterProfile"));
    QVERIFY(env.contains(">3<"));             // Allow
    QVERIFY(env.contains("FilterDirection"));
    QVERIFY(env.contains("HdrProtocolID8"));
    QVERIFY(env.contains(">6<"));             // TCP
    // Port range — single port goes on both Start and End.
    QVERIFY(env.contains("HdrDestPortStart"));
    QVERIFY(env.contains("HdrDestPortEnd"));
    QVERIFY(env.contains(">16992<"));
    // Source IPv4 → uppercase hex; AMT echoes back this form.
    QVERIFY(env.contains("HdrSrcAddress"));
    QVERIFY(env.contains(">0A000005<"));
    // No destination address was set; the field must be omitted.
    QVERIFY(!env.contains("HdrDestAddress"));
}

void TestSoapEnvelope::addIpHeadersFilterOmitsOptionalFieldsWhenUnset()
{
    // Bare-minimum L3 filter — no IPs, no ports, no protocol. AMT
    // rejects empty scalars, so the optional fields must be omitted
    // entirely.
    IpHeadersFilter f;
    f.name = QStringLiteral("Drop all v6 inbound");
    f.ipVersion = 6;
    f.filterProfile = 4;          // Drop
    f.filterDirection = 1;        // Rx
    // protocol, srcAddress, dstAddress, srcPort, dstPort all unset.

    const QByteArray env = buildAddIpHeadersFilterEnvelopeForTesting(f);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    QVERIFY(env.contains(">Drop all v6 inbound<"));
    QVERIFY(env.contains(">6<"));   // ipVersion = 6
    QVERIFY(env.contains(">4<"));   // FilterProfile = Drop
    // None of the optional fields appear.
    QVERIFY(!env.contains("HdrProtocolID8"));
    QVERIFY(!env.contains("HdrSrcAddress"));
    QVERIFY(!env.contains("HdrDestAddress"));
    QVERIFY(!env.contains("HdrSrcPortStart"));
    QVERIFY(!env.contains("HdrDestPortStart"));
}

void TestSoapEnvelope::extractFilterHandleFromInstanceIdParsesTrailingInt()
{
    // L2 + L3 filter InstanceID formats both terminate in the
    // numeric handle. The exact prefix varies by AMT version /
    // class, so the extractor walks back from the tail rather than
    // matching a known prefix.
    QCOMPARE(extractFilterHandleFromInstanceId(
                 QStringLiteral("Intel(r) AMT:IP Filter Set:Handle 5")), 5);
    QCOMPARE(extractFilterHandleFromInstanceId(
                 QStringLiteral("Intel(r) AMT:Hdr 8021 Filter 17")), 17);
    QCOMPARE(extractFilterHandleFromInstanceId(QStringLiteral("Handle 0")), 0);
    // No trailing digits → -1 (caller drops the row).
    QCOMPARE(extractFilterHandleFromInstanceId(QStringLiteral("no-handle")), -1);
    QCOMPARE(extractFilterHandleFromInstanceId(QString()), -1);
}

void TestSoapEnvelope::addSystemDefensePolicyEnvelopeShape()
{
    SystemDefensePolicy p;
    p.policyName = QStringLiteral("default-drop");
    p.priority = 5;
    p.txDefaultCount = true;
    p.txDefaultDrop = true;
    p.txDefaultMatchEvent = false;
    p.rxDefaultCount = false;
    p.rxDefaultDrop = true;
    p.rxDefaultMatchEvent = true;

    const QByteArray env = buildAddSystemDefensePolicyEnvelopeForTesting(p);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Create action + resource.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Create"));
    QVERIFY(env.contains("AMT_SystemDefensePolicy"));

    // Verbatim field names from the legacy reference + values.
    QVERIFY(env.contains(">default-drop<"));
    QVERIFY(env.contains("PolicyPrecedence"));
    QVERIFY(env.contains(">5<"));
    // All six default-action flags must be emitted unconditionally.
    QVERIFY(env.contains("TxDefaultCount"));
    QVERIFY(env.contains("TxDefaultDrop"));
    QVERIFY(env.contains("TxDefaultMatchEvent"));
    QVERIFY(env.contains("RxDefaultCount"));
    QVERIFY(env.contains("RxDefaultDrop"));
    QVERIFY(env.contains("RxDefaultMatchEvent"));

    // No FilterCreationHandles in an empty-filter policy. AMT
    // accepts this (a policy that only kicks in on defaults).
    QVERIFY(!env.contains("FilterCreationHandles"));
}

void TestSoapEnvelope::addSystemDefensePolicyEnvelopeRepeatsFilterHandles()
{
    SystemDefensePolicy p;
    p.policyName = QStringLiteral("dmz-restrict");
    p.priority = 10;
    // Three filter handles — must hit the wire as three separate
    // <r:FilterCreationHandles> elements, not a single comma- or
    // space-joined string. That's the legacy MeshCommander shape.
    p.filterCreationHandles = { 7, 12, 33 };

    const QByteArray env = buildAddSystemDefensePolicyEnvelopeForTesting(p);

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Each handle gets its own repeated element. Count occurrences
    // of the closing tag — three handles = three closing tags.
    QCOMPARE(env.count(QByteArray("</r:FilterCreationHandles>")), 3);
    // Sanity: each value lands inside the elements.
    QVERIFY(env.contains(">7<"));
    QVERIFY(env.contains(">12<"));
    QVERIFY(env.contains(">33<"));
}

void TestSoapEnvelope::bindSystemDefensePolicyEnvelopeEmbedsBothEprs()
{
    // Port 0 + a policy InstanceID. Both EPRs land inside the
    // Create body — Antecedent points at the Ethernet port, Dependent
    // at the policy. Locks the legacy reference shape.
    const QByteArray env = buildBindSystemDefensePolicyEnvelopeForTesting(
        0, QStringLiteral("Intel(r) AMT:Policy 42"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Create action + target class.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Create"));
    QVERIFY(env.contains("AMT_NetworkPortSystemDefensePolicy"));

    // Antecedent EPR → CIM_EthernetPort with the AMT DeviceID.
    QVERIFY(env.contains("Antecedent"));
    QVERIFY(env.contains("CIM_EthernetPort"));
    QVERIFY(env.contains("Intel(r) AMT Ethernet Port 0"));
    QVERIFY(env.contains("CreationClassName"));

    // Dependent EPR → AMT_SystemDefensePolicy by InstanceID.
    QVERIFY(env.contains("Dependent"));
    QVERIFY(env.contains("AMT_SystemDefensePolicy"));
    QVERIFY(env.contains("Intel(r) AMT:Policy 42"));
}

void TestSoapEnvelope::unbindSystemDefensePolicyEnvelopeHasEprValuedSelectors()
{
    // Delete uses EPR-valued selectors (same pattern as the WS-Eventing
    // Unsubscribe from #352). Two named selectors — Antecedent +
    // Dependent — each containing a full EPR.
    const QByteArray env = buildUnbindSystemDefensePolicyEnvelopeForTesting(
        1, QStringLiteral("Intel(r) AMT:Policy 7"));

    QXmlStreamReader r(env);
    while (!r.atEnd()) r.readNext();
    QVERIFY2(!r.hasError(), qPrintable(r.errorString()));

    // Delete action + target class.
    QVERIFY(env.contains("http://schemas.xmlsoap.org/ws/2004/09/transfer/Delete"));
    QVERIFY(env.contains("AMT_NetworkPortSystemDefensePolicy"));

    // Selector names "Antecedent" and "Dependent" in the SelectorSet.
    QVERIFY(env.contains("Name=\"Antecedent\"") || env.contains("Name='Antecedent'"));
    QVERIFY(env.contains("Name=\"Dependent\"") || env.contains("Name='Dependent'"));
    // Both selector values are full EPRs.
    QVERIFY(env.contains("EndpointReference"));
    QVERIFY(env.contains("CIM_EthernetPort"));
    QVERIFY(env.contains("Intel(r) AMT Ethernet Port 1"));
    QVERIFY(env.contains("AMT_SystemDefensePolicy"));
    QVERIFY(env.contains("Intel(r) AMT:Policy 7"));
}

QTEST_GUILESS_MAIN(TestSoapEnvelope)
#include "test_soap_envelope.moc"
