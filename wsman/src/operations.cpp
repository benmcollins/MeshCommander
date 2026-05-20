// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include "wsman/soap_envelope.h"
#include "wsman/wsman_client.h"


#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QTimeZone>
#include <QUuid>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <array>
#include <memory>

namespace qumesh::wsman {

namespace {

constexpr char kPowerMgmtResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_AssociatedPowerManagementService";

constexpr char kPowerServiceResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_PowerManagementService";

constexpr char kComputerSystemResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_ComputerSystem";

constexpr char kGeneralSettingsResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_GeneralSettings";

constexpr char kEthernetSettingsResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_EthernetPortSettings";

constexpr char kTimeSyncResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_TimeSynchronizationService";

constexpr char kSystemPowerSchemeResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_SystemPowerScheme";

constexpr char kElementSettingDataResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ElementSettingData";

constexpr char kAgentPresenceCapabilitiesResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AgentPresenceCapabilities";

constexpr char kAgentPresenceWatchdogResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AgentPresenceWatchdog";

constexpr char kAgentPresenceServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AgentPresenceService";

constexpr char kFilterCollectionResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_FilterCollection";

constexpr char kListenerDestinationResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ListenerDestination";

/// The WSManagement-flavoured listener destination class. Subscribe
/// returns one of these as the auto-created handler, and Unsubscribe
/// has to reference it back through an EPR.
constexpr char kListenerDestinationWSManagementResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ListenerDestinationWSManagement";

constexpr char kFilterCollectionSubscriptionResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_FilterCollectionSubscription";

constexpr char kAlarmClockOccurrenceResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_AlarmClockOccurrence";

constexpr char kAlarmClockServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AlarmClockService";

constexpr char kSystemDefensePolicyResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_SystemDefensePolicy";

constexpr char kHdr8021FilterResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_Hdr8021Filter";

constexpr char kIpHeadersFilterResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_IPHeadersFilter";

constexpr char kNetworkFilterResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_NetworkFilter";

constexpr char kActiveFilterStatisticsResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_ActiveFilterStatistics";

constexpr char kBootSettingDataResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_BootSettingData";

constexpr char kBootServiceResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_BootService";

constexpr char kIpsPowerServiceResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/"
    "IPS_PowerManagementService";

constexpr char kBootCapabilitiesResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_BootCapabilities";

constexpr char kCimBootSourceSettingResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_BootSourceSetting";

constexpr char kMessageLogResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_MessageLog";

constexpr char kOptInServiceResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/"
    "IPS_OptInService";

constexpr char kKvmRedirectionSettingDataResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/"
    "IPS_KVMRedirectionSettingData";

constexpr char kSetupAndConfigurationResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_SetupAndConfigurationService";

constexpr char kSoftwareIdentityResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_SoftwareIdentity";

constexpr char kRedirectionServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_RedirectionService";

constexpr char kKvmRedirectionSapResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_KVMRedirectionSAP";

constexpr char kChassisResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Chassis";
constexpr char kCardResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Card";
constexpr char kBiosElementResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_BIOSElement";
constexpr char kSystemPackagingResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_SystemPackaging";
constexpr char kProcessorResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Processor";
constexpr char kChipResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Chip";
constexpr char kPhysicalMemoryResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_PhysicalMemory";
constexpr char kMediaAccessDeviceResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_MediaAccessDevice";
constexpr char kPhysicalPackageResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_PhysicalPackage";
constexpr char kBatteryResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Battery";

constexpr char kSolSessionUsingPortResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_SolSessionUsingPort";
constexpr char kKvmSessionUsingPortResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_KvmSessionUsingPort";
constexpr char kIderSessionUsingPortResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_IderSessionUsingPort";

constexpr char kAuditLogResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuditLog";

constexpr char kAuthorizationServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService";

constexpr char kIpv6PortSettingsResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_IPv6PortSettings";

constexpr char kPublicKeyCertificateResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_PublicKeyCertificate";
constexpr char kPublicPrivateKeyPairResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_PublicPrivateKeyPair";
constexpr char kPublicKeyManagementServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_PublicKeyManagementService";
constexpr char kTlsSettingDataResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_TLSSettingData";
constexpr char kTlsCredentialContextResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_TLSCredentialContext";

constexpr char kEnvironmentDetectionResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_EnvironmentDetectionSettingData";
constexpr char kUserInitiatedConnectionResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_UserInitiatedConnectionService";
constexpr char kRemoteAccessPolicyRuleResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_RemoteAccessPolicyRule";
constexpr char kRemoteAccessPolicyAppliesToMpsResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_RemoteAccessPolicyAppliesToMPS";
constexpr char kManagementPresenceRemoteSapResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_ManagementPresenceRemoteSAP";
constexpr char kRemoteAccessServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_RemoteAccessService";
constexpr char kMpsUsernamePasswordResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_MPSUsernamePassword";
constexpr char kHttpProxyServiceResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_HTTPProxyService";
constexpr char kHttpProxyAccessPointResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_HTTPProxyAccessPoint";

constexpr char kWiFiPortResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_WiFiPort";
constexpr char kWiFiEndpointResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_WiFiEndpoint";
constexpr char kWiFiPortConfigServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_WiFiPortConfigurationService";
constexpr char kWiFiEndpointSettingsResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_WiFiEndpointSettings";
constexpr char kIEEE8021xSettingsResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_IEEE8021xSettings";
constexpr char kWired8021xProfileResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_8021XProfile";

QString newMessageId()
{
    return QStringLiteral("uuid:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

/// Boilerplate: shoot off an envelope, parse the reply, hand off to the
/// caller-supplied `extract` lambda. `extract` receives the parsed body
/// XML and a result struct it should populate (including setting `ok`).
template <typename ResultT, typename Extract>
void runRequest(WsmanClient *client, const QByteArray &envelope, ResultT &&zero,
                Extract &&extract,
                std::function<void(ResultT)> callback)
{
    if (client == nullptr) {
        ResultT r = zero;
        r.error = QStringLiteral("client is null");
        callback(std::move(r));
        return;
    }
    WsmanReply *reply = client->sendEnvelope(envelope);
    QObject::connect(reply, &WsmanReply::finished, client,
                     [reply, zero = std::move(zero), extract = std::forward<Extract>(extract),
                       cb = std::move(callback)]() mutable {
                         ResultT r = zero;
                         const QByteArray body = reply->readAll();
                         const auto err = reply->hasError();
                         const auto errString = reply->errorString();
                         reply->deleteLater();
                         if (err) {
                             r.error = errString;
                             cb(std::move(r));
                             return;
                         }
                         const SoapResponse soap = parseResponse(body);
                         if (soap.isFault()) {
                             r.error = soap.fault;
                             cb(std::move(r));
                             return;
                         }
                         extract(soap.bodyXml, r);
                         cb(std::move(r));
                     });
}

/// Forward declaration so callers earlier in the file (e.g.
/// `getEthernetSettings`) can call into the multi-Enumerate+Pull
/// helper defined further down with the hardware-inventory code.
void enumerateAll(WsmanClient *client, const char *resourceUri,
                  std::function<void(QList<QByteArray>, QString)> onDone);

} // namespace

void identify(WsmanClient *client, std::function<void(IdentifyResult)> callback)
{
    if (client == nullptr) {
        callback(IdentifyResult{false, QStringLiteral("client is null"), {}, {}, {}});
        return;
    }
    const QByteArray env = buildIdentifyEnvelope();
    WsmanReply *reply = client->sendEnvelope(env);

    QObject::connect(reply, &WsmanReply::finished, client,
                     [reply, cb = std::move(callback)]() mutable {
                         IdentifyResult r;
                         const QByteArray body = reply->readAll();
                         const auto err = reply->hasError();
                         const auto errString = reply->errorString();
                         reply->deleteLater();
                         if (err) {
                             r.error = errString;
                             cb(std::move(r));
                             return;
                         }
                         const SoapResponse soap = parseResponse(body);
                         if (soap.isFault()) {
                             r.error = soap.fault;
                             cb(std::move(r));
                             return;
                         }
                         r.protocolVersion =
                             findScalar(soap.bodyXml, QStringLiteral("ProtocolVersion"));
                         r.productVendor =
                             findScalar(soap.bodyXml, QStringLiteral("ProductVendor"));
                         r.productVersion =
                             findScalar(soap.bodyXml, QStringLiteral("ProductVersion"));
                         r.ok = !r.protocolVersion.isEmpty();
                         if (!r.ok && r.error.isEmpty()) {
                             r.error = QStringLiteral("Identify response had no ProtocolVersion");
                         }
                         cb(std::move(r));
                     });
}

void getPowerState(WsmanClient *client, std::function<void(PowerStateResult)> callback)
{
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kPowerMgmtResource), {},
                                            client ? client->endpoint().toString() : QString(),
                                            newMessageId());
    runRequest<PowerStateResult>(client, env, {},
        [](const QByteArray &body, PowerStateResult &r) {
            const QString ps = findScalar(body, QStringLiteral("PowerState"));
            if (ps.isEmpty()) {
                r.error = QStringLiteral("response had no PowerState element");
                return;
            }
            bool conv = false;
            r.powerState = ps.toInt(&conv);
            r.ok = conv;
            if (!conv) r.error = QStringLiteral("PowerState '%1' was not numeric").arg(ps);
        },
        std::move(callback));
}

void getGeneralSettings(WsmanClient *client,
                        std::function<void(GeneralSettingsResult)> callback)
{
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kGeneralSettingsResource),
                                             {},
                                             client ? client->endpoint().toString() : QString(),
                                             newMessageId());
    runRequest<GeneralSettingsResult>(client, env, {},
        [](const QByteArray &body, GeneralSettingsResult &r) {
            r.hostName     = findScalar(body, QStringLiteral("HostName"));
            r.domainName   = findScalar(body, QStringLiteral("DomainName"));
            r.digestRealm  = findScalar(body, QStringLiteral("DigestRealm"));
            r.networkInterfaceEnabled =
                findScalar(body, QStringLiteral("NetworkInterfaceEnabled")) == QStringLiteral("true");
            r.rmcpPingResponseEnabled =
                findScalar(body, QStringLiteral("RmcpPingResponseEnabled")) == QStringLiteral("true");
            // `PowerSource`: 0 = plugged-in / AC, 1 = on battery. Older
            // firmware omits the field entirely — leave it at -1 so the
            // QML can render "(unknown)" instead of "Plugged-in".
            const QString ps = findScalar(body, QStringLiteral("PowerSource"));
            if (!ps.isEmpty()) {
                bool conv = false;
                const int v = ps.toInt(&conv);
                if (conv) r.powerSource = v;
            }
            r.ok = !r.hostName.isEmpty() || !r.domainName.isEmpty()
                   || !r.digestRealm.isEmpty();
            if (!r.ok)
                r.error = QStringLiteral("AMT_GeneralSettings body had no recognised fields");
        },
        std::move(callback));
}

void getComputerSystem(WsmanClient *client,
                       std::function<void(ComputerSystemResult)> callback)
{
    // AMT exposes multiple CIM_ComputerSystem instances (one per
    // logical subsystem). A naked Get returns a SOAP fault → HTTP 400.
    // The AMT firmware itself is selected via Name = "Intel(r) AMT".
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"), QStringLiteral("Intel(r) AMT"));
    selectors.insert(QStringLiteral("CreationClassName"),
                     QStringLiteral("CIM_ComputerSystem"));
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kComputerSystemResource),
                                             selectors,
                                             client ? client->endpoint().toString() : QString(),
                                             newMessageId());
    runRequest<ComputerSystemResult>(client, env, {},
        [](const QByteArray &body, ComputerSystemResult &r) {
            r.name              = findScalar(body, QStringLiteral("Name"));
            r.elementName       = findScalar(body, QStringLiteral("ElementName"));
            r.creationClassName = findScalar(body, QStringLiteral("CreationClassName"));
            // AMT's Name on CIM_ComputerSystem is the UUID; expose both
            // names so the QML side can pick the friendlier one.
            r.systemUuid = r.name;
            r.ok = !r.name.isEmpty() || !r.elementName.isEmpty();
            if (!r.ok)
                r.error = QStringLiteral("CIM_ComputerSystem body had no Name/ElementName");
        },
        std::move(callback));
}

QString linkPolicyLabel(int code)
{
    switch (code) {
    case 1:   return QStringLiteral("S0/AC");
    case 14:  return QStringLiteral("Sx/AC");
    case 16:  return QStringLiteral("S0/DC");
    case 224: return QStringLiteral("Sx/DC");
    default:  return QStringLiteral("Code %1").arg(code);
    }
}

namespace {

/// Parse one `AMT_EthernetPortSettings` item-XML into a struct. The
/// item is the `<g:CIM_EthernetPort>…</g:CIM_EthernetPort>`-style
/// inner XML returned by `parsePullResponse`.
EthernetInterface parseEthernetItem(const QByteArray &item)
{
    EthernetInterface e;
    e.instanceId     = findScalar(item, QStringLiteral("InstanceID"));
    e.macAddress     = findScalar(item, QStringLiteral("MACAddress"));
    e.dhcpEnabled    = findScalar(item, QStringLiteral("DHCPEnabled")) == QStringLiteral("true");
    e.ipSyncEnabled  = findScalar(item, QStringLiteral("IpSyncEnabled")) == QStringLiteral("true");
    e.ipAddress      = findScalar(item, QStringLiteral("IPAddress"));
    e.subnetMask     = findScalar(item, QStringLiteral("SubnetMask"));
    e.defaultGateway = findScalar(item, QStringLiteral("DefaultGateway"));
    e.primaryDns     = findScalar(item, QStringLiteral("PrimaryDNS"));
    e.secondaryDns   = findScalar(item, QStringLiteral("SecondaryDNS"));

    // LinkPolicy is repeated — `<g:LinkPolicy>1</g:LinkPolicy>
    // <g:LinkPolicy>14</g:LinkPolicy>…`. Walk via QXmlStreamReader.
    QXmlStreamReader r(item);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("LinkPolicy")) {
            bool conv = false;
            const int v = r.readElementText().toInt(&conv);
            if (conv) e.linkPolicy.append(v);
        }
    }
    return e;
}

IPv6PortSettings parseIpv6Item(const QByteArray &item)
{
    IPv6PortSettings s;
    s.present = true;
    s.instanceId    = findScalar(item, QStringLiteral("InstanceID"));
    s.defaultRouter = findScalar(item, QStringLiteral("CurrentDefaultRouter"));
    s.primaryDns    = findScalar(item, QStringLiteral("CurrentPrimaryDNS"));
    s.secondaryDns  = findScalar(item, QStringLiteral("CurrentSecondaryDNS"));

    // `CurrentAddressInfo` is a `<g:CurrentAddressInfo>addr,prefix,…
    // </g:CurrentAddressInfo>` per address — repeated when there are
    // several. Address-only is what the UI wants.
    QXmlStreamReader r(item);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()
            && r.name() == QStringLiteral("CurrentAddressInfo")) {
            const QString tok = r.readElementText();
            if (tok.isEmpty()) continue;
            const int comma = tok.indexOf(QLatin1Char(','));
            s.addresses.append(comma < 0 ? tok : tok.left(comma));
        }
    }
    return s;
}

/// Pull the trailing integer off an InstanceID like "Intel(r) AMT
/// Ethernet Port Settings 0" or "Intel(r) AMT IPv6 Port Settings 0".
/// Returns -1 if none.
int instanceIdOrdinal(const QString &id)
{
    int i = id.size() - 1;
    while (i >= 0 && id[i].isDigit()) --i;
    if (i == id.size() - 1) return -1;
    bool conv = false;
    const int v = id.mid(i + 1).toInt(&conv);
    return conv ? v : -1;
}

void populateBackwardCompatFields(EthernetSettingsResult &r)
{
    if (r.interfaces.isEmpty()) return;
    const auto &i0 = r.interfaces.first();
    r.macAddress     = i0.macAddress;
    r.dhcpEnabled    = i0.dhcpEnabled;
    r.ipv4Enabled    = i0.ipSyncEnabled || !i0.ipAddress.isEmpty();
    r.ipAddress      = i0.ipAddress;
    r.subnetMask     = i0.subnetMask;
    r.defaultGateway = i0.defaultGateway;
    r.primaryDns     = i0.primaryDns;
    r.secondaryDns   = i0.secondaryDns;
}

} // namespace

void getEthernetSettings(WsmanClient *client,
                         std::function<void(EthernetSettingsResult)> callback)
{
    // Two parallel enumerations (Ethernet + IPv6) merged by ordinal.
    struct Acc {
        QList<QByteArray> ethItems;
        QList<QByteArray> ipv6Items;
        bool ethDone = false;
        bool ipv6Done = false;
        QString error;
        std::function<void(EthernetSettingsResult)> cb;
        bool fired = false;
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    if (client == nullptr) {
        EthernetSettingsResult r;
        r.error = QStringLiteral("client is null");
        acc->cb(std::move(r));
        return;
    }

    auto maybeFire = [acc]() {
        if (acc->fired) return;
        if (!acc->ethDone || !acc->ipv6Done) return;
        acc->fired = true;
        EthernetSettingsResult r;

        // Parse Ethernet items in order.
        for (const QByteArray &item : acc->ethItems)
            r.interfaces.append(parseEthernetItem(item));

        // Parse IPv6 and attach by ordinal — the InstanceID suffix
        // ("…Port Settings 0/1/…") matches between the two classes.
        QHash<int, IPv6PortSettings> ipv6ByOrdinal;
        for (const QByteArray &item : acc->ipv6Items) {
            IPv6PortSettings s = parseIpv6Item(item);
            const int n = instanceIdOrdinal(s.instanceId);
            if (n >= 0) ipv6ByOrdinal.insert(n, s);
        }
        for (auto &iface : r.interfaces) {
            const int n = instanceIdOrdinal(iface.instanceId);
            if (n >= 0 && ipv6ByOrdinal.contains(n))
                iface.ipv6 = ipv6ByOrdinal.value(n);
        }

        populateBackwardCompatFields(r);
        // ok if we got at least one interface; old single-NIC contract
        // required MACAddress.
        r.ok = !r.interfaces.isEmpty() && !r.interfaces.first().macAddress.isEmpty();
        if (!r.ok && r.error.isEmpty())
            r.error = acc->error.isEmpty()
                ? QStringLiteral("AMT_EthernetPortSettings returned no interfaces")
                : acc->error;
        acc->cb(std::move(r));
    };

    enumerateAll(client, kEthernetSettingsResource,
        [acc, maybeFire](QList<QByteArray> items, QString error) mutable {
            acc->ethItems = std::move(items);
            if (!error.isEmpty() && acc->error.isEmpty()) acc->error = error;
            acc->ethDone = true;
            maybeFire();
        });

    // IPv6 is AMT 6+ — older firmware will return a fault. Treat that
    // as "no IPv6 available" rather than a top-level failure.
    enumerateAll(client, kIpv6PortSettingsResource,
        [acc, maybeFire](QList<QByteArray> items, QString /*error*/) mutable {
            acc->ipv6Items = std::move(items);
            acc->ipv6Done = true;
            maybeFire();
        });
}

namespace {

void appendU16Le(QByteArray &b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

void appendU32Le(QByteArray &b, quint32 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}

/// Match the legacy `makeUefiBootParam(type, data, len, vendorid)`:
///   * 2 bytes LE vendor (default Intel 0x8086)
///   * 2 bytes LE type
///   * 4 bytes LE length
///   * `data.size()` bytes payload
void appendTlv(QByteArray &b, quint16 vendor, quint16 type, const QByteArray &data)
{
    appendU16Le(b, vendor);
    appendU16Le(b, type);
    appendU32Le(b, static_cast<quint32>(data.size()));
    b.append(data);
}

QByteArray u32LeBytes(quint32 v)
{
    QByteArray b;
    appendU32Le(b, v);
    return b;
}

} // namespace

QByteArray buildHttpsBootUrlTlv(const QString &url, int *tlvCount)
{
    constexpr quint16 kVendorIntel = 0x8086;
    int count = 0;
    QByteArray blob;
    appendTlv(blob, kVendorIntel, /*type*/ 1, url.toUtf8());
    ++count;
    QByteArray syncOne; syncOne.append(char(0x01));
    appendTlv(blob, kVendorIntel, /*type*/ 20, syncOne);
    ++count;
    QByteArray timeoutZero(2, '\0');
    appendTlv(blob, kVendorIntel, /*type*/ 30, timeoutZero);
    ++count;
    if (tlvCount != nullptr) *tlvCount = count;
    return blob;
}

QByteArray buildOcrPbaBootTlv(const QString &bootString, int *tlvCount)
{
    constexpr quint16 kVendorIntel = 0x8086;
    int count = 0;
    QByteArray blob;

    // Type 2: the firmware-registered BootString for the chosen PBA /
    // WinRE row. Type 3: its byte length as a little-endian u16.
    const QByteArray bs = bootString.toUtf8();
    appendTlv(blob, kVendorIntel,
              static_cast<quint16>(OcrTlvType::EfiFileDevicePath), bs);
    ++count;

    QByteArray lenBytes(2, '\0');
    const quint16 len16 = static_cast<quint16>(bs.size());
    lenBytes[0] = char(len16 & 0xFF);
    lenBytes[1] = char((len16 >> 8) & 0xFF);
    appendTlv(blob, kVendorIntel,
              static_cast<quint16>(OcrTlvType::EfiDevicePathLen), lenBytes);
    ++count;

    if (tlvCount != nullptr) *tlvCount = count;
    return blob;
}

namespace {

// Map "sha256" / "sha384" / "sha512" to the matching OCR boot-image
// TLV type. The HTTPS-pinned builder uses this twice — once for the
// image hash, once for the server-cert hash (which is a different
// type number but the same algorithm dispatch).
quint16 ocrImageHashType(const QString &alg)
{
    const QString a = alg.toLower();
    if (a == QStringLiteral("sha256"))
        return static_cast<quint16>(OcrTlvType::BootImageHashSha256);
    if (a == QStringLiteral("sha384"))
        return static_cast<quint16>(OcrTlvType::BootImageHashSha384);
    if (a == QStringLiteral("sha512"))
        return static_cast<quint16>(OcrTlvType::BootImageHashSha512);
    return 0;
}

quint16 ocrServerCertHashType(const QString &alg)
{
    const QString a = alg.toLower();
    if (a == QStringLiteral("sha256"))
        return static_cast<quint16>(OcrTlvType::HttpsServerCertHashSha256);
    if (a == QStringLiteral("sha384"))
        return static_cast<quint16>(OcrTlvType::HttpsServerCertHashSha384);
    if (a == QStringLiteral("sha512"))
        return static_cast<quint16>(OcrTlvType::HttpsServerCertHashSha512);
    return 0;
}

} // namespace

QByteArray buildOcrHttpsBootPinnedTlv(
    const QString &url,
    const QString &hashAlg,
    const QByteArray &imageHash,
    const QString &pinnedServerCertHashAlg,
    const QByteArray &pinnedServerCertHash,
    const QString &username,
    const QString &password,
    int *tlvCount)
{
    constexpr quint16 kVendorIntel = 0x8086;
    int count = 0;
    QByteArray blob;

    appendTlv(blob, kVendorIntel,
              static_cast<quint16>(OcrTlvType::EfiNetworkDevicePath),
              url.toUtf8());
    ++count;

    // Boot image hash pin (optional). Maps the alg name onto type
    // 4/5/6. Skip if alg is empty or unknown.
    if (!hashAlg.isEmpty() && !imageHash.isEmpty()) {
        const quint16 t = ocrImageHashType(hashAlg);
        if (t != 0) {
            appendTlv(blob, kVendorIntel, t, imageHash);
            ++count;
        }
    }

    // Sync Root CA flag. Always set to 1 — this matches the existing
    // HTTPS-boot path and Intel's MPS reference, and lets AMT trust
    // the server cert via its existing chain when no explicit pin is
    // supplied.
    QByteArray syncOne; syncOne.append(char(0x01));
    appendTlv(blob, kVendorIntel,
              static_cast<quint16>(OcrTlvType::HttpsCertSyncRootCa), syncOne);
    ++count;

    // Server cert hash pin (optional).
    if (!pinnedServerCertHashAlg.isEmpty()
        && !pinnedServerCertHash.isEmpty()) {
        const quint16 t = ocrServerCertHashType(pinnedServerCertHashAlg);
        if (t != 0) {
            appendTlv(blob, kVendorIntel, t, pinnedServerCertHash);
            ++count;
        }
    }

    // HTTPS basic-auth credentials (optional).
    if (!username.isEmpty()) {
        appendTlv(blob, kVendorIntel,
                  static_cast<quint16>(OcrTlvType::HttpsUserName),
                  username.toUtf8());
        ++count;
    }
    if (!password.isEmpty()) {
        appendTlv(blob, kVendorIntel,
                  static_cast<quint16>(OcrTlvType::HttpsPassword),
                  password.toUtf8());
        ++count;
    }

    if (tlvCount != nullptr) *tlvCount = count;
    return blob;
}

QByteArray buildPlatformEraseTlv(quint32 flags, const QString &psid,
                                  const QString &ssdPassword, int *tlvCount)
{
    constexpr quint16 kVendorIntel = 0x8086;
    int count = 0;
    QByteArray blob;

    // Header: the bitmask of which sub-actions to perform.
    appendTlv(blob, kVendorIntel, /*type*/ 1, u32LeBytes(flags));
    ++count;

    // Pyrite Revert (bit 1) → PSID bytes.
    if ((flags & (1u << 1)) && !psid.isEmpty()) {
        appendTlv(blob, kVendorIntel, /*type*/ 10, psid.toUtf8());
        ++count;
    }
    // Secure Erase All SSDs (bit 2) → admin password bytes.
    if ((flags & (1u << 2)) && !ssdPassword.isEmpty()) {
        appendTlv(blob, kVendorIntel, /*type*/ 20, ssdPassword.toUtf8());
        ++count;
    }
    // OEM Custom (bit 16) intentionally not exposed — needs hex blob
    // input + non-Intel vendor id 0x000B that the QML side doesn't
    // surface yet.

    if (tlvCount != nullptr) *tlvCount = count;
    return blob;
}

void getBootCapabilities(WsmanClient *client,
                         std::function<void(BootCapabilitiesResult)> callback)
{
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kBootCapabilitiesResource),
                                             {},
                                             client ? client->endpoint().toString() : QString(),
                                             newMessageId());
    runRequest<BootCapabilitiesResult>(client, env, {},
        [](const QByteArray &body, BootCapabilitiesResult &r) {
            const auto truthy = [&](const QString &name) {
                return findScalar(body, name) == QStringLiteral("true");
            };
            r.ider                   = truthy(QStringLiteral("IDER"));
            r.sol                    = truthy(QStringLiteral("SOL"));
            r.biosReflash            = truthy(QStringLiteral("BIOSReflash"));
            r.biosSetup              = truthy(QStringLiteral("BIOSSetup"));
            r.biosPause              = truthy(QStringLiteral("BIOSPause"));
            r.forcePxeBoot           = truthy(QStringLiteral("ForcePXEBoot"));
            r.forceHddBoot           = truthy(QStringLiteral("ForceHDDBoot"));
            r.forceCdOrDvdBoot       = truthy(QStringLiteral("ForceCDorDVDBoot"));
            r.verbosityScreenBlank   = truthy(QStringLiteral("VerbosityScreenBlank"));
            r.powerButtonLock        = truthy(QStringLiteral("PowerButtonLock"));
            r.resetButtonLock        = truthy(QStringLiteral("ResetButtonLock"));
            r.keyboardLock           = truthy(QStringLiteral("KeyboardLock"));
            r.sleepButtonLock        = truthy(QStringLiteral("SleepButtonLock"));
            r.userPasswordBypass     = truthy(QStringLiteral("UserPasswordBypass"));
            r.forcedProgressEvents   = truthy(QStringLiteral("ForcedProgressEvents"));
            r.verbosityVerbose       = truthy(QStringLiteral("VerbosityVerbose"));
            r.verbosityQuiet         = truthy(QStringLiteral("VerbosityQuiet"));
            r.configurationDataReset = truthy(QStringLiteral("ConfigurationDataReset"));
            r.biosSecureBoot         = truthy(QStringLiteral("BIOSSecureBoot"));
            r.secureErase            = truthy(QStringLiteral("SecureErase"));
            r.forceWinReBoot         = truthy(QStringLiteral("ForceWinREBoot"));
            r.forceUefiLocalPbaBoot  = truthy(QStringLiteral("ForceUEFILocalPBABoot"));
            r.forceUefiHttpsBoot     = truthy(QStringLiteral("ForceUEFIHTTPSBoot"));
            r.amtSecureBootControl   = truthy(QStringLiteral("AMTSecureBootControl"));
            // PlatformErase is a bitmask in newer firmware. Decode as
            // u32 when numeric; treat "true" as "all bits supported"
            // so older firmware still gets the menu surface.
            const QString pe = findScalar(body, QStringLiteral("PlatformErase"));
            if (pe == QStringLiteral("true")) {
                r.platformErase = true;
                r.platformEraseMask = ~quint32{0};
            } else {
                bool conv = false;
                const quint32 mask = pe.toUInt(&conv);
                r.platformEraseMask = conv ? mask : 0;
                r.platformErase = r.platformEraseMask != 0;
            }
            r.ok = true;
        },
        std::move(callback));
}

void enumerateBootSourceSettings(WsmanClient *client,
                                  std::function<void(BootSourceSettingsResult)> callback)
{
    auto cb = std::make_shared<std::function<void(BootSourceSettingsResult)>>(
        std::move(callback));
    if (client == nullptr) {
        BootSourceSettingsResult r;
        r.error = QStringLiteral("client is null");
        (*cb)(std::move(r));
        return;
    }

    enumerateAll(client, kCimBootSourceSettingResource,
        [cb](QList<QByteArray> items, QString error) {
            BootSourceSettingsResult res;
            if (!error.isEmpty()) {
                res.error = std::move(error);
                (*cb)(std::move(res));
                return;
            }
            res.sources.reserve(items.size());
            for (const QByteArray &item : items) {
                BootSourceSetting s;
                // Pull the scalars by local name. `findScalar` walks
                // any nested element, but the boot-source rows are flat
                // so this is a direct lookup.
                s.instanceId           = findScalar(item, QStringLiteral("InstanceID"));
                s.elementName          = findScalar(item, QStringLiteral("ElementName"));
                s.bootString           = findScalar(item, QStringLiteral("BootString"));
                s.biosBootString       = findScalar(item, QStringLiteral("BIOSBootString"));
                s.structuredBootString = findScalar(item, QStringLiteral("StructuredBootString"));
                if (!s.instanceId.isEmpty()) res.sources.append(s);
            }
            res.ok = true;
            (*cb)(std::move(res));
        });
}

void getSetupAndConfiguration(WsmanClient *client,
                              std::function<void(SetupAndConfigResult)> callback)
{
    const QByteArray env = buildGetEnvelope(
        QString::fromLatin1(kSetupAndConfigurationResource), {},
        client ? client->endpoint().toString() : QString(),
        newMessageId());
    runRequest<SetupAndConfigResult>(client, env, {},
        [](const QByteArray &body, SetupAndConfigResult &r) {
            const QString ps = findScalar(body, QStringLiteral("ProvisioningState"));
            const QString pm = findScalar(body, QStringLiteral("ProvisioningMode"));
            bool conv = false;
            if (!ps.isEmpty()) {
                const int v = ps.toInt(&conv);
                if (conv) r.provisioningState = v;
            }
            conv = false;
            if (!pm.isEmpty()) {
                const int v = pm.toInt(&conv);
                if (conv) r.provisioningMode = v;
            }
            r.ok = r.provisioningState >= 0;
            if (!r.ok)
                r.error = QStringLiteral(
                    "AMT_SetupAndConfigurationService body had no ProvisioningState");
        },
        std::move(callback));
}

void getMeVersion(WsmanClient *client,
                  std::function<void(MeVersionResult)> callback)
{
    // CIM_SoftwareIdentity is a collection — `Enumerate` + `Pull` the
    // whole thing. The legacy code only kept the row whose InstanceID
    // is "AMT"; we keep all of them so the UI can fingerprint the
    // firmware (SKU / build / recovery / vendor / flash). The InstanceID
    // strings vary by firmware version — match by case-insensitive
    // substring rather than exact equality so reduced-SKU ISM, future
    // AMT releases, and vendor forks all populate the same UI rows.
    struct Acc {
        MeVersionResult r;
        bool foundAmt = false;
    };
    auto acc = std::make_shared<Acc>();
    auto onDone = std::make_shared<std::function<void(QString)>>();
    auto cb = std::make_shared<std::function<void(MeVersionResult)>>(std::move(callback));

    *onDone = [acc, cb](QString error) {
        acc->r.ok = error.isEmpty() && acc->foundAmt;
        if (!acc->r.ok && error.isEmpty())
            error = QStringLiteral(
                "CIM_SoftwareIdentity enumeration had no InstanceID='AMT'");
        acc->r.error = std::move(error);
        (*cb)(std::move(acc->r));
    };

    auto pullStep = std::make_shared<std::function<void(const QString &)>>();
    *pullStep = [client, acc, pullStep, onDone](const QString &context) mutable {
        const QByteArray env = buildPullEnvelope(
            QString::fromLatin1(kSoftwareIdentityResource),
            context, 64, client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc, pullStep, onDone]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                if (err) { (*onDone)(errString); return; }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) { (*onDone)(soap.fault); return; }
                const PullChunk chunk = parsePullResponse(soap.bodyXml);
                for (const QByteArray &item : chunk.items) {
                    const QString id = findScalar(item, QStringLiteral("InstanceID"));
                    const QString ver = findScalar(item, QStringLiteral("VersionString"));
                    if (id.isEmpty()) continue;
                    acc->r.identities.append(qMakePair(id, ver));

                    const QString idLower = id.toLower();
                    if (id == QStringLiteral("AMT")) {
                        acc->r.versionString = ver;
                        acc->foundAmt = true;
                    } else if (idLower.contains(QStringLiteral("recovery"))) {
                        acc->r.recoveryVersion = ver;
                    } else if (idLower.contains(QStringLiteral("build"))) {
                        acc->r.buildNumber = ver;
                    } else if (idLower == QStringLiteral("sku")) {
                        acc->r.sku = ver;
                    } else if (idLower.contains(QStringLiteral("vendor"))) {
                        acc->r.vendorId = ver;
                    } else if (idLower == QStringLiteral("flash")) {
                        acc->r.flash = ver;
                    }
                }
                if (chunk.endOfSequence || chunk.enumerationContext.isEmpty()) {
                    (*onDone)({});
                    return;
                }
                (*pullStep)(chunk.enumerationContext);
            });
    };

    if (client == nullptr) { (*onDone)(QStringLiteral("client is null")); return; }
    const QByteArray env = buildEnumerateEnvelope(
        QString::fromLatin1(kSoftwareIdentityResource),
        client->endpoint().toString(), newMessageId());
    WsmanReply *reply = client->sendEnvelope(env);
    QObject::connect(reply, &WsmanReply::finished, client,
        [reply, pullStep, onDone]() mutable {
            const QByteArray body = reply->readAll();
            const auto err = reply->hasError();
            const auto errString = reply->errorString();
            reply->deleteLater();
            if (err) { (*onDone)(errString); return; }
            const SoapResponse soap = parseResponse(body);
            if (soap.isFault()) { (*onDone)(soap.fault); return; }
            const QString ctx = parseEnumerateContext(soap.bodyXml);
            if (ctx.isEmpty()) { (*onDone)({}); return; }
            (*pullStep)(ctx);
        });
}

void getRedirectionStatus(WsmanClient *client,
                          std::function<void(RedirectionStatusResult)> callback)
{
    // Two independent Gets — AMT_RedirectionService (always present)
    // and CIM_KVMRedirectionSAP (AMT > 5 only). Merge into a single
    // result like `getOptInStatus` does for OptIn+KVMSettings.
    struct Acc {
        RedirectionStatusResult r;
        bool gotRedir = false;
        bool gotKvm   = false;
        std::function<void(RedirectionStatusResult)> cb;
        bool fired = false;
        void maybeFire() {
            if (fired) return;
            if (!gotRedir || !gotKvm) return;
            fired = true;
            r.ok = r.error.isEmpty();
            cb(std::move(r));
        }
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    if (client == nullptr) {
        acc->r.error = QStringLiteral("client is null");
        acc->gotRedir = acc->gotKvm = true;
        acc->maybeFire();
        return;
    }

    // --- AMT_RedirectionService -----------------------------------
    {
        const QByteArray env = buildGetEnvelope(
            QString::fromLatin1(kRedirectionServiceResource), {},
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                acc->gotRedir = true;
                if (err) {
                    if (acc->r.error.isEmpty()) acc->r.error = errString;
                    acc->maybeFire();
                    return;
                }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) {
                    if (acc->r.error.isEmpty()) acc->r.error = soap.fault;
                    acc->maybeFire();
                    return;
                }
                acc->r.redirectionListenerEnabled =
                    findScalar(soap.bodyXml, QStringLiteral("ListenerEnabled"))
                        == QStringLiteral("true");
                bool conv = false;
                const int state = findScalar(soap.bodyXml,
                                              QStringLiteral("EnabledState")).toInt(&conv);
                if (conv) {
                    acc->r.solEnabled  = (state & 2) != 0;
                    acc->r.iderEnabled = (state & 1) != 0;
                }
                acc->maybeFire();
            });
    }

    // --- CIM_KVMRedirectionSAP ------------------------------------
    {
        const QByteArray env = buildGetEnvelope(
            QString::fromLatin1(kKvmRedirectionSapResource), {},
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                reply->deleteLater();
                acc->gotKvm = true;
                if (err) {
                    // Soft failure — AMT 5 and earlier don't expose
                    // this class. Leave kvmAvailable=false and don't
                    // surface as a top-level error.
                    acc->maybeFire();
                    return;
                }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) {
                    // Same soft-failure rationale.
                    acc->maybeFire();
                    return;
                }
                acc->r.kvmAvailable = true;
                bool conv = false;
                const int state = findScalar(soap.bodyXml,
                                              QStringLiteral("EnabledState")).toInt(&conv);
                if (conv) acc->r.kvmEnabled = (state == 2 || state == 6);
                acc->maybeFire();
            });
    }
}

namespace {

/// Walk a single `CIM_ElementSettingData` item body and, if it points
/// at an `AMT_SystemPowerScheme` with `IsCurrent` set, return the
/// `InstanceID` selector value. Empty string otherwise.
QString extractCurrentPowerSchemeInstanceId(const QByteArray &itemXml)
{
    QXmlStreamReader r(itemXml);
    bool sawPowerSchemeResourceUri = false;
    bool isCurrent = false;
    QString settingDataInstanceId;
    QString pendingSelectorName;
    bool inSettingDataEpr = false;

    while (!r.atEnd() && !r.hasError()) {
        r.readNext();
        if (r.tokenType() == QXmlStreamReader::StartElement) {
            const auto name = r.name();
            if (name == QStringLiteral("IsCurrent")) {
                const QString v = r.readElementText().trimmed();
                isCurrent = (v == QStringLiteral("1") || v == QStringLiteral("true"));
            } else if (name == QStringLiteral("SettingData")) {
                inSettingDataEpr = true;
            } else if (inSettingDataEpr && name == QStringLiteral("ResourceURI")) {
                const QString v = r.readElementText().trimmed();
                if (v.endsWith(QStringLiteral("AMT_SystemPowerScheme")))
                    sawPowerSchemeResourceUri = true;
            } else if (inSettingDataEpr && name == QStringLiteral("Selector")) {
                pendingSelectorName.clear();
                const auto attrs = r.attributes();
                if (attrs.hasAttribute(QStringLiteral("Name")))
                    pendingSelectorName = attrs.value(QStringLiteral("Name")).toString();
                const QString v = r.readElementText().trimmed();
                if (pendingSelectorName == QStringLiteral("InstanceID"))
                    settingDataInstanceId = v;
            }
        } else if (r.tokenType() == QXmlStreamReader::EndElement
                   && r.name() == QStringLiteral("SettingData")) {
            inSettingDataEpr = false;
        }
    }

    if (isCurrent && sawPowerSchemeResourceUri)
        return settingDataInstanceId;
    return {};
}

} // namespace

void getPowerSchemes(WsmanClient *client,
                     std::function<void(PowerSchemesResult)> callback)
{
    // Two independent enumerates: the SystemPowerScheme list and the
    // ElementSettingData associations that link them to the active one.
    // Fire them in parallel and merge in the join callback — the WSMAN
    // client serialises HTTP for us when an SSH tunnel is active.
    struct Acc {
        PowerSchemesResult r;
        bool gotSchemes = false;
        bool gotAssoc = false;
        QString schemesErr;
        QString assocErr;
        std::function<void(PowerSchemesResult)> cb;
        void maybeFire() {
            if (!gotSchemes || !gotAssoc) return;
            r.ok = !r.schemes.isEmpty()
                && schemesErr.isEmpty();
            // Association is best-effort — older firmware may not
            // surface the IsCurrent link at all. Leave currentInstanceId
            // empty in that case rather than failing the whole call.
            if (!r.ok && r.error.isEmpty())
                r.error = !schemesErr.isEmpty()
                          ? schemesErr
                          : QStringLiteral("AMT_SystemPowerScheme enumeration returned no rows");
            cb(std::move(r));
        }
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    enumerateAll(client, kSystemPowerSchemeResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotSchemes = true;
            acc->schemesErr = err;
            for (const QByteArray &it : items) {
                PowerScheme p;
                p.instanceId  = findScalar(it, QStringLiteral("InstanceID"));
                p.schemeGuid  = findScalar(it, QStringLiteral("SchemeGUID"));
                p.description = findScalar(it, QStringLiteral("Description"));
                if (!p.instanceId.isEmpty())
                    acc->r.schemes.append(std::move(p));
            }
            acc->maybeFire();
        });

    enumerateAll(client, kElementSettingDataResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotAssoc = true;
            acc->assocErr = err;
            for (const QByteArray &it : items) {
                const QString cur = extractCurrentPowerSchemeInstanceId(it);
                if (!cur.isEmpty()) {
                    acc->r.currentInstanceId = cur;
                    break;
                }
            }
            acc->maybeFire();
        });
}

namespace {

/// `MonitoredEntity` enum → human-readable label. Values come from
/// the DMTF spec for `AMT_AgentPresenceWatchdog`.
const char *monitoredEntityName(int code)
{
    switch (code) {
    case 0:  return "Unknown";
    case 1:  return "Other";
    case 2:  return "Operating System";
    case 3:  return "OS Boot Process";
    case 4:  return "OS Shutdown Process";
    case 5:  return "Firmware Boot Process";
    case 6:  return "BIOS Boot Process";
    case 7:  return "Application";
    case 8:  return "Service Processor";
    default: return "Unknown";
    }
}

/// `EnabledState` enum → label (DMTF standard for `CIM_EnabledLogicalElement`).
const char *enabledStateName(int code)
{
    switch (code) {
    case 0:  return "Unknown";
    case 1:  return "Other";
    case 2:  return "Enabled";
    case 3:  return "Disabled";
    case 4:  return "Shutting Down";
    case 5:  return "Not Applicable";
    case 6:  return "Enabled but Offline";
    case 7:  return "In Test";
    case 8:  return "Deferred";
    case 9:  return "Quiesce";
    case 10: return "Starting";
    default: return "Unknown";
    }
}

/// Watchdog `CurrentState` bitmask → label. Values mirror legacy
/// MeshCommander's `WatchdogCurrentStates` table.
const char *watchdogStateName(int code)
{
    switch (code) {
    case 1:  return "Not Started";
    case 2:  return "Stopped";
    case 4:  return "Running";
    case 8:  return "Expired";
    case 16: return "Suspended";
    default: return "Unknown";
    }
}

/// `DeviceID` in `AMT_AgentPresenceWatchdog` is reported as a base-64
/// encoded sequence of 16 raw GUID bytes. Decode and emit the standard
/// 8-4-4-4-12 GUID string. Returns empty on malformed input.
QString decodeWatchdogDeviceId(const QString &b64)
{
    if (b64.isEmpty()) return {};
    const QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
    if (raw.size() != 16) return {};
    const QByteArray hex = raw.toHex().toLower();
    // 8-4-4-4-12.
    return QString::fromLatin1(hex.constData(),     8) + QLatin1Char('-')
         + QString::fromLatin1(hex.constData() +  8, 4) + QLatin1Char('-')
         + QString::fromLatin1(hex.constData() + 12, 4) + QLatin1Char('-')
         + QString::fromLatin1(hex.constData() + 16, 4) + QLatin1Char('-')
         + QString::fromLatin1(hex.constData() + 20, 12);
}

/// Inverse of `decodeWatchdogDeviceId`: take the standard 8-4-4-4-12
/// GUID string and produce the base-64-of-raw-16-bytes form the
/// firmware expects on `DeviceID`. Returns empty on malformed input.
/// Round-trips with `decodeWatchdogDeviceId` for any well-formed GUID
/// (no endianness swap — the firmware reports and accepts the bytes
/// in the same big-endian-ish hex order).
QString encodeWatchdogDeviceId(const QString &guid)
{
    QString hex;
    hex.reserve(32);
    for (QChar c : guid) {
        if (c == QLatin1Char('-') || c == QLatin1Char('{') || c == QLatin1Char('}'))
            continue;
        if (!c.isLetterOrNumber()) return {};
        hex.append(c.toLower());
    }
    if (hex.size() != 32) return {};
    const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
    if (raw.size() != 16) return {};
    return QString::fromLatin1(raw.toBase64());
}

} // namespace

void getAgentPresence(WsmanClient *client,
                      std::function<void(AgentPresenceResult)> callback)
{
    struct Acc {
        AgentPresenceResult r;
        bool gotCaps = false;
        bool gotWatchdogs = false;
        QString capsErr;
        QString watchdogsErr;
        std::function<void(AgentPresenceResult)> cb;
        void maybeFire() {
            if (!gotCaps || !gotWatchdogs) return;
            // Treat caps as advisory — older firmware may not surface
            // it. The watchdog enumerate is the real indicator.
            r.ok = watchdogsErr.isEmpty();
            if (!r.ok && r.error.isEmpty())
                r.error = watchdogsErr;
            cb(std::move(r));
        }
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    // AMT_AgentPresenceCapabilities — single-instance Get. Soft-fails
    // on firmware that doesn't expose the class.
    const QByteArray capsEnv = buildGetEnvelope(
        QString::fromLatin1(kAgentPresenceCapabilitiesResource), {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    if (client == nullptr) {
        acc->gotCaps = true;
    } else {
        WsmanReply *reply = client->sendEnvelope(capsEnv);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                reply->deleteLater();
                if (!err) {
                    const SoapResponse soap = parseResponse(body);
                    if (!soap.isFault()) {
                        bool conv = false;
                        const int agents = findScalar(soap.bodyXml,
                            QStringLiteral("MaxTotalAgents")).toInt(&conv);
                        if (conv) acc->r.maxTotalAgents = agents;
                        conv = false;
                        const int actions = findScalar(soap.bodyXml,
                            QStringLiteral("MaxTotalActions")).toInt(&conv);
                        if (conv) acc->r.maxTotalActions = actions;
                    }
                }
                acc->gotCaps = true;
                acc->maybeFire();
            });
    }

    // AMT_AgentPresenceWatchdog — enumerate.
    enumerateAll(client, kAgentPresenceWatchdogResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotWatchdogs = true;
            acc->watchdogsErr = err;
            for (const QByteArray &it : items) {
                AgentPresenceWatchdog w;
                w.deviceIdGuid = decodeWatchdogDeviceId(
                    findScalar(it, QStringLiteral("DeviceID")));
                w.description = findScalar(it,
                    QStringLiteral("MonitoredEntityDescription"));
                bool conv = false;
                w.monitoredEntityCode = findScalar(it,
                    QStringLiteral("MonitoredEntity")).toInt(&conv);
                if (!conv) w.monitoredEntityCode = -1;
                w.monitoredEntityLabel = QString::fromLatin1(
                    monitoredEntityName(w.monitoredEntityCode));
                conv = false;
                w.currentStateCode = findScalar(it,
                    QStringLiteral("CurrentState")).toInt(&conv);
                if (!conv) w.currentStateCode = -1;
                w.currentStateLabel = QString::fromLatin1(
                    watchdogStateName(w.currentStateCode));
                conv = false;
                w.enabledStateCode = findScalar(it,
                    QStringLiteral("EnabledState")).toInt(&conv);
                if (!conv) w.enabledStateCode = -1;
                w.enabledStateLabel = QString::fromLatin1(
                    enabledStateName(w.enabledStateCode));
                conv = false;
                const int s = findScalar(it,
                    QStringLiteral("StartupInterval")).toInt(&conv);
                if (conv) w.startupIntervalSec = s;
                conv = false;
                const int t = findScalar(it,
                    QStringLiteral("TimeoutInterval")).toInt(&conv);
                if (conv) w.timeoutIntervalSec = t;
                acc->r.watchdogs.append(std::move(w));
            }
            acc->maybeFire();
        });
}

namespace {

/// Build an `AMT_AgentPresenceService.RegisterAgent` envelope by hand.
/// The method input wraps an embedded `AMT_AgentPresenceWatchdog` in
/// an `AgentTemplate` element. We populate only the fields a Phase B
/// register needs: DeviceID, MonitoredEntityDescription,
/// MonitoredEntity, StartupInterval, TimeoutInterval. State fields
/// are firmware-managed and rejected on the write path.
QByteArray buildRegisterAgentEnvelope(const AgentPresenceWatchdog &w,
                                       const QString &to,
                                       const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter writer(&out);
    writer.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString service = QString::fromLatin1(kAgentPresenceServiceResource);
    const QString watchdog = QString::fromLatin1(kAgentPresenceWatchdogResource);
    const QString action = service + QStringLiteral("/RegisterAgent");

    writer.writeStartDocument();
    writer.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    writer.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    writer.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    writer.writeNamespace(service,                             QStringLiteral("r"));
    writer.writeNamespace(watchdog,                            QStringLiteral("p"));

    writer.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header — keyed at the singleton AgentPresenceService.
    writer.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    writer.writeTextElement(QString::fromLatin1(kNsAddressing),
                             QStringLiteral("Action"), action);
    writer.writeTextElement(QString::fromLatin1(kNsAddressing),
                             QStringLiteral("To"), to);
    writer.writeTextElement(QString::fromLatin1(kNsWsman),
                             QStringLiteral("ResourceURI"), service);
    writer.writeTextElement(QString::fromLatin1(kNsAddressing),
                             QStringLiteral("MessageID"), messageId);
    writer.writeStartElement(QString::fromLatin1(kNsAddressing),
                             QStringLiteral("ReplyTo"));
    writer.writeTextElement(QString::fromLatin1(kNsAddressing),
                             QStringLiteral("Address"),
                             QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    writer.writeEndElement(); // ReplyTo
    writer.writeStartElement(QString::fromLatin1(kNsWsman),
                             QStringLiteral("SelectorSet"));
    for (const auto &kv : std::initializer_list<std::pair<QString, QString>>{
             { QStringLiteral("Name"),                    QStringLiteral("Intel(r) AMT Agent Presence Service") },
             { QStringLiteral("SystemCreationClassName"), QStringLiteral("CIM_ComputerSystem") },
             { QStringLiteral("SystemName"),              QStringLiteral("Intel(r) AMT") },
             { QStringLiteral("CreationClassName"),       QStringLiteral("AMT_AgentPresenceService") },
         }) {
        writer.writeStartElement(QString::fromLatin1(kNsWsman),
                                  QStringLiteral("Selector"));
        writer.writeAttribute(QStringLiteral("Name"), kv.first);
        writer.writeCharacters(kv.second);
        writer.writeEndElement(); // Selector
    }
    writer.writeEndElement(); // SelectorSet
    writer.writeEndElement(); // Header

    // Body
    writer.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    writer.writeStartElement(service, QStringLiteral("RegisterAgent_INPUT"));
    writer.writeStartElement(service, QStringLiteral("AgentTemplate"));

    // Embedded AMT_AgentPresenceWatchdog under the watchdog namespace.
    writer.writeTextElement(watchdog, QStringLiteral("DeviceID"),
                             encodeWatchdogDeviceId(w.deviceIdGuid));
    if (!w.description.isEmpty()) {
        writer.writeTextElement(watchdog, QStringLiteral("MonitoredEntityDescription"),
                                 w.description);
    }
    if (w.monitoredEntityCode >= 0) {
        writer.writeTextElement(watchdog, QStringLiteral("MonitoredEntity"),
                                 QString::number(w.monitoredEntityCode));
    }
    writer.writeTextElement(watchdog, QStringLiteral("StartupInterval"),
                             QString::number(w.startupIntervalSec));
    writer.writeTextElement(watchdog, QStringLiteral("TimeoutInterval"),
                             QString::number(w.timeoutIntervalSec));

    writer.writeEndElement(); // AgentTemplate
    writer.writeEndElement(); // RegisterAgent_INPUT
    writer.writeEndElement(); // Body

    writer.writeEndElement(); // Envelope
    writer.writeEndDocument();
    return out;
}

} // namespace

void registerWatchdogAgent(WsmanClient *client,
                            const AgentPresenceWatchdog &watchdog,
                            std::function<void(InvokeResult)> callback)
{
    if (encodeWatchdogDeviceId(watchdog.deviceIdGuid).isEmpty()) {
        callback({ false, QStringLiteral("RegisterAgent: DeviceID must be a 16-byte GUID"), -1 });
        return;
    }
    if (watchdog.timeoutIntervalSec <= 0) {
        callback({ false, QStringLiteral("RegisterAgent: TimeoutInterval must be > 0"), -1 });
        return;
    }
    const QByteArray env = buildRegisterAgentEnvelope(watchdog,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                // Some AMT versions return an EPR with no ReturnValue
                // on success. Treat that as success too.
                if (body.contains("AgentRef")
                    || body.contains("RegisterAgent_OUTPUT")) {
                    r.ok = true;
                    r.returnValue = 0;
                    return;
                }
                r.error = QStringLiteral("RegisterAgent response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && r.returnValue == 0;
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("RegisterAgent returned %1").arg(rv);
        },
        std::move(callback));
}

QByteArray buildRegisterAgentEnvelopeForTesting(const AgentPresenceWatchdog &watchdog,
                                                  const QString &to,
                                                  const QString &messageId)
{
    return buildRegisterAgentEnvelope(watchdog, to, messageId);
}

void deleteAgentPresenceWatchdog(WsmanClient *client,
                                   const QString &deviceIdGuid,
                                   std::function<void(InvokeResult)> callback)
{
    const QString encoded = encodeWatchdogDeviceId(deviceIdGuid);
    if (encoded.isEmpty()) {
        callback({ false, QStringLiteral("DeleteWatchdog: DeviceID must be a 16-byte GUID"), -1 });
        return;
    }
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("DeviceID"), encoded);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kAgentPresenceWatchdogResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

QString listenerDeliveryModeLabel(int code)
{
    switch (code) {
    case 2: return QStringLiteral("Push");
    case 3: return QStringLiteral("Push with ACK");
    case 4: return QStringLiteral("Events");
    case 5: return QStringLiteral("Pull");
    default: return QStringLiteral("Unknown");
    }
}

namespace {

/// Walk a single `CIM_FilterCollectionSubscription` item and pull the
/// selector value with `Name="<key>"` out of the named EPR. Used to
/// extract Filter.InstanceID and Handler.Name. Returns empty when the
/// EPR or selector is absent.
QString extractEprSelector(const QByteArray &itemXml,
                            const QString &eprElement,
                            const QString &selectorName)
{
    QXmlStreamReader r(itemXml);
    bool inEpr = false;
    QString pendingSelectorName;
    while (!r.atEnd() && !r.hasError()) {
        r.readNext();
        if (r.tokenType() == QXmlStreamReader::StartElement) {
            const auto name = r.name();
            if (name == eprElement) {
                inEpr = true;
            } else if (inEpr && name == QStringLiteral("Selector")) {
                pendingSelectorName.clear();
                const auto attrs = r.attributes();
                if (attrs.hasAttribute(QStringLiteral("Name")))
                    pendingSelectorName = attrs.value(QStringLiteral("Name")).toString();
                const QString v = r.readElementText().trimmed();
                if (pendingSelectorName == selectorName)
                    return v;
            }
        } else if (r.tokenType() == QXmlStreamReader::EndElement
                   && r.name() == eprElement) {
            inEpr = false;
        }
    }
    return {};
}

} // namespace

void getEventSubscriptions(WsmanClient *client,
                           std::function<void(EventSubscriptionsResult)> callback)
{
    // Three parallel enumerates. All three are optional — older firmware
    // SKUs (ISM) don't expose subscription support and return SOAP faults
    // on the listener / subscription enumerates while still returning the
    // filter catalog. Treat any one returning rows as success.
    struct Acc {
        EventSubscriptionsResult r;
        bool gotFilters = false;
        bool gotListeners = false;
        bool gotSubscriptions = false;
        QString filterErr;
        QString listenerErr;
        QString subscriptionErr;
        std::function<void(EventSubscriptionsResult)> cb;
        void maybeFire() {
            if (!gotFilters || !gotListeners || !gotSubscriptions) return;
            const bool allFailed = !filterErr.isEmpty()
                                && !listenerErr.isEmpty()
                                && !subscriptionErr.isEmpty();
            r.ok = !allFailed;
            if (!r.ok && r.error.isEmpty()) {
                r.error = !subscriptionErr.isEmpty()
                              ? subscriptionErr
                              : (!listenerErr.isEmpty() ? listenerErr : filterErr);
            }
            cb(std::move(r));
        }
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    enumerateAll(client, kFilterCollectionResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotFilters = true;
            acc->filterErr = err;
            for (const QByteArray &it : items) {
                EventFilter f;
                f.instanceId     = findScalar(it, QStringLiteral("InstanceID"));
                f.collectionName = findScalar(it, QStringLiteral("CollectionName"));
                if (!f.instanceId.isEmpty())
                    acc->r.filters.append(std::move(f));
            }
            acc->maybeFire();
        });

    enumerateAll(client, kListenerDestinationResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotListeners = true;
            acc->listenerErr = err;
            for (const QByteArray &it : items) {
                EventListener l;
                l.name        = findScalar(it, QStringLiteral("Name"));
                l.destination = findScalar(it, QStringLiteral("Destination"));
                bool conv = false;
                l.deliveryMode = findScalar(it,
                    QStringLiteral("DeliveryMode")).toInt(&conv);
                if (!conv) l.deliveryMode = -1;
                l.deliveryModeLabel = listenerDeliveryModeLabel(l.deliveryMode);
                if (!l.destination.isEmpty())
                    acc->r.listeners.append(std::move(l));
            }
            acc->maybeFire();
        });

    enumerateAll(client, kFilterCollectionSubscriptionResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotSubscriptions = true;
            acc->subscriptionErr = err;
            for (const QByteArray &it : items) {
                EventSubscription s;
                s.filterInstanceId = extractEprSelector(it,
                    QStringLiteral("Filter"),
                    QStringLiteral("InstanceID"));
                s.listenerName = extractEprSelector(it,
                    QStringLiteral("Handler"),
                    QStringLiteral("Name"));
                acc->r.subscriptions.append(std::move(s));
            }
            acc->maybeFire();
        });
}

namespace {

constexpr char kNsEventing[] =
    "http://schemas.xmlsoap.org/ws/2004/08/eventing";

/// Map our enum to the URI AMT expects in the Delivery `Mode` attr.
QString deliveryModeUri(EventDeliveryMode m)
{
    return m == EventDeliveryMode::PushWithAck
        ? QStringLiteral("http://schemas.dmtf.org/wbem/wsman/1/wsman/PushWithAck")
        : QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/eventing/DeliveryModes/Push");
}

/// Build the WS-Eventing Subscribe envelope. ResourceURI is the
/// `CIM_FilterCollection` we're subscribing to; the filter row's
/// `InstanceID` goes into the SelectorSet. If `user` and `pass` are
/// both non-empty we emit a WS-Trust UsernameToken issued-tokens
/// block plus the `w:Auth` profile element, mirroring how the legacy
/// MeshCommander pushed HTTP basic auth credentials through to the
/// listener.
QByteArray buildSubscribeEnvelope(const QString &filterInstanceId,
                                    EventDeliveryMode deliveryMode,
                                    const QString &notifyUrl,
                                    const QString &user,
                                    const QString &pass,
                                    const QString &to,
                                    const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    constexpr char kNsTrust[] = "http://schemas.xmlsoap.org/ws/2005/02/trust";
    constexpr char kNsWsse[] =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd";
    const QString resource = QString::fromLatin1(kFilterCollectionResource);
    const QString action = QString::fromLatin1(kNsEventing) + QStringLiteral("/Subscribe");

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(QString::fromLatin1(kNsEventing),   QStringLiteral("e"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("SelectorSet"));
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("Selector"));
    w.writeAttribute(QStringLiteral("Name"), QStringLiteral("InstanceID"));
    w.writeCharacters(filterInstanceId);
    w.writeEndElement(); // Selector
    w.writeEndElement(); // SelectorSet

    // WS-Trust issued-tokens block — only emitted when both user and
    // pass are supplied. The firmware uses this to forward HTTP basic
    // auth to the listener side; without it the listener gets the
    // notifications unauthenticated.
    const bool withAuth = !user.isEmpty() && !pass.isEmpty();
    if (withAuth) {
        w.writeNamespace(QString::fromLatin1(kNsTrust), QStringLiteral("t"));
        w.writeNamespace(QString::fromLatin1(kNsWsse),  QStringLiteral("se"));
        w.writeStartElement(QString::fromLatin1(kNsTrust),
                             QStringLiteral("IssuedTokens"));
        w.writeStartElement(QString::fromLatin1(kNsTrust),
                             QStringLiteral("RequestSecurityTokenResponse"));
        w.writeTextElement(QString::fromLatin1(kNsTrust),
                             QStringLiteral("TokenType"),
                             QStringLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#UsernameToken"));
        w.writeStartElement(QString::fromLatin1(kNsTrust),
                             QStringLiteral("RequestedSecurityToken"));
        w.writeStartElement(QString::fromLatin1(kNsWsse),
                             QStringLiteral("UsernameToken"));
        w.writeTextElement(QString::fromLatin1(kNsWsse),
                             QStringLiteral("Username"), user);
        w.writeStartElement(QString::fromLatin1(kNsWsse),
                             QStringLiteral("Password"));
        w.writeAttribute(QStringLiteral("Type"),
                          QStringLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd#PasswordText"));
        w.writeCharacters(pass);
        w.writeEndElement(); // Password
        w.writeEndElement(); // UsernameToken
        w.writeEndElement(); // RequestedSecurityToken
        w.writeEndElement(); // RequestSecurityTokenResponse
        w.writeEndElement(); // IssuedTokens
    }
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(QString::fromLatin1(kNsEventing),
                         QStringLiteral("Subscribe"));
    w.writeStartElement(QString::fromLatin1(kNsEventing),
                         QStringLiteral("Delivery"));
    w.writeAttribute(QStringLiteral("Mode"), deliveryModeUri(deliveryMode));
    w.writeStartElement(QString::fromLatin1(kNsEventing),
                         QStringLiteral("NotifyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"), notifyUrl);
    w.writeEndElement(); // NotifyTo
    if (withAuth) {
        // Profile marker — partners the WS-Trust block in the header.
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                             QStringLiteral("Auth"));
        w.writeAttribute(QStringLiteral("Profile"),
                          QStringLiteral("http://schemas.dmtf.org/wbem/wsman/1/wsman/secprofile/http/basic"));
        w.writeEndElement(); // Auth
    }
    w.writeEndElement(); // Delivery
    w.writeEndElement(); // Subscribe
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

/// Build a WS-Eventing Unsubscribe envelope. The CIM subscription
/// row is keyed by two EPR-valued selectors (Filter and Handler),
/// each of which is an embedded EPR pointing at the originating
/// row. We hand-write the EPR XML rather than going through a
/// generic helper because the existing builder doesn't support
/// nested EPRs as selector content.
QByteArray buildUnsubscribeEnvelope(const QString &filterInstanceId,
                                       const QString &listenerName,
                                       const QString &to,
                                       const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource = QString::fromLatin1(kFilterCollectionSubscriptionResource);
    const QString action = QString::fromLatin1(kNsEventing) + QStringLiteral("/Unsubscribe");
    const QString filterResource = QString::fromLatin1(kFilterCollectionResource);
    const QString handlerResource =
        QString::fromLatin1(kListenerDestinationWSManagementResource);
    const QString anonRole =
        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous");

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(QString::fromLatin1(kNsEventing),   QStringLiteral("e"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"), anonRole);
    w.writeEndElement(); // ReplyTo

    // SelectorSet with two EPR-valued selectors. Each selector's
    // text content is an embedded EPR.
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("SelectorSet"));

    // Filter selector: EPR → CIM_FilterCollection { InstanceID }.
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("Selector"));
    w.writeAttribute(QStringLiteral("Name"), QStringLiteral("Filter"));
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("EndpointReference"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"), anonRole);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReferenceParameters"));
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), filterResource);
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("SelectorSet"));
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("Selector"));
    w.writeAttribute(QStringLiteral("Name"), QStringLiteral("InstanceID"));
    w.writeCharacters(filterInstanceId);
    w.writeEndElement(); // Selector (inner)
    w.writeEndElement(); // SelectorSet (inner)
    w.writeEndElement(); // ReferenceParameters
    w.writeEndElement(); // EndpointReference
    w.writeEndElement(); // Selector (Filter)

    // Handler selector: EPR → CIM_ListenerDestinationWSManagement
    // with the standard 4-key tuple. CreationClassName is
    // "CIM_ListenerDestinationWSMAN" (not …WSManagement) — that's
    // the legacy convention and matches what AMT echoes back on
    // enumeration.
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("Selector"));
    w.writeAttribute(QStringLiteral("Name"), QStringLiteral("Handler"));
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("EndpointReference"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"), anonRole);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReferenceParameters"));
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), handlerResource);
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("SelectorSet"));
    for (const auto &kv : std::initializer_list<std::pair<QString, QString>>{
             { QStringLiteral("CreationClassName"),       QStringLiteral("CIM_ListenerDestinationWSMAN") },
             { QStringLiteral("Name"),                    listenerName },
             { QStringLiteral("SystemCreationClassName"), QStringLiteral("CIM_ComputerSystem") },
             { QStringLiteral("SystemName"),              QStringLiteral("Intel(r) AMT") },
         }) {
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), kv.first);
        w.writeCharacters(kv.second);
        w.writeEndElement(); // Selector (inner)
    }
    w.writeEndElement(); // SelectorSet (inner)
    w.writeEndElement(); // ReferenceParameters
    w.writeEndElement(); // EndpointReference
    w.writeEndElement(); // Selector (Handler)

    w.writeEndElement(); // SelectorSet (outer)
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeEmptyElement(QString::fromLatin1(kNsEventing),
                         QStringLiteral("Unsubscribe"));
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

} // namespace

void subscribeToEventFilter(WsmanClient *client,
                              const QString &filterInstanceId,
                              EventDeliveryMode deliveryMode,
                              const QString &notifyUrl,
                              const QString &user,
                              const QString &pass,
                              std::function<void(InvokeResult)> callback)
{
    if (filterInstanceId.isEmpty()) {
        callback({ false, QStringLiteral("Subscribe: filter is required"), -1 });
        return;
    }
    if (notifyUrl.isEmpty()) {
        callback({ false, QStringLiteral("Subscribe: notify URL is required"), -1 });
        return;
    }
    const QByteArray env = buildSubscribeEnvelope(filterInstanceId, deliveryMode,
        notifyUrl, user, pass,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            // WS-Eventing SubscribeResponse contains a SubscriptionManager
            // EPR + an Expires element; presence of either is enough to
            // treat the call as successful. AMT also returns plain SOAP
            // faults on failure which `runRequest` surfaces via `error`.
            if (body.contains("SubscribeResponse")
                || body.contains("SubscriptionManager")) {
                r.ok = true;
                r.returnValue = 0;
                return;
            }
            r.error = QStringLiteral("Subscribe response had no SubscriptionManager");
        },
        std::move(callback));
}

QByteArray buildSubscribeEnvelopeForTesting(const QString &filterInstanceId,
                                              EventDeliveryMode deliveryMode,
                                              const QString &notifyUrl,
                                              const QString &user,
                                              const QString &pass,
                                              const QString &to,
                                              const QString &messageId)
{
    return buildSubscribeEnvelope(filterInstanceId, deliveryMode,
                                    notifyUrl, user, pass, to, messageId);
}

void unsubscribeFromEventFilter(WsmanClient *client,
                                  const QString &filterInstanceId,
                                  const QString &listenerName,
                                  std::function<void(InvokeResult)> callback)
{
    if (filterInstanceId.isEmpty() || listenerName.isEmpty()) {
        callback({ false, QStringLiteral("Unsubscribe: filter and listener are required"), -1 });
        return;
    }
    const QByteArray env = buildUnsubscribeEnvelope(filterInstanceId, listenerName,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            // WS-Eventing UnsubscribeResponse is empty on success; the
            // runRequest layer already trips into `error` on a SOAP
            // fault, so reaching here means the subscription is gone.
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

QByteArray buildUnsubscribeEnvelopeForTesting(const QString &filterInstanceId,
                                                const QString &listenerName,
                                                const QString &to,
                                                const QString &messageId)
{
    return buildUnsubscribeEnvelope(filterInstanceId, listenerName, to, messageId);
}

namespace {

/// Walk one `IPS_AlarmClockOccurrence` row and pull out the fields
/// that live inside nested wrapper elements. The AMT firmware reports
/// times as `<StartTime><Datetime>2026-01-01T08:00:00Z</Datetime></StartTime>`
/// and recurrences as `<Interval><Interval>P1DT0H0M</Interval></Interval>`;
/// `findScalar`'s default behaviour can't drill into those nested
/// duplicates, so do it manually.
void parseWakeAlarm(const QByteArray &itemXml, WakeAlarm &out)
{
    QXmlStreamReader r(itemXml);
    enum Region { Top, InStartTime, InInterval } region = Top;
    while (!r.atEnd() && !r.hasError()) {
        r.readNext();
        if (r.tokenType() == QXmlStreamReader::StartElement) {
            const auto name = r.name();
            if (region == Top) {
                if (name == QStringLiteral("InstanceID")) {
                    out.instanceId = r.readElementText().trimmed();
                } else if (name == QStringLiteral("ElementName")) {
                    out.elementName = r.readElementText().trimmed();
                } else if (name == QStringLiteral("DeleteOnCompletion")) {
                    const QString v = r.readElementText().trimmed();
                    out.deleteOnCompletion =
                        (v == QStringLiteral("true") || v == QStringLiteral("1"));
                } else if (name == QStringLiteral("StartTime")) {
                    region = InStartTime;
                } else if (name == QStringLiteral("Interval")) {
                    region = InInterval;
                }
            } else if (region == InStartTime
                       && name == QStringLiteral("Datetime")) {
                out.startTimeIso = r.readElementText().trimmed();
            } else if (region == InInterval
                       && name == QStringLiteral("Interval")) {
                out.intervalIso = r.readElementText().trimmed();
            }
        } else if (r.tokenType() == QXmlStreamReader::EndElement) {
            const auto name = r.name();
            if (name == QStringLiteral("StartTime") && region == InStartTime)
                region = Top;
            else if (name == QStringLiteral("Interval") && region == InInterval)
                region = Top;
        }
    }
}

} // namespace

void getWakeAlarms(WsmanClient *client,
                   std::function<void(WakeAlarmsResult)> callback)
{
    auto cb = std::make_shared<std::function<void(WakeAlarmsResult)>>(std::move(callback));
    enumerateAll(client, kAlarmClockOccurrenceResource,
        [cb](QList<QByteArray> items, QString err) {
            WakeAlarmsResult r;
            r.error = err;
            r.ok = err.isEmpty();
            for (const QByteArray &it : items) {
                WakeAlarm a;
                parseWakeAlarm(it, a);
                if (!a.instanceId.isEmpty() || !a.startTimeIso.isEmpty())
                    r.alarms.append(std::move(a));
            }
            (*cb)(std::move(r));
        });
}

namespace {

/// Build an `AMT_AlarmClockService.AddAlarm` envelope by hand.
/// The method input wraps an embedded `IPS_AlarmClockOccurrence` in
/// an `AlarmTemplate` element; the embedded instance is in the IPS
/// namespace and uses the nested `<StartTime><Datetime>…` and
/// `<Interval><Interval>…` shape the read side already parses. The
/// interval block is omitted entirely for a one-shot alarm — sending
/// an empty `PT0S` makes some firmware reject the call.
QByteArray buildAddAlarmEnvelope(const WakeAlarm &alarm,
                                  const QString &to,
                                  const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    constexpr char kNsCimCommon[] =
        "http://schemas.dmtf.org/wbem/wscim/1/common";
    const QString service = QString::fromLatin1(kAlarmClockServiceResource);
    const QString occurrence = QString::fromLatin1(kAlarmClockOccurrenceResource);
    const QString action = service + QStringLiteral("/AddAlarm");

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(service,                             QStringLiteral("r"));
    w.writeNamespace(occurrence,                          QStringLiteral("p"));
    w.writeNamespace(QString::fromLatin1(kNsCimCommon),  QStringLiteral("b"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header — keyed at the singleton AlarmClockService.
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), service);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeStartElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("SelectorSet"));
    for (const auto &kv : std::initializer_list<std::pair<QString, QString>>{
             { QStringLiteral("Name"),                    QStringLiteral("Intel(r) AMT Alarm Clock Service") },
             { QStringLiteral("SystemCreationClassName"), QStringLiteral("CIM_ComputerSystem") },
             { QStringLiteral("SystemName"),              QStringLiteral("Intel(r) AMT") },
             { QStringLiteral("CreationClassName"),       QStringLiteral("AMT_AlarmClockService") },
         }) {
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), kv.first);
        w.writeCharacters(kv.second);
        w.writeEndElement(); // Selector
    }
    w.writeEndElement(); // SelectorSet
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(service, QStringLiteral("AddAlarm_INPUT"));
    w.writeStartElement(service, QStringLiteral("AlarmTemplate"));

    // Embedded IPS_AlarmClockOccurrence under the IPS namespace.
    w.writeTextElement(occurrence, QStringLiteral("ElementName"),
                        alarm.elementName);
    w.writeTextElement(occurrence, QStringLiteral("InstanceID"),
                        alarm.elementName);

    // <StartTime><Datetime>ISO</Datetime></StartTime>
    w.writeStartElement(occurrence, QStringLiteral("StartTime"));
    w.writeTextElement(QString::fromLatin1(kNsCimCommon),
                        QStringLiteral("Datetime"), alarm.startTimeIso);
    w.writeEndElement(); // StartTime

    // <Interval><Interval>P…</Interval></Interval> — omitted for one-shot.
    if (!alarm.intervalIso.isEmpty()) {
        w.writeStartElement(occurrence, QStringLiteral("Interval"));
        w.writeTextElement(QString::fromLatin1(kNsCimCommon),
                            QStringLiteral("Interval"), alarm.intervalIso);
        w.writeEndElement(); // Interval
    }

    w.writeTextElement(occurrence, QStringLiteral("DeleteOnCompletion"),
                        alarm.deleteOnCompletion
                            ? QStringLiteral("true")
                            : QStringLiteral("false"));

    w.writeEndElement(); // AlarmTemplate
    w.writeEndElement(); // AddAlarm_INPUT
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

} // namespace

void addWakeAlarm(WsmanClient *client, const WakeAlarm &alarm,
                  std::function<void(InvokeResult)> callback)
{
    if (alarm.elementName.isEmpty() || alarm.startTimeIso.isEmpty()) {
        callback({ false, QStringLiteral("AddAlarm: ElementName and StartTime are required"), -1 });
        return;
    }
    const QByteArray env = buildAddAlarmEnvelope(alarm,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                // Some AMT versions return an EPR with no ReturnValue
                // on success. Treat that as success too.
                if (body.contains("AlarmRef") || body.contains("AddAlarm_OUTPUT")) {
                    r.ok = true;
                    r.returnValue = 0;
                    return;
                }
                r.error = QStringLiteral("AddAlarm response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && r.returnValue == 0;
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("AddAlarm returned %1").arg(rv);
        },
        std::move(callback));
}

QByteArray buildAddAlarmEnvelopeForTesting(const WakeAlarm &alarm,
                                            const QString &to,
                                            const QString &messageId)
{
    return buildAddAlarmEnvelope(alarm, to, messageId);
}

void deleteWakeAlarm(WsmanClient *client, const QString &instanceId,
                     std::function<void(InvokeResult)> callback)
{
    if (instanceId.isEmpty()) {
        callback({ false, QStringLiteral("DeleteAlarm: InstanceID is required"), -1 });
        return;
    }
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kAlarmClockOccurrenceResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

namespace {

/// Expand a bare class name into the full WSMAN resource URI by
/// looking at the standard `AMT_`/`IPS_`/`CIM_` prefix. Inputs that
/// already look like URIs (start with `http`) pass through unchanged.
QString resolveBrowseUri(const QString &classOrUri)
{
    if (classOrUri.startsWith(QLatin1String("http"))) return classOrUri;
    if (classOrUri.startsWith(QLatin1String("AMT_")))
        return QStringLiteral("http://intel.com/wbem/wscim/1/amt-schema/1/")
             + classOrUri;
    if (classOrUri.startsWith(QLatin1String("IPS_")))
        return QStringLiteral("http://intel.com/wbem/wscim/1/ips-schema/1/")
             + classOrUri;
    if (classOrUri.startsWith(QLatin1String("CIM_")))
        return QStringLiteral("http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/")
             + classOrUri;
    // Anything else — return as-is and let the firmware error out
    // rather than guessing.
    return classOrUri;
}

} // namespace

void executeBrowse(WsmanClient *client, const QString &classOrUri,
                   BrowseKind kind,
                   const QHash<QString, QString> &selectors,
                   std::function<void(WsmanBrowseResult)> callback)
{
    auto cb = std::make_shared<std::function<void(WsmanBrowseResult)>>(
        std::move(callback));
    const QString uri = resolveBrowseUri(classOrUri.trimmed());

    if (client == nullptr) {
        WsmanBrowseResult r;
        r.kind = kind;
        r.error = QStringLiteral("client is null");
        (*cb)(std::move(r));
        return;
    }

    if (kind == BrowseKind::Get) {
        const QByteArray env = buildGetEnvelope(uri, selectors,
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, cb, kind]() mutable {
                WsmanBrowseResult r;
                r.kind = kind;
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                if (err) {
                    r.error = errString;
                    (*cb)(std::move(r));
                    return;
                }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) {
                    r.error = soap.fault;
                    r.xml = body;     // Surface the SOAP fault for visibility.
                    (*cb)(std::move(r));
                    return;
                }
                r.xml = body;         // Raw response body, including envelope.
                r.ok = true;
                (*cb)(std::move(r));
            });
        return;
    }

    // Enumerate — walk all pulls and stitch each item under a synthetic
    // `<Items>` wrapper so the caller can render the whole result.
    enumerateAll(client, uri.toLatin1().constData(),
        [cb, kind](QList<QByteArray> items, QString err) {
            WsmanBrowseResult r;
            r.kind = kind;
            if (!err.isEmpty()) {
                r.error = err;
                (*cb)(std::move(r));
                return;
            }
            QByteArray merged;
            merged.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            merged.append("<wsen:Items xmlns:wsen=\""
                          "http://schemas.xmlsoap.org/ws/2004/09/enumeration\">\n");
            for (const QByteArray &it : items) {
                merged.append(it);
                merged.append('\n');
            }
            merged.append("</wsen:Items>\n");
            r.xml = std::move(merged);
            r.itemCount = items.size();
            r.ok = true;
            (*cb)(std::move(r));
        });
}

namespace {

/// Walk an `AMT_ActiveFilterStatistics` row and pull the scalar
/// fields. AMT exposes counters as unsigned 64-bit values; we keep
/// them in `quint64` and treat parse failures as 0 (the read side
/// just shows a zero rather than dropping the row).
ActiveFilterStatRow parseActiveFilterStat(const QByteArray &itemXml)
{
    ActiveFilterStatRow s;
    s.filterInstanceId =
        findScalar(itemXml, QStringLiteral("InstanceID"));
    bool conv = false;
    const quint64 pp = findScalar(itemXml,
        QStringLiteral("PacketsPassed")).toULongLong(&conv);
    if (conv) s.packetsPassed = pp;
    conv = false;
    const quint64 pd = findScalar(itemXml,
        QStringLiteral("PacketsDropped")).toULongLong(&conv);
    if (conv) s.packetsDropped = pd;
    return s;
}

} // namespace

void getSystemDefense(WsmanClient *client,
                      std::function<void(SystemDefenseResult)> callback)
{
    // Five parallel enumerates. The firmware returns SOAP fault on
    // every class for non-ACM / pre-AMT-6 boxes; treat consistent
    // failure across all four core classes (excluding stats, which
    // some pre-stats firmware drops separately) as "not supported"
    // rather than an error.
    struct Acc {
        SystemDefenseResult r;
        bool gotPolicies = false;
        bool gotHdr      = false;
        bool gotIp       = false;
        bool gotSub      = false;
        bool gotStats    = false;
        int faulted      = 0;
        std::function<void(SystemDefenseResult)> cb;
        void maybeFire() {
            if (!gotPolicies || !gotHdr || !gotIp || !gotSub || !gotStats) return;
            if (faulted == 4) {
                r.supported = false;
                r.ok = true;
            } else {
                r.ok = true;
            }
            cb(std::move(r));
        }
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    enumerateAll(client, kSystemDefensePolicyResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotPolicies = true;
            if (!err.isEmpty()) ++acc->faulted;
            for (const QByteArray &it : items) {
                SystemDefensePolicy p;
                p.instanceId = findScalar(it, QStringLiteral("InstanceID"));
                p.policyName = findScalar(it, QStringLiteral("PolicyName"));
                bool conv = false;
                const int pr = findScalar(it,
                    QStringLiteral("Priority")).toInt(&conv);
                if (conv) p.priority = pr;
                p.txEnabled = findScalar(it,
                    QStringLiteral("AntiSpoofingSupport")) == QStringLiteral("true");
                p.rxEnabled = findScalar(it,
                    QStringLiteral("PolicyEnabled")) == QStringLiteral("true");
                p.defaultPolicy = findScalar(it,
                    QStringLiteral("DefaultPolicy")) == QStringLiteral("true");
                if (!p.instanceId.isEmpty())
                    acc->r.policies.append(std::move(p));
            }
            acc->maybeFire();
        });

    enumerateAll(client, kHdr8021FilterResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotHdr = true;
            if (!err.isEmpty()) ++acc->faulted;
            for (const QByteArray &it : items) {
                Hdr8021Filter f;
                f.instanceId = findScalar(it, QStringLiteral("InstanceID"));
                f.name       = findScalar(it, QStringLiteral("Name"));
                bool conv = false;
                f.filterDirection = findScalar(it,
                    QStringLiteral("FilterDirection")).toInt(&conv);
                if (!conv) f.filterDirection = -1;
                conv = false;
                f.vlanTag = findScalar(it,
                    QStringLiteral("VLANTag")).toInt(&conv);
                if (!conv) f.vlanTag = -1;
                conv = false;
                // Field on the wire is `HdrProtocolID8021`, not
                // `EtherType` — the earlier `EtherType` scrape never
                // matched anything and the L2-row chip rendered empty
                // for every filter. See #353.
                f.etherType = findScalar(it,
                    QStringLiteral("HdrProtocolID8021")).toInt(&conv);
                if (!conv) f.etherType = -1;
                conv = false;
                f.priority = findScalar(it,
                    QStringLiteral("Priority")).toInt(&conv);
                if (!conv) f.priority = -1;
                if (!f.instanceId.isEmpty())
                    acc->r.hdrFilters.append(std::move(f));
            }
            acc->maybeFire();
        });

    enumerateAll(client, kIpHeadersFilterResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotIp = true;
            if (!err.isEmpty()) ++acc->faulted;
            for (const QByteArray &it : items) {
                IpHeadersFilter f;
                f.instanceId  = findScalar(it, QStringLiteral("InstanceID"));
                f.name        = findScalar(it, QStringLiteral("Name"));
                bool conv = false;
                f.filterDirection = findScalar(it,
                    QStringLiteral("FilterDirection")).toInt(&conv);
                if (!conv) f.filterDirection = -1;
                f.srcAddress = findScalar(it,
                    QStringLiteral("HdrSrcAddress"));
                f.dstAddress = findScalar(it,
                    QStringLiteral("HdrDestAddress"));
                conv = false;
                f.protocol = findScalar(it,
                    QStringLiteral("HdrProtocolID8")).toInt(&conv);
                if (!conv) f.protocol = -1;
                conv = false;
                f.srcPort = findScalar(it,
                    QStringLiteral("HdrSrcPortStart")).toInt(&conv);
                if (!conv) f.srcPort = -1;
                conv = false;
                f.dstPort = findScalar(it,
                    QStringLiteral("HdrDestPortStart")).toInt(&conv);
                if (!conv) f.dstPort = -1;
                if (!f.instanceId.isEmpty())
                    acc->r.ipFilters.append(std::move(f));
            }
            acc->maybeFire();
        });

    enumerateAll(client, kNetworkFilterResource,
        [acc](QList<QByteArray> items, QString err) {
            acc->gotSub = true;
            if (!err.isEmpty()) ++acc->faulted;
            for (const QByteArray &it : items) {
                NetworkFilterRow n;
                n.instanceId  = findScalar(it, QStringLiteral("InstanceID"));
                n.name        = findScalar(it, QStringLiteral("Name"));
                n.filterClass = findScalar(it,
                    QStringLiteral("CreationClassName"));
                if (!n.instanceId.isEmpty())
                    acc->r.subFilters.append(std::move(n));
            }
            acc->maybeFire();
        });

    // Stats are best-effort: some firmware exposes the policy/filter
    // classes but not AMT_ActiveFilterStatistics. Drop the rows on
    // fault — don't count toward the four-class "not supported" gate.
    enumerateAll(client, kActiveFilterStatisticsResource,
        [acc](QList<QByteArray> items, QString /*err*/) {
            acc->gotStats = true;
            for (const QByteArray &it : items) {
                ActiveFilterStatRow s = parseActiveFilterStat(it);
                if (!s.filterInstanceId.isEmpty())
                    acc->r.stats.append(std::move(s));
            }
            acc->maybeFire();
        });
}

void getActiveFilterStatistics(WsmanClient *client,
                                std::function<void(QList<ActiveFilterStatRow>, QString)> callback)
{
    auto cb = std::make_shared<std::function<void(QList<ActiveFilterStatRow>, QString)>>(
        std::move(callback));
    enumerateAll(client, kActiveFilterStatisticsResource,
        [cb](QList<QByteArray> items, QString err) {
            QList<ActiveFilterStatRow> rows;
            rows.reserve(items.size());
            for (const QByteArray &it : items) {
                ActiveFilterStatRow s = parseActiveFilterStat(it);
                if (!s.filterInstanceId.isEmpty())
                    rows.append(std::move(s));
            }
            (*cb)(std::move(rows), err);
        });
}

namespace {

/// Common WS-Transfer Delete helper for the System Defense classes —
/// they all key off `InstanceID`. Surfaces an empty-instance-id error
/// up front rather than firing the request and waiting for AMT to
/// fault on the empty selector. See #346.
void deleteByInstanceId(WsmanClient *client, const char *resourceUri,
                          const QString &what,
                          const QString &instanceId,
                          std::function<void(InvokeResult)> callback)
{
    if (instanceId.isEmpty()) {
        callback({ false, QStringLiteral("%1: InstanceID is required").arg(what), -1 });
        return;
    }
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(resourceUri), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

} // namespace

void deleteSystemDefensePolicy(WsmanClient *client, const QString &instanceId,
                                 std::function<void(InvokeResult)> callback)
{
    deleteByInstanceId(client, kSystemDefensePolicyResource,
                        QStringLiteral("DeletePolicy"), instanceId,
                        std::move(callback));
}

void deleteHdr8021Filter(WsmanClient *client, const QString &instanceId,
                          std::function<void(InvokeResult)> callback)
{
    deleteByInstanceId(client, kHdr8021FilterResource,
                        QStringLiteral("DeleteHdr8021Filter"), instanceId,
                        std::move(callback));
}

void deleteIpHeadersFilter(WsmanClient *client, const QString &instanceId,
                            std::function<void(InvokeResult)> callback)
{
    deleteByInstanceId(client, kIpHeadersFilterResource,
                        QStringLiteral("DeleteIPHeadersFilter"), instanceId,
                        std::move(callback));
}

namespace {

/// Build a WS-Transfer Create envelope for `AMT_Hdr8021Filter`. Field
/// shape mirrors the legacy NW.js MeshCommander
/// (`c25301e^:legacy/source/Commander.htm` — `AddDefenseFilterOk`):
/// the three creation-class scalars (`InstanceID`, `CreationClassName`,
/// `SystemName`, `SystemCreationClassName`) are sent as `"0"` and the
/// firmware fills them in; the user-controlled fields go straight on
/// the wire under the AMT_Hdr8021Filter namespace. `FilterProfileData`
/// is omitted unless `filterProfile == 2` (Rate Limit) — sending it
/// on a non-rate-limit filter is what trips AMT's strict-mode
/// validation. See #353.
QByteArray buildAddHdr8021FilterEnvelope(const Hdr8021Filter &f,
                                            const QString &to,
                                            const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    constexpr char kNsTransfer[] = "http://schemas.xmlsoap.org/ws/2004/09/transfer";
    const QString resource = QString::fromLatin1(kHdr8021FilterResource);
    const QString action =
        QString::fromLatin1(kNsTransfer) + QStringLiteral("/Create");

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("r"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeEndElement(); // Header

    // Body — new instance, all under the AMT_Hdr8021Filter namespace.
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource, QStringLiteral("AMT_Hdr8021Filter"));

    w.writeTextElement(resource, QStringLiteral("InstanceID"),
                        QStringLiteral("0"));
    w.writeTextElement(resource, QStringLiteral("Name"), f.name);
    w.writeTextElement(resource, QStringLiteral("CreationClassName"),
                        QStringLiteral("0"));
    w.writeTextElement(resource, QStringLiteral("SystemName"),
                        QStringLiteral("0"));
    w.writeTextElement(resource, QStringLiteral("SystemCreationClassName"),
                        QStringLiteral("0"));
    w.writeTextElement(resource, QStringLiteral("HdrProtocolID8021"),
                        QString::number(f.etherType));
    w.writeTextElement(resource, QStringLiteral("FilterProfile"),
                        QString::number(f.filterProfile));
    w.writeTextElement(resource, QStringLiteral("FilterDirection"),
                        QString::number(f.filterDirection));
    w.writeTextElement(resource, QStringLiteral("ActionEventOnMatch"),
                        f.actionEventOnMatch
                            ? QStringLiteral("true")
                            : QStringLiteral("false"));
    if (f.filterProfile == 2 /* Rate Limit */) {
        w.writeTextElement(resource, QStringLiteral("FilterProfileData"),
                            QString::number(f.filterProfileData));
    }

    w.writeEndElement(); // AMT_Hdr8021Filter
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

} // namespace

void addHdr8021Filter(WsmanClient *client, const Hdr8021Filter &filter,
                       std::function<void(InvokeResult)> callback)
{
    if (filter.name.isEmpty()) {
        callback({ false, QStringLiteral("AddHdr8021Filter: name is required"), -1 });
        return;
    }
    if (filter.etherType < 0) {
        callback({ false, QStringLiteral("AddHdr8021Filter: ethertype is required"), -1 });
        return;
    }
    if (filter.filterProfile < 0 || filter.filterProfile > 4) {
        callback({ false, QStringLiteral("AddHdr8021Filter: filter profile out of range"), -1 });
        return;
    }
    if (filter.filterDirection != 0 && filter.filterDirection != 1) {
        callback({ false, QStringLiteral("AddHdr8021Filter: direction must be 0 (Tx) or 1 (Rx)"), -1 });
        return;
    }
    const QByteArray env = buildAddHdr8021FilterEnvelope(filter,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            // CreateResponse echoes the EPR of the new row; we
            // don't need to parse the InstanceID since the section
            // re-enumerates on success. Absence of a fault tag is
            // enough to call it a success — the run-request layer
            // already maps SOAP faults into r.error.
            if (body.contains("CreateResponse")
                || body.contains("AMT_Hdr8021Filter")) {
                r.ok = true;
                r.returnValue = 0;
                return;
            }
            r.error = QStringLiteral("AddHdr8021Filter response had no CreateResponse");
        },
        std::move(callback));
}

QByteArray buildAddHdr8021FilterEnvelopeForTesting(const Hdr8021Filter &filter,
                                                       const QString &to,
                                                       const QString &messageId)
{
    return buildAddHdr8021FilterEnvelope(filter, to, messageId);
}

void setPowerScheme(WsmanClient *client, const QString &instanceId,
                    std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kSystemPowerSchemeResource),
        QStringLiteral("SetPowerScheme"), selectors, {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && r.returnValue == 0;
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("SetPowerScheme returned %1").arg(rv);
        },
        std::move(callback));
}

void getTimeSettings(WsmanClient *client,
                     std::function<void(TimeSettingsResult)> callback)
{
    // GetLowAccuracyTimeSynch is the canonical "what time is it" call; a
    // Get on AMT_TimeSynchronizationService doesn't return a timestamp.
    const QString resource = QString::fromLatin1(kTimeSyncResource);
    const QByteArray env = buildInvokeEnvelope(resource,
        QStringLiteral("GetLowAccuracyTimeSynch"), {}, {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<TimeSettingsResult>(client, env, {},
        [](const QByteArray &body, TimeSettingsResult &r) {
            const QString ta = findScalar(body, QStringLiteral("Ta0"));
            if (ta.isEmpty()) {
                r.error = QStringLiteral("response had no Ta0 timestamp");
                return;
            }
            bool conv = false;
            r.secondsSinceEpoch = ta.toLongLong(&conv);
            r.ok = conv;
            if (!conv) r.error = QStringLiteral("Ta0 '%1' was not numeric").arg(ta);
        },
        std::move(callback));
}

void setHighAccuracyTimeSync(WsmanClient *client,
                             qint64 ta0, qint64 tm1, qint64 tm2,
                             std::function<void(InvokeResult)> callback)
{
    // AMT's XSD requires Ta0/Tm1/Tm2 in this exact order; the
    // unordered overload iterates a QHash and trips
    // SchemaValidationError when the hash spits them out shuffled.
    const QList<QPair<QString, QString>> params{
        { QStringLiteral("Ta0"), QString::number(ta0) },
        { QStringLiteral("Tm1"), QString::number(tm1) },
        { QStringLiteral("Tm2"), QString::number(tm2) },
    };
    const QByteArray env = buildInvokeEnvelopeOrdered(
        QString::fromLatin1(kTimeSyncResource),
        QStringLiteral("SetHighAccuracyTimeSynch"), {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && r.returnValue == 0;
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("SetHighAccuracyTimeSynch returned %1").arg(rv);
        },
        std::move(callback));
}

namespace {

QString endpointStr(WsmanClient *client)
{
    return client ? client->endpoint().toString() : QString();
}

/// One async step in the boot-action chain. Builds the envelope, sends
/// it, parses the standard SOAP fault path, then either fails the chain
/// or invokes `next` on success. `name` is for error messages.
template <typename ExtractRv>
void runChainStep(WsmanClient *client, const QByteArray &envelope, const QString &name,
                  ExtractRv &&extract,
                  std::function<void(InvokeResult)> onError,
                  std::function<void()> next)
{
    if (client == nullptr) {
        InvokeResult r{false, QStringLiteral("client is null"), -1};
        onError(std::move(r));
        return;
    }
    WsmanReply *reply = client->sendEnvelope(envelope);
    QObject::connect(reply, &WsmanReply::finished, client,
                     [reply, name, extract = std::forward<ExtractRv>(extract),
                       onError = std::move(onError), next = std::move(next)]() mutable {
                         const QByteArray body = reply->readAll();
                         const auto err = reply->hasError();
                         const auto errString = reply->errorString();
                         reply->deleteLater();
                         if (err) {
                             onError({false, QStringLiteral("%1: %2").arg(name).arg(errString), -1});
                             return;
                         }
                         const SoapResponse soap = parseResponse(body);
                         if (soap.isFault()) {
                             onError({false, QStringLiteral("%1: %2").arg(name).arg(soap.fault), -1});
                             return;
                         }
                         InvokeResult r;
                         extract(soap.bodyXml, r);
                         if (!r.ok) {
                             onError({false,
                                       QStringLiteral("%1: %2").arg(name).arg(
                                           r.error.isEmpty()
                                               ? QStringLiteral("ReturnValue %1").arg(r.returnValue)
                                               : r.error),
                                       r.returnValue});
                             return;
                         }
                         next();
                     });
}

bool readReturnValueOk(const QByteArray &body, InvokeResult &r)
{
    const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
    if (rv.isEmpty()) {
        r.error = QStringLiteral("response had no ReturnValue");
        return false;
    }
    bool conv = false;
    r.returnValue = rv.toInt(&conv);
    r.ok = conv && r.returnValue == 0;
    return r.ok;
}

void runPerformBootAction(WsmanClient *client, const BootActionParams &p,
                          std::function<void(InvokeResult)> callback)
{
    const QString to = endpointStr(client);

    // Step 5: RequestPowerStateChange.
    auto doPowerChange = [client, code = p.targetPowerState, callback, to]() mutable {
        requestPowerStateChange(client, code, std::move(callback));
    };

    // Step 4: set specific boot source (if any), else jump straight to step 5.
    auto doSetSpecificBootSource = [client, p, to, doPowerChange, callback]() mutable {
        if (p.amtBootSource.isEmpty()) {
            doPowerChange();
            return;
        }
        const QByteArray env = buildChangeBootOrderEnvelope(p.amtBootSource, to, newMessageId());
        runChainStep(client, env, QStringLiteral("ChangeBootOrder(%1)").arg(p.amtBootSource),
                     [](const QByteArray &body, InvokeResult &r) { readReturnValueOk(body, r); },
                     callback, doPowerChange);
    };

    // Step 3: SetBootConfigRole(1).
    auto doSetRole = [client, to, doSetSpecificBootSource, callback]() mutable {
        QHash<QString, QString> selectors;
        selectors.insert(QStringLiteral("Name"),
                         QStringLiteral("Intel(r) AMT Boot Service"));
        selectors.insert(QStringLiteral("SystemCreationClassName"),
                         QStringLiteral("CIM_ComputerSystem"));
        selectors.insert(QStringLiteral("SystemName"), QStringLiteral("Intel(r) AMT"));
        selectors.insert(QStringLiteral("CreationClassName"), QStringLiteral("CIM_BootService"));

        // BootConfigSetting EPR is the parameter.
        QHash<QString, QString> params;
        // We send the EPR-wrapped Role=IsNextSingleUse via a hand-built
        // envelope so we can include the BootConfigSetting EPR. The
        // generic buildInvokeEnvelope helper doesn't support EPR-shaped
        // parameters, so we splice it inline here.
        QByteArray envelope;
        QXmlStreamWriter w(&envelope);
        w.setAutoFormatting(false);
        constexpr const char *kSoap = "http://www.w3.org/2003/05/soap-envelope";
        constexpr const char *kAddr = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
        constexpr const char *kWsman = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
        const QString svcUri = QString::fromLatin1(kBootServiceResource);
        const QString cfgUri =
            QStringLiteral("http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_BootConfigSetting");

        w.writeStartDocument();
        w.writeNamespace(QString::fromLatin1(kSoap), QStringLiteral("s"));
        w.writeNamespace(QString::fromLatin1(kAddr), QStringLiteral("a"));
        w.writeNamespace(QString::fromLatin1(kWsman), QStringLiteral("w"));
        w.writeNamespace(svcUri, QStringLiteral("r"));

        w.writeStartElement(QString::fromLatin1(kSoap), QStringLiteral("Envelope"));
        w.writeStartElement(QString::fromLatin1(kSoap), QStringLiteral("Header"));
        w.writeTextElement(QString::fromLatin1(kAddr), QStringLiteral("Action"),
                            svcUri + QLatin1String("/SetBootConfigRole"));
        w.writeTextElement(QString::fromLatin1(kAddr), QStringLiteral("To"), to);
        w.writeTextElement(QString::fromLatin1(kWsman), QStringLiteral("ResourceURI"), svcUri);
        w.writeTextElement(QString::fromLatin1(kAddr), QStringLiteral("MessageID"),
                            newMessageId());
        w.writeStartElement(QString::fromLatin1(kAddr), QStringLiteral("ReplyTo"));
        w.writeTextElement(QString::fromLatin1(kAddr), QStringLiteral("Address"),
                            QString::fromLatin1(
                                "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
        w.writeEndElement(); // ReplyTo

        w.writeStartElement(QString::fromLatin1(kWsman), QStringLiteral("SelectorSet"));
        for (auto it = selectors.constBegin(); it != selectors.constEnd(); ++it) {
            w.writeStartElement(QString::fromLatin1(kWsman), QStringLiteral("Selector"));
            w.writeAttribute(QStringLiteral("Name"), it.key());
            w.writeCharacters(it.value());
            w.writeEndElement();
        }
        w.writeEndElement(); // SelectorSet
        w.writeEndElement(); // Header

        w.writeStartElement(QString::fromLatin1(kSoap), QStringLiteral("Body"));
        w.writeStartElement(svcUri, QStringLiteral("SetBootConfigRole_INPUT"));

        // BootConfigSetting EPR.
        w.writeStartElement(svcUri, QStringLiteral("BootConfigSetting"));
        w.writeTextElement(QString::fromLatin1(kAddr), QStringLiteral("Address"),
                            QString::fromLatin1(
                                "http://schemas.xmlsoap.org/ws/2004/08/addressing"));
        w.writeStartElement(QString::fromLatin1(kAddr),
                            QStringLiteral("ReferenceParameters"));
        w.writeTextElement(QString::fromLatin1(kWsman),
                            QStringLiteral("ResourceURI"), cfgUri);
        w.writeStartElement(QString::fromLatin1(kWsman), QStringLiteral("SelectorSet"));
        w.writeStartElement(QString::fromLatin1(kWsman), QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), QStringLiteral("InstanceID"));
        w.writeCharacters(QStringLiteral("Intel(r) AMT: Boot Configuration 0"));
        w.writeEndElement(); // Selector
        w.writeEndElement(); // SelectorSet
        w.writeEndElement(); // ReferenceParameters
        w.writeEndElement(); // BootConfigSetting

        w.writeTextElement(svcUri, QStringLiteral("Role"), QStringLiteral("1"));
        w.writeEndElement(); // SetBootConfigRole_INPUT
        w.writeEndElement(); // Body
        w.writeEndElement(); // Envelope
        w.writeEndDocument();

        runChainStep(client, envelope, QStringLiteral("SetBootConfigRole"),
                     [](const QByteArray &body, InvokeResult &r) { readReturnValueOk(body, r); },
                     callback, doSetSpecificBootSource);
    };

    // Step 2: write the AMT_BootSettingData with our flags.
    auto doPutBootSettingData = [client, p, to, doSetRole, callback]() mutable {
        QHash<QString, QString> props;
        const auto boolStr = [](bool v) { return v ? QStringLiteral("true") : QStringLiteral("false"); };
        props.insert(QStringLiteral("BIOSPause"),     boolStr(p.biosPause));
        props.insert(QStringLiteral("BIOSSetup"),     boolStr(p.biosSetup));
        props.insert(QStringLiteral("BootMediaIndex"), QStringLiteral("0"));
        props.insert(QStringLiteral("ConfigurationDataReset"), boolStr(false));
        props.insert(QStringLiteral("EnforceSecureBoot"), boolStr(false));
        props.insert(QStringLiteral("FirmwareVerbosity"), QStringLiteral("0"));
        props.insert(QStringLiteral("ForcedProgressEvents"), boolStr(false));
        props.insert(QStringLiteral("IDERBootDevice"), QString::number(p.iderBootDevice));
        props.insert(QStringLiteral("LockKeyboard"),    boolStr(false));
        props.insert(QStringLiteral("LockPowerButton"), boolStr(false));
        props.insert(QStringLiteral("LockResetButton"), boolStr(false));
        props.insert(QStringLiteral("LockSleepButton"), boolStr(false));
        props.insert(QStringLiteral("ReflashBIOS"),     boolStr(false));
        props.insert(QStringLiteral("UseIDER"),         boolStr(p.useIder));
        props.insert(QStringLiteral("UseSOL"),          boolStr(p.useSol));
        props.insert(QStringLiteral("UseSafeMode"),     boolStr(false));
        props.insert(QStringLiteral("UserPasswordBypass"), boolStr(false));
        if (p.secureErase) {
            props.insert(QStringLiteral("SecureErase"), QStringLiteral("true"));
            if (!p.rsePassword.isEmpty())
                props.insert(QStringLiteral("RSEPassword"), p.rsePassword);
        } else {
            props.insert(QStringLiteral("SecureErase"), QStringLiteral("false"));
        }
        if (p.platformErase) {
            props.insert(QStringLiteral("PlatformErase"), QStringLiteral("true"));
            if (!p.platformEraseTlvBase64.isEmpty()) {
                props.insert(QStringLiteral("UefiBootParametersArray"),
                             p.platformEraseTlvBase64);
                props.insert(QStringLiteral("UefiBootNumberOfParams"),
                             QString::number(p.platformEraseTlvCount));
            }
        } else {
            props.insert(QStringLiteral("PlatformErase"), QStringLiteral("false"));
        }
        if (p.httpsBootUrl && !p.httpsBootUrlStr.isEmpty()) {
            int httpsTlvCount = 0;
            const QByteArray httpsTlv =
                buildHttpsBootUrlTlv(p.httpsBootUrlStr, &httpsTlvCount);
            props.insert(QStringLiteral("UefiBootParametersArray"),
                         QString::fromLatin1(httpsTlv.toBase64()));
            props.insert(QStringLiteral("UefiBootNumberOfParams"),
                         QString::number(httpsTlvCount));
            // BootMediaIndex must be 0 for OCR — anything else makes
            // AMT prefer the legacy CD-ROM order over the URL boot.
            props.insert(QStringLiteral("BootMediaIndex"), QStringLiteral("0"));
        }

        // One-Click Recovery (#170). The caller already built the TLV
        // and set `amtBootSource` to the matching "Force OCR UEFI ..."
        // row; we just attach the bytes to the Put. BootMediaIndex is
        // 0 for the same reason as the HTTPS-Boot branch.
        if (p.oneClickRecovery && !p.ocrTlvBase64.isEmpty()) {
            props.insert(QStringLiteral("UefiBootParametersArray"), p.ocrTlvBase64);
            props.insert(QStringLiteral("UefiBootNumberOfParams"),
                         QString::number(p.ocrTlvCount));
            props.insert(QStringLiteral("BootMediaIndex"), QStringLiteral("0"));
        }

        QHash<QString, QString> selectors;
        selectors.insert(QStringLiteral("InstanceID"),
                         QStringLiteral("Intel(r) AMT:BootSettingData 0"));
        const QByteArray env = buildPutEnvelope(
            QString::fromLatin1(kBootSettingDataResource),
            QStringLiteral("AMT_BootSettingData"),
            selectors, props, to, newMessageId());
        runChainStep(client, env, QStringLiteral("Put AMT_BootSettingData"),
                     [](const QByteArray &, InvokeResult &r) { r.ok = true; r.returnValue = 0; },
                     callback, doSetRole);
    };

    // Step 1: clear the boot order.
    const QByteArray env = buildChangeBootOrderEnvelope(QString(), to, newMessageId());
    runChainStep(client, env, QStringLiteral("ChangeBootOrder(clear)"),
                 [](const QByteArray &body, InvokeResult &r) { readReturnValueOk(body, r); },
                 callback, doPutBootSettingData);
}

} // namespace

void performBootAction(WsmanClient *client, BootActionParams params,
                       std::function<void(InvokeResult)> callback)
{
    runPerformBootAction(client, params, std::move(callback));
}

void requestOsPowerStateChange(WsmanClient *client, int osPowerState,
                               std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"),
                     QStringLiteral("Intel(r) AMT Power Management Service"));
    selectors.insert(QStringLiteral("SystemCreationClassName"),
                     QStringLiteral("CIM_ComputerSystem"));
    selectors.insert(QStringLiteral("SystemName"), QStringLiteral("Intel(r) AMT"));
    selectors.insert(QStringLiteral("CreationClassName"),
                     QStringLiteral("IPS_PowerManagementService"));
    QHash<QString, QString> params;
    params.insert(QStringLiteral("OSPowerSavingState"), QString::number(osPowerState));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kIpsPowerServiceResource),
        QStringLiteral("RequestOSPowerSavingStateChange"),
        selectors, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && r.returnValue == 0;
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("RequestOSPowerSavingStateChange returned %1").arg(rv);
        },
        std::move(callback));
}

void requestPowerStateChange(WsmanClient *client, int powerState,
                             std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"),
                     QStringLiteral("Intel(r) AMT Power Management Service"));
    selectors.insert(QStringLiteral("SystemCreationClassName"),
                     QStringLiteral("CIM_ComputerSystem"));
    selectors.insert(QStringLiteral("SystemName"), QStringLiteral("Intel(r) AMT"));
    selectors.insert(QStringLiteral("CreationClassName"),
                     QStringLiteral("CIM_PowerManagementService"));
    QHash<QString, QString> params;
    params.insert(QStringLiteral("PowerState"), QString::number(powerState));
    // The ManagedElement parameter is a full EPR; we omit it because the
    // AMT implementation accepts the selectors on the service alone.
    const QByteArray env = buildInvokeEnvelope(QString::fromLatin1(kPowerServiceResource),
        QStringLiteral("RequestPowerStateChange"), selectors, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && r.returnValue == 0;
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("RequestPowerStateChange returned %1").arg(rv);
        },
        std::move(callback));
}

namespace {

// Lookup tables ported from legacy MeshCommander
// (`source/Commander.htm` ~line 4870, ported per CLAUDE.md guidance —
// the legacy tree was deleted but its decoder is still the canonical
// reference). Pipe-delimited there; flattened here for static linkage.

constexpr const char *kSystemEntityTypes[] = {
    "Unspecified", "Other", "Unknown", "Processor", "Disk", "Peripheral",
    "System management module", "System board", "Memory module",
    "Processor module", "Power supply", "Add in card", "Front panel board",
    "Back panel board", "Power system board", "Drive backplane",
    "System internal expansion board", "Other system board",
    "Processor board", "Power unit", "Power module",
    "Power management board", "Chassis back panel board", "System chassis",
    "Sub chassis", "Other chassis board", "Disk drive bay",
    "Peripheral bay", "Device bay", "Fan cooling", "Cooling unit",
    "Cable interconnect", "Memory device", "System management software",
    "BIOS", "Intel(r) ME", "System bus", "Group", "Intel(r) ME",
    "External environment", "Battery", "Processing blade",
    "Connectivity switch", "Processor/memory module", "I/O module",
    "Processor I/O module", "Management controller firmware",
    "IPMI channel", "PCI bus", "PCI express bus", "SCSI bus",
    "SATA/SAS bus", "Processor front side bus",
};

constexpr const char *kSystemFirmwareError[] = {
    "Unspecified.",
    "No system memory is physically installed in the system.",
    "No usable system memory, all installed memory has experienced an "
        "unrecoverable failure.",
    "Unrecoverable hard-disk/ATAPI/IDE device failure.",
    "Unrecoverable system-board failure.",
    "Unrecoverable diskette subsystem failure.",
    "Unrecoverable hard-disk controller failure.",
    "Unrecoverable PS/2 or USB keyboard failure.",
    "Removable boot media not found.",
    "Unrecoverable video controller failure.",
    "No video device detected.",
    "Firmware (BIOS) ROM corruption detected.",
    "CPU voltage mismatch (processors that share same supply have "
        "mismatched voltage requirements)",
    "CPU speed matching failure",
};

constexpr const char *kSystemFirmwareProgress[] = {
    "Unspecified.",
    "Memory initialization.",
    "Starting hard-disk initialization and test",
    "Secondary processor(s) initialization",
    "User authentication",
    "Entering BIOS setup",
    "USB resource configuration",
    "PCI resource configuration",
    "Option ROM initialization",
    "Video initialization",
    "Cache initialization",
    "SM Bus initialization",
    "Keyboard controller initialization",
    "Embedded controller/management controller initialization",
    "Docking station attachment",
    "Enabling docking station",
    "Docking station ejection",
    "Disabling docking station",
    "Calling operating system wake-up vector",
    "Starting operating system boot process",
    "Baseboard or motherboard initialization",
    "reserved",
    "Floppy initialization",
    "Keyboard test",
    "Pointing device test",
    "Primary processor initialization",
};

constexpr const char *kOcrProgressEvents[] = {
    "Boot parameters received from CSME",
    "CSME Boot Option % added successfully",
    "HTTPS URI name resolved",
    "HTTPS connected successfully",
    "HTTPSBoot download is completed",
    "Attempt to boot",
    "Exit boot services",
};

constexpr const char *kOcrErrorEvents[] = {
    "",
    "No network connection available",
    "Name resolution of URI failed",
    "Connect to URI failed",
    "OEM app not found at local URI",
    "HTTPS TLS Auth failed",
    "HTTPS Digest Auth failed",
    "Verified boot failed (bad image)",
    "HTTPS Boot File not found",
};

QString eventWatchdogStateName(int code)
{
    // Mirrors `watchdogStateName` above but returns QString so we can
    // fall through to `QString::number()` for unknown codes.
    switch (code) {
    case 1: return QStringLiteral("Not Started");
    case 2: return QStringLiteral("Stopped");
    case 4: return QStringLiteral("Running");
    case 8: return QStringLiteral("Expired");
    case 16: return QStringLiteral("Suspended");
    default: return QString::number(code);
    }
}

QString ocrSourceName(int code)
{
    switch (code) {
    case 2: return QStringLiteral("HTTPS");
    case 4: return QStringLiteral("Local PBA");
    case 8: return QStringLiteral("WinRE");
    default: return QString::number(code);
    }
}

QString formatHex2(int v) { return QStringLiteral("%1").arg(v & 0xff, 2, 16, QLatin1Char('0')); }

QString formatEventMessage(int sensorType, int offset, const std::array<int, 8> &d)
{
    if (sensorType == 15) {
        if (d[0] == 235) return QStringLiteral("Invalid Data");
        if (offset == 0) {
            const int idx = d[1];
            if (idx >= 0 && idx < int(std::size(kSystemFirmwareError)))
                return QString::fromLatin1(kSystemFirmwareError[idx]);
            return QStringLiteral("Unknown firmware error %1").arg(idx);
        }
        if (offset == 3) {
            if (d[0] == 170 && d[1] == 48) {
                const int e = d[2];
                if (e >= 0 && e < int(std::size(kOcrErrorEvents)))
                    return QStringLiteral("AMT One Click Recovery: %1")
                        .arg(QLatin1String(kOcrErrorEvents[e]));
            } else if (d[0] == 170 && d[1] == 64) {
                if (d[2] == 1) return QStringLiteral("Got an error erasing Device SSD");
                if (d[2] == 2) return QStringLiteral("Erasing Device TPM is not supported");
                if (d[2] == 3) return QStringLiteral("Reached Max Counter");
            }
            return QStringLiteral("OEM Specific Firmware Error event");
        }
        if (offset == 5) {
            if (d[0] == 170 && d[1] == 48) {
                if (d[2] == 1) {
                    return QStringLiteral("AMT One Click Recovery: CSME Boot Option %1:%2 "
                                          "added successfully")
                        .arg(d[3]).arg(ocrSourceName(d[3]));
                } else if (d[2] < 7) {
                    return QStringLiteral("AMT One Click Recovery: %1")
                        .arg(QLatin1String(kOcrProgressEvents[d[2]]));
                }
                return QStringLiteral("AMT One Click Recovery: Unknown progress event %1")
                    .arg(d[2]);
            }
            if (d[0] == 170 && d[1] == 64) {
                if (d[2] == 1) {
                    if (d[3] == 2) return QStringLiteral("Started erasing Device SSD");
                    if (d[3] == 3) return QStringLiteral("Started erasing Device TPM");
                    if (d[3] == 5)
                        return QStringLiteral("Started erasing Device BIOS Reload of "
                                              "Golden Config");
                }
                if (d[2] == 2) {
                    if (d[3] == 2)
                        return QStringLiteral("Erasing Device SSD ended successfully");
                    if (d[3] == 3)
                        return QStringLiteral("Erasing Device TPM ended successfully");
                    if (d[3] == 5)
                        return QStringLiteral("Erasing Device BIOS Reload of Golden "
                                              "Config ended successfully");
                }
                if (d[2] == 3) return QStringLiteral("Beginning Platform Erase");
                if (d[2] == 4) return QStringLiteral("Clear Reserved Parameters");
                if (d[2] == 5) return QStringLiteral("All setting decremented");
            }
            return QStringLiteral("OEM Specific Firmware Progress event");
        }
        const int idx = d[1];
        if (idx >= 0 && idx < int(std::size(kSystemFirmwareProgress)))
            return QString::fromLatin1(kSystemFirmwareProgress[idx]);
        return QStringLiteral("Unknown firmware progress %1").arg(idx);
    }

    if (sensorType == 18 && d[0] == 170) {
        return QStringLiteral("Agent watchdog %1%2%3%4-%5%6-... changed to %7")
            .arg(formatHex2(d[4]), formatHex2(d[3]), formatHex2(d[2]), formatHex2(d[1]),
                 formatHex2(d[6]), formatHex2(d[5]), eventWatchdogStateName(d[7]));
    }

    if (sensorType == 5 && offset == 0)
        return QStringLiteral("Case intrusion");

    if (sensorType == 192 && offset == 0 && d[0] == 170 && d[1] == 48) {
        if (d[2] == 0) return QStringLiteral("A remote Serial Over LAN session was established.");
        if (d[2] == 1)
            return QStringLiteral("Remote Serial Over LAN session finished. "
                                  "User control was restored.");
        if (d[2] == 2)
            return QStringLiteral("A remote IDE-Redirection session was established.");
        if (d[2] == 3)
            return QStringLiteral("Remote IDE-Redirection session finished. "
                                  "User control was restored.");
    }

    if (sensorType == 36) {
        const quint32 handle = (quint32(d[1]) << 24) | (quint32(d[2]) << 16)
                             | (quint32(d[3]) << 8)  |  quint32(d[4]);
        const QString nic = (d[0] == 0xAA) ? QStringLiteral("wired")
                                           : QStringLiteral("#%1").arg(d[0]);
        if (handle == 0xFFFFFFFDu)
            return QStringLiteral("All received packet filter was matched on %1 interface.")
                .arg(nic);
        if (handle == 0xFFFFFFFCu)
            return QStringLiteral("All outbound packet filter was matched on %1 interface.")
                .arg(nic);
        if (handle == 0xFFFFFFFAu)
            return QStringLiteral("Spoofed packet filter was matched on %1 interface.").arg(nic);
        return QStringLiteral("Filter %1 was matched on %2 interface.").arg(handle).arg(nic);
    }

    if (sensorType == 192) {
        if (d[2] == 0)
            return QStringLiteral("Security policy invoked. Some or all network traffic (TX) "
                                  "was stopped.");
        if (d[2] == 2)
            return QStringLiteral("Security policy invoked. Some or all network traffic (RX) "
                                  "was stopped.");
        return QStringLiteral("Security policy invoked.");
    }

    if (sensorType == 193) {
        if (d[0] == 0xAA && d[1] == 0x30 && d[2] == 0x00 && d[3] == 0x00)
            return QStringLiteral("User request for remote connection.");
        if (d[0] == 0xAA && d[1] == 0x20 && d[2] == 0x03 && d[3] == 0x01)
            return QStringLiteral("EAC error: attempt to get posture while NAC in Intel AMT "
                                  "is disabled.");
        if (d[0] == 0xAA && d[1] == 0x20 && d[2] == 0x04 && d[3] == 0x00)
            return QStringLiteral("HWA Error: general error");
    }

    if (sensorType == 6)
        return QStringLiteral("Authentication failed %1 times. The system may be under attack.")
            .arg(d[1] | (d[2] << 8));
    if (sensorType == 30) return QStringLiteral("No bootable media");
    if (sensorType == 32) return QStringLiteral("Operating system lockup or power interrupt");
    if (sensorType == 35) {
        if (d[0] == 64) return QStringLiteral("BIOS POST (Power On Self-Test) Watchdog Timeout.");
        return QStringLiteral("System boot failure");
    }
    if (sensorType == 37)
        return QStringLiteral("System firmware started (at least one CPU is properly executing).");

    return QStringLiteral("Unknown Sensor Type #%1").arg(sensorType);
}

QString entityLabel(int entity)
{
    if (entity >= 0 && entity < int(std::size(kSystemEntityTypes)))
        return QString::fromLatin1(kSystemEntityTypes[entity]);
    return {};
}

/// Walk the body of a GetRecords reply once, collecting every
/// `<…:RecordArray>` element's text in order. `findScalar` only returns
/// the first, so we need a multi-element walker here.
QStringList collectRecordArray(const QByteArray &bodyXml)
{
    QStringList out;
    QXmlStreamReader r(bodyXml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("RecordArray"))
            out.append(r.readElementText());
    }
    return out;
}

} // namespace

EventLogEntry decodeEventRecord(const QByteArray &raw)
{
    // Each AMT event record is a fixed 21-byte struct:
    //   [0..3]   timestamp (uint32 LE, seconds since epoch)
    //   [4]      DeviceAddress
    //   [5]      EventSensorType         (drives the message decoder)
    //   [6]      EventType
    //   [7]      EventOffset             (drives the message decoder)
    //   [8]      EventSourceType         (currently unused at decode time)
    //   [9]      EventSeverity           (CIM bucket, kept as string)
    //   [10]     SensorNumber
    //   [11]     Entity                  (used only for the label)
    //   [12]     EntityInstance
    //   [13..20] EventData[8]            (drives the message decoder)
    constexpr int kEventRecordSize = 21;

    EventLogEntry out;
    if (raw.size() < kEventRecordSize) return out;

    const auto b = reinterpret_cast<const quint8 *>(raw.constData());
    const quint32 ts = quint32(b[0]) | (quint32(b[1]) << 8)
                     | (quint32(b[2]) << 16) | (quint32(b[3]) << 24);
    if (ts == 0 || ts == 0xFFFFFFFFu) return out;

    // AMT stores the device's wall-clock seconds-since-epoch. Display
    // the raw value (formatted in UTC) so it matches whatever clock the
    // operator set on the box — legacy MeshCommander does the same
    // trick via a `getTimezoneOffset()` shim.
    const QDateTime when = QDateTime::fromSecsSinceEpoch(ts, QTimeZone::utc());
    out.timestamp = when.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

    const int sensorType = b[5];
    const int offset     = b[7];
    const int severity   = b[9];
    const int entity     = b[11];
    std::array<int, 8> data{};
    for (int i = 0; i < 8; ++i) data[i] = b[13 + i];

    out.severity    = QString::number(severity);
    out.message     = formatEventMessage(sensorType, offset, data);
    out.entityLabel = entityLabel(entity);
    // recordId is filled in by the enumerator with the running index;
    // a decoder-only call leaves it empty.
    return out;
}

void enumerateEventLog(WsmanClient *client,
                       std::function<void(EventLogResult)> callback)
{
    struct Acc { QList<EventLogEntry> items; };
    auto acc = std::make_shared<Acc>();
    auto cb = std::make_shared<std::function<void(EventLogResult)>>(std::move(callback));
    auto done = std::make_shared<std::function<void(QString)>>();

    *done = [acc, cb](QString error) {
        EventLogResult r;
        r.ok = error.isEmpty();
        r.error = std::move(error);
        r.entries = std::move(acc->items);
        (*cb)(std::move(r));
    };

    if (client == nullptr) { (*done)(QStringLiteral("client is null")); return; }

    // Firmware caps the log at ~390 entries; the cap below is a
    // belt-and-braces guard against a misbehaving box that never sets
    // NoMoreRecords.
    constexpr int kMaxRecords = 8000;
    constexpr int kBatchSize  = 390;

    const QString resource = QString::fromLatin1(kMessageLogResource);
    const QString endpoint = client->endpoint().toString();

    auto getRecordsStep = std::make_shared<std::function<void(const QString &)>>();
    *getRecordsStep = [client, acc, getRecordsStep, done, resource, endpoint](
                          const QString &iterId) mutable {
        QHash<QString, QString> params;
        params.insert(QStringLiteral("IterationIdentifier"), iterId);
        params.insert(QStringLiteral("MaxReadRecords"), QString::number(kBatchSize));
        const QByteArray env = buildInvokeEnvelope(resource, QStringLiteral("GetRecords"),
                                                    {}, params, endpoint, newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc, getRecordsStep, done]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                if (err) { (*done)(errString); return; }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) { (*done)(soap.fault); return; }
                const QString rv = findScalar(soap.bodyXml, QStringLiteral("ReturnValue"));
                if (rv != QStringLiteral("0")) {
                    (*done)(QStringLiteral("GetRecords returned %1").arg(rv));
                    return;
                }
                const QStringList records = collectRecordArray(soap.bodyXml);
                for (const QString &b64 : records) {
                    const QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
                    EventLogEntry e = decodeEventRecord(raw);
                    if (e.timestamp.isEmpty()) continue;
                    e.recordId = QString::number(acc->items.size() + 1);
                    acc->items.append(std::move(e));
                    if (acc->items.size() >= kMaxRecords) {
                        (*done)({});
                        return;
                    }
                }
                const QString noMore = findScalar(soap.bodyXml,
                                                  QStringLiteral("NoMoreRecords"));
                if (noMore.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
                    (*done)({});
                    return;
                }
                const QString nextId = findScalar(soap.bodyXml,
                                                  QStringLiteral("IterationIdentifier"));
                if (nextId.isEmpty()) { (*done)({}); return; }
                (*getRecordsStep)(nextId);
            });
    };

    const QByteArray env = buildInvokeEnvelope(resource,
                                                QStringLiteral("PositionToFirstRecord"),
                                                {}, {}, endpoint, newMessageId());
    WsmanReply *reply = client->sendEnvelope(env);
    QObject::connect(reply, &WsmanReply::finished, client,
        [reply, getRecordsStep, done]() mutable {
            const QByteArray body = reply->readAll();
            const auto err = reply->hasError();
            const auto errString = reply->errorString();
            reply->deleteLater();
            if (err) { (*done)(errString); return; }
            const SoapResponse soap = parseResponse(body);
            if (soap.isFault()) { (*done)(soap.fault); return; }
            const QString rv = findScalar(soap.bodyXml, QStringLiteral("ReturnValue"));
            if (rv != QStringLiteral("0")) {
                (*done)(QStringLiteral("PositionToFirstRecord returned %1").arg(rv));
                return;
            }
            const QString iterId = findScalar(soap.bodyXml,
                                              QStringLiteral("IterationIdentifier"));
            if (iterId.isEmpty()) {
                (*done)(QStringLiteral("PositionToFirstRecord had no IterationIdentifier"));
                return;
            }
            (*getRecordsStep)(iterId);
        });
}

QString realmName(int index)
{
    // Ported verbatim from `legacy/source/Commander.htm` line ~4870
    // `RealmNames`. The empty slots are reserved by AMT.
    static const char *const kTable[] = {
        "",                          // 0
        "",                          // 1
        "Redirection",               // 2
        "",                          // 3 — sentinel "Administrator", handled separately
        "Hardware Asset",            // 4
        "Remote Control",            // 5
        "Storage",                   // 6
        "Event Manager",             // 7
        "Storage Admin",             // 8
        "Agent Presence Local",      // 9
        "Agent Presence Remote",     // 10
        "Circuit Breaker",           // 11
        "Network Time",              // 12
        "General Information",       // 13
        "Firmware Update",           // 14
        "EIT",                       // 15
        "LocalUN",                   // 16
        "Endpoint Access Control",   // 17
        "Endpoint Access Control Admin", // 18
        "Event Log Reader",          // 19
        "Audit Log",                 // 20
        "ACL Realm",                 // 21
        "",                          // 22
        "",                          // 23
        "Local System",              // 24
    };
    if (index < 0 || index >= int(sizeof(kTable) / sizeof(kTable[0])))
        return {};
    return QString::fromLatin1(kTable[index]);
}

QString accessPermissionLabel(int code)
{
    switch (code) {
    case 0:   return QStringLiteral("Local only");
    case 1:   return QStringLiteral("Network only");
    case 2:   return QStringLiteral("All (Local & Network)");
    case 999: return QStringLiteral("Administrator");
    default:  return QStringLiteral("Permission %1").arg(code);
    }
}

namespace {

/// Drive the rich legacy user-account read:
///   GetAdminAclEntry → synthetic Handle=-1 row
///   EnumerateUserAclEntries(StartIndex) → list of Handles
///     For each: GetUserAclEntryEx + GetAclEnabledState
QList<int> parseAclHandles(const QByteArray &bodyXml)
{
    QList<int> out;
    QXmlStreamReader r(bodyXml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("Handles")) {
            const QString s = r.readElementText();
            bool conv = false;
            const int v = s.toInt(&conv);
            if (conv) out.append(v);
        }
    }
    return out;
}

QList<int> parseUserRealms(const QByteArray &bodyXml)
{
    QList<int> out;
    QXmlStreamReader r(bodyXml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("Realms")) {
            bool conv = false;
            const int v = r.readElementText().toInt(&conv);
            if (conv) out.append(v);
        }
    }
    return out;
}

} // namespace

void enumerateUserAccounts(WsmanClient *client,
                            std::function<void(UserAccountsResult)> callback)
{
    struct Acc {
        QHash<int, UserAccount> byHandle;   // -1 reserved for admin entry
        std::function<void(UserAccountsResult)> cb;
        int pendingPerHandle = 0;
        bool adminDone = false;
        bool enumDone  = false;
        bool fired = false;
        QString error;
        void maybeFire() {
            if (fired) return;
            if (!adminDone || !enumDone) return;
            if (pendingPerHandle > 0) return;
            fired = true;
            UserAccountsResult r;
            r.ok = error.isEmpty();
            r.error = error;
            QList<int> keys = byHandle.keys();
            std::sort(keys.begin(), keys.end()); // admin (-1) first.
            for (int k : keys) r.accounts.append(byHandle.value(k));
            cb(std::move(r));
        }
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    if (client == nullptr) {
        acc->error = QStringLiteral("client is null");
        acc->adminDone = acc->enumDone = true;
        acc->maybeFire();
        return;
    }

    // --- GetAdminAclEntry ----------------------------------------
    {
        const QByteArray env = buildInvokeEnvelope(
            QString::fromLatin1(kAuthorizationServiceResource),
            QStringLiteral("GetAdminAclEntry"), {}, {},
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                reply->deleteLater();
                if (!err) {
                    const SoapResponse soap = parseResponse(body);
                    if (!soap.isFault()) {
                        const QString u = findScalar(soap.bodyXml,
                                                      QStringLiteral("Username"));
                        if (!u.isEmpty()) {
                            UserAccount admin;
                            admin.handle = -1;
                            admin.digestUsername = u;
                            admin.name = u;
                            admin.accessPermission = 999;
                            admin.enabled = true;
                            admin.hidden = (u.startsWith(QStringLiteral("$$")));
                            acc->byHandle.insert(-1, admin);
                        }
                    }
                }
                acc->adminDone = true;
                acc->maybeFire();
            });
    }

    // --- EnumerateUserAclEntries (paged) ------------------------
    auto enumStep = std::make_shared<std::function<void(int)>>();
    *enumStep = [client, acc, enumStep](int startIndex) mutable {
        QHash<QString, QString> params;
        params.insert(QStringLiteral("StartIndex"),
                      QString::number(startIndex));
        const QByteArray env = buildInvokeEnvelope(
            QString::fromLatin1(kAuthorizationServiceResource),
            QStringLiteral("EnumerateUserAclEntries"), {}, params,
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc, enumStep, startIndex, client]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                if (err) {
                    if (acc->error.isEmpty()) acc->error = errString;
                    acc->enumDone = true;
                    acc->maybeFire();
                    return;
                }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) {
                    if (acc->error.isEmpty()) acc->error = soap.fault;
                    acc->enumDone = true;
                    acc->maybeFire();
                    return;
                }
                const QList<int> handles = parseAclHandles(soap.bodyXml);
                // Kick a GetUserAclEntryEx + GetAclEnabledState for
                // each handle. Each pair increments pendingPerHandle
                // by 2; both decrements unblock maybeFire().
                for (int h : handles) {
                    acc->pendingPerHandle += 2;
                    UserAccount stub;
                    stub.handle = h;
                    acc->byHandle.insert(h, stub);

                    QHash<QString, QString> p1;
                    p1.insert(QStringLiteral("Handle"), QString::number(h));
                    const QByteArray e1 = buildInvokeEnvelope(
                        QString::fromLatin1(kAuthorizationServiceResource),
                        QStringLiteral("GetUserAclEntryEx"), {}, p1,
                        client->endpoint().toString(), newMessageId());
                    WsmanReply *r1 = client->sendEnvelope(e1);
                    QObject::connect(r1, &WsmanReply::finished, client,
                        [r1, acc, h]() mutable {
                            const QByteArray b = r1->readAll();
                            const auto er = r1->hasError();
                            r1->deleteLater();
                            if (!er) {
                                const SoapResponse s = parseResponse(b);
                                if (!s.isFault() && acc->byHandle.contains(h)) {
                                    UserAccount u = acc->byHandle.value(h);
                                    u.digestUsername =
                                        findScalar(s.bodyXml,
                                                    QStringLiteral("DigestUsername"));
                                    u.kerberosUserSidB64 =
                                        findScalar(s.bodyXml,
                                                    QStringLiteral("KerberosUserSid"));
                                    bool conv = false;
                                    const int ap = findScalar(s.bodyXml,
                                        QStringLiteral("AccessPermission")).toInt(&conv);
                                    if (conv) u.accessPermission = ap;
                                    u.realms = parseUserRealms(s.bodyXml);
                                    if (!u.digestUsername.isEmpty()) {
                                        u.name = u.digestUsername;
                                        u.hidden = u.digestUsername.startsWith(
                                            QStringLiteral("$$"));
                                    } else {
                                        // Decode Kerberos SID lazily —
                                        // keep the raw b64 for now and
                                        // surface it to the controller,
                                        // which formats SidString.
                                        u.name = QStringLiteral("(Kerberos)");
                                    }
                                    acc->byHandle.insert(h, u);
                                }
                            }
                            acc->pendingPerHandle--;
                            acc->maybeFire();
                        });

                    const QByteArray e2 = buildInvokeEnvelope(
                        QString::fromLatin1(kAuthorizationServiceResource),
                        QStringLiteral("GetAclEnabledState"), {}, p1,
                        client->endpoint().toString(), newMessageId());
                    WsmanReply *r2 = client->sendEnvelope(e2);
                    QObject::connect(r2, &WsmanReply::finished, client,
                        [r2, acc, h]() mutable {
                            const QByteArray b = r2->readAll();
                            const auto er = r2->hasError();
                            r2->deleteLater();
                            if (!er) {
                                const SoapResponse s = parseResponse(b);
                                if (!s.isFault() && acc->byHandle.contains(h)) {
                                    UserAccount u = acc->byHandle.value(h);
                                    const QString en = findScalar(
                                        s.bodyXml, QStringLiteral("Enabled"));
                                    u.enabled = (en == QStringLiteral("true"));
                                    acc->byHandle.insert(h, u);
                                }
                            }
                            acc->pendingPerHandle--;
                            acc->maybeFire();
                        });
                }

                // Continuation: AMT's `EnumerateUserAclEntries` returns
                // a maximum of 50 handles per call. If we got a full
                // page, page forward. With <50 entries we're done.
                if (handles.size() >= 50) {
                    (*enumStep)(startIndex + handles.size());
                } else {
                    acc->enumDone = true;
                    acc->maybeFire();
                }
            });
    };
    (*enumStep)(1);
}

QString computeDigestPassword(const QString &username, const QString &realm,
                              const QString &plaintext)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(username.toUtf8());
    md5.addData(QByteArrayLiteral(":"));
    md5.addData(realm.toUtf8());
    md5.addData(QByteArrayLiteral(":"));
    md5.addData(plaintext.toUtf8());
    return QString::fromLatin1(md5.result().toBase64());
}

namespace {

/// Common reply parser for the five `AMT_AuthorizationService` write
/// invokes. They all return a `ReturnValue` scalar where 0 = success
/// and any other code is an error AMT enumerates by name; surface the
/// raw number so the operator sees something actionable in
/// `lastError`.
auto aclReturnValueExtractor(const QString &what)
{
    return [what](const QByteArray &body, InvokeResult &r) {
        const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
        r.returnValue = rv.toInt();
        r.ok = (r.returnValue == 0);
        if (!r.ok) {
            r.error = QStringLiteral("%1 returned %2").arg(what,
                rv.isEmpty() ? QStringLiteral("(no ReturnValue)") : rv);
        }
    };
}

QList<QPair<QString, QString>> realmsAsRepeatedParams(const QList<int> &realms)
{
    QList<QPair<QString, QString>> out;
    out.reserve(realms.size());
    for (int r : realms) {
        out.append({ QStringLiteral("Realms"), QString::number(r) });
    }
    return out;
}

} // namespace

void addUserAclEntryEx(WsmanClient *client, const QString &digestUsername,
                       const QString &digestPassword, int accessPermission,
                       const QList<int> &realms,
                       std::function<void(InvokeResult)> callback)
{
    // DigestUsername / DigestPassword / AccessPermission / Realms…
    // The XML order matches the legacy MeshCommander invocation; AMT
    // is order-tolerant but matching the legacy shape avoids surprises
    // with older firmware.
    QList<QPair<QString, QString>> params;
    params.append({ QStringLiteral("DigestUsername"), digestUsername });
    params.append({ QStringLiteral("DigestPassword"), digestPassword });
    params.append({ QStringLiteral("AccessPermission"),
                    QString::number(accessPermission) });
    params.append(realmsAsRepeatedParams(realms));

    const QByteArray env = buildInvokeEnvelopeOrdered(
        QString::fromLatin1(kAuthorizationServiceResource),
        QStringLiteral("AddUserAclEntryEx"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        aclReturnValueExtractor(QStringLiteral("AddUserAclEntryEx")),
        std::move(callback));
}

void updateUserAclEntryEx(WsmanClient *client, int handle,
                          const UserAclEntryPatch &patch,
                          std::function<void(InvokeResult)> callback)
{
    QList<QPair<QString, QString>> params;
    params.append({ QStringLiteral("Handle"), QString::number(handle) });
    if (patch.setDigestUsername)
        params.append({ QStringLiteral("DigestUsername"), patch.digestUsername });
    if (patch.setDigestPassword)
        params.append({ QStringLiteral("DigestPassword"), patch.digestPassword });
    if (patch.setAccessPermission) {
        params.append({ QStringLiteral("AccessPermission"),
                        QString::number(patch.accessPermission) });
    }
    if (patch.setRealms)
        params.append(realmsAsRepeatedParams(patch.realms));

    const QByteArray env = buildInvokeEnvelopeOrdered(
        QString::fromLatin1(kAuthorizationServiceResource),
        QStringLiteral("UpdateUserAclEntryEx"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        aclReturnValueExtractor(QStringLiteral("UpdateUserAclEntryEx")),
        std::move(callback));
}

void removeUserAclEntry(WsmanClient *client, int handle,
                        std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("Handle"), QString::number(handle));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kAuthorizationServiceResource),
        QStringLiteral("RemoveUserAclEntry"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        aclReturnValueExtractor(QStringLiteral("RemoveUserAclEntry")),
        std::move(callback));
}

void setAclEnabledState(WsmanClient *client, int handle, bool enabled,
                        std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("Handle"), QString::number(handle));
    params.insert(QStringLiteral("Enabled"),
                  enabled ? QStringLiteral("true") : QStringLiteral("false"));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kAuthorizationServiceResource),
        QStringLiteral("SetAclEnabledState"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        aclReturnValueExtractor(QStringLiteral("SetAclEnabledState")),
        std::move(callback));
}

void setAdminAclEntryEx(WsmanClient *client, const QString &username,
                        const QString &digestPassword,
                        std::function<void(InvokeResult)> callback)
{
    QList<QPair<QString, QString>> params;
    params.append({ QStringLiteral("Username"), username });
    params.append({ QStringLiteral("DigestPassword"), digestPassword });
    const QByteArray env = buildInvokeEnvelopeOrdered(
        QString::fromLatin1(kAuthorizationServiceResource),
        QStringLiteral("SetAdminAclEntryEx"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        aclReturnValueExtractor(QStringLiteral("SetAdminAclEntryEx")),
        std::move(callback));
}

namespace {

QHash<QString, QString> optInServiceSelectors()
{
    QHash<QString, QString> sel;
    sel.insert(QStringLiteral("Name"),
               QStringLiteral("Intel(r) AMT OptIn Service"));
    sel.insert(QStringLiteral("SystemCreationClassName"),
               QStringLiteral("CIM_ComputerSystem"));
    sel.insert(QStringLiteral("SystemName"), QStringLiteral("Intel(r) AMT"));
    sel.insert(QStringLiteral("CreationClassName"),
               QStringLiteral("IPS_OptInService"));
    return sel;
}

} // namespace

void getOptInStatus(WsmanClient *client,
                    std::function<void(OptInServiceResult)> callback)
{
    // Two-step: read IPS_OptInService for the runtime fields, then
    // IPS_KVMRedirectionSettingData for the persisted policy. We chain
    // them so the caller gets one merged result.
    const QString to = client ? client->endpoint().toString() : QString();
    auto cb = std::make_shared<std::function<void(OptInServiceResult)>>(std::move(callback));
    auto partial = std::make_shared<OptInServiceResult>();

    const QByteArray svcEnv = buildGetEnvelope(
        QString::fromLatin1(kOptInServiceResource), {}, to, newMessageId());
    auto *reply = client->sendEnvelope(svcEnv);
    QObject::connect(reply, &WsmanReply::finished, client,
        [reply, client, partial, cb, to]() mutable {
            const QByteArray body = reply->readAll();
            const bool err = reply->hasError();
            const QString errStr = reply->errorString();
            reply->deleteLater();
            if (err) {
                partial->error = errStr;
                (*cb)(*partial);
                return;
            }
            const SoapResponse soap = parseResponse(body);
            if (soap.isFault()) {
                partial->error = soap.fault;
                (*cb)(*partial);
                return;
            }
            const QString req = findScalar(soap.bodyXml,
                                            QStringLiteral("OptInRequired"));
            const QString st = findScalar(soap.bodyXml,
                                           QStringLiteral("OptInState"));
            const QString canMod = findScalar(
                soap.bodyXml, QStringLiteral("CanModifyOptInPolicy"));
            // `OptInRequired` is an enum in newer firmware (0 / 1 / 0xFF
            // / etc) and a bool in older. Treat any non-zero as "yes".
            partial->optInRequired = !req.isEmpty() && req != QStringLiteral("0")
                                     && req.toLower() != QStringLiteral("false");
            partial->optInState = st.toInt();
            partial->canModifyOptInPolicy = canMod.toLower() == QStringLiteral("true")
                                            || canMod == QStringLiteral("1");

            // Now fetch the KVM setting data.
            const QByteArray kvmEnv = buildGetEnvelope(
                QString::fromLatin1(kKvmRedirectionSettingDataResource),
                {}, to, newMessageId());
            auto *r2 = client->sendEnvelope(kvmEnv);
            QObject::connect(r2, &WsmanReply::finished, client,
                [r2, partial, cb]() {
                    const QByteArray body2 = r2->readAll();
                    const bool err2 = r2->hasError();
                    const QString errStr2 = r2->errorString();
                    r2->deleteLater();
                    if (err2) {
                        // Older firmware may not expose this resource;
                        // partial result still useful so don't blame.
                        partial->kvmOptInPolicy = false;
                    } else {
                        const SoapResponse s = parseResponse(body2);
                        if (!s.isFault()) {
                            const QString pol = findScalar(
                                s.bodyXml, QStringLiteral("OptInPolicy"));
                            partial->kvmOptInPolicy =
                                pol.toLower() == QStringLiteral("true")
                                || pol == QStringLiteral("1");
                            // OptInPolicyTimeout lives on the same
                            // IPS_KVMRedirectionSettingData class — the
                            // firmware applies it to every consent
                            // session, not just KVM. Drives the
                            // countdown in the PIN-entry dialog (#171).
                            bool conv = false;
                            const int t = findScalar(
                                s.bodyXml,
                                QStringLiteral("OptInPolicyTimeout")).toInt(&conv);
                            if (conv) partial->optInPolicyTimeoutSec = t;
                            // KVM settings fields (#175).
                            const QString port = findScalar(s.bodyXml,
                                QStringLiteral("Is5900PortEnabled"));
                            partial->is5900PortEnabled =
                                port.toLower() == QStringLiteral("true")
                                || port == QStringLiteral("1");
                            conv = false;
                            const int sto = findScalar(s.bodyXml,
                                QStringLiteral("SessionTimeout")).toInt(&conv);
                            if (conv) partial->sessionTimeoutMinutes = sto;
                            const QString grey = findScalar(s.bodyXml,
                                QStringLiteral("GreyscalePixelFormatRequested"));
                            partial->greyscalePixelFormatRequested =
                                grey.toLower() == QStringLiteral("true")
                                || grey == QStringLiteral("1");
                        }
                    }
                    partial->ok = true;
                    (*cb)(*partial);
                });
        });
}

void startOptIn(WsmanClient *client, std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kOptInServiceResource),
        QStringLiteral("StartOptIn"),
        optInServiceSelectors(), {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            r.returnValue = rv.toInt();
            r.ok = (r.returnValue == 0);
            if (!r.ok)
                r.error = QStringLiteral("StartOptIn returned %1").arg(rv);
        },
        std::move(callback));
}

void sendOptInCode(WsmanClient *client, quint32 code,
                   std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("OptInCode"), QString::number(code));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kOptInServiceResource),
        QStringLiteral("SendOptInCode"),
        optInServiceSelectors(), params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            r.returnValue = rv.toInt();
            r.ok = (r.returnValue == 0);
            if (!r.ok)
                r.error = QStringLiteral("SendOptInCode returned %1").arg(rv);
        },
        std::move(callback));
}

void cancelOptIn(WsmanClient *client, std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kOptInServiceResource),
        QStringLiteral("CancelOptIn"),
        optInServiceSelectors(), {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            r.returnValue = rv.toInt();
            r.ok = (r.returnValue == 0);
            if (!r.ok)
                r.error = QStringLiteral("CancelOptIn returned %1").arg(rv);
        },
        std::move(callback));
}

void setKvmOptInPolicy(WsmanClient *client, bool policyRequired,
                       std::function<void(InvokeResult)> callback)
{
    // Read-modify-write: AMT Put requires the entire SettingData record
    // round-tripped, so we Get first, flip the single field, then Put.
    const QString to = client ? client->endpoint().toString() : QString();
    auto cb = std::make_shared<std::function<void(InvokeResult)>>(std::move(callback));

    const QByteArray getEnv = buildGetEnvelope(
        QString::fromLatin1(kKvmRedirectionSettingDataResource),
        {}, to, newMessageId());
    auto *reply = client->sendEnvelope(getEnv);
    QObject::connect(reply, &WsmanReply::finished, client,
        [reply, client, policyRequired, to, cb]() {
            const QByteArray body = reply->readAll();
            const bool err = reply->hasError();
            const QString errStr = reply->errorString();
            reply->deleteLater();
            InvokeResult r;
            if (err) { r.error = errStr; (*cb)(r); return; }
            const SoapResponse soap = parseResponse(body);
            if (soap.isFault()) { r.error = soap.fault; (*cb)(r); return; }

            // Echo back every scalar the firmware sent us, swapping
            // OptInPolicy. Anything we don't know about gets passed
            // through verbatim so the firmware's validator is happy.
            QHash<QString, QString> props;
            const QList<QString> keys = {
                QStringLiteral("EnabledByMEBx"),
                QStringLiteral("Is5900PortEnabled"),
                QStringLiteral("OptInPolicy"),
                QStringLiteral("OptInPolicyTimeout"),
                QStringLiteral("SessionTimeout"),
                QStringLiteral("RFBPassword"),
                QStringLiteral("DefaultScreen"),
                QStringLiteral("InitialDecimationModeForLowResScreens"),
                QStringLiteral("GreyscalePixelFormatSupported"),
                QStringLiteral("BackToBackFeatureSupported"),
            };
            for (const QString &k : keys) {
                const QString v = findScalar(soap.bodyXml, k);
                if (!v.isEmpty()) props.insert(k, v);
            }
            props.insert(QStringLiteral("OptInPolicy"),
                         policyRequired ? QStringLiteral("true") : QStringLiteral("false"));

            // The IPS_KVMRedirectionSettingData InstanceID selector is
            // a fixed string the firmware advertises.
            QHash<QString, QString> sel;
            sel.insert(QStringLiteral("InstanceID"),
                       QStringLiteral("Intel(r) KVM Redirection Settings"));

            const QByteArray putEnv = buildPutEnvelope(
                QString::fromLatin1(kKvmRedirectionSettingDataResource),
                QStringLiteral("IPS_KVMRedirectionSettingData"),
                sel, props, to, newMessageId());

            auto *r2 = client->sendEnvelope(putEnv);
            QObject::connect(r2, &WsmanReply::finished, client,
                [r2, cb]() {
                    const QByteArray body2 = r2->readAll();
                    const bool err2 = r2->hasError();
                    const QString errStr2 = r2->errorString();
                    r2->deleteLater();
                    InvokeResult res;
                    if (err2) { res.error = errStr2; (*cb)(res); return; }
                    const SoapResponse s = parseResponse(body2);
                    if (s.isFault()) {
                        // AMT returns a SOAP fault when the login lacks
                        // permission. The error string is what we surface
                        // to the UI ("not authorized" or similar).
                        res.error = s.fault;
                        (*cb)(res);
                        return;
                    }
                    res.ok = true;
                    res.returnValue = 0;
                    (*cb)(res);
                });
        });
}

void setKvmSettings(WsmanClient *client, const KvmSettingsPatch &patch,
                    std::function<void(InvokeResult)> callback)
{
    // Same read-modify-write dance as setKvmOptInPolicy, but with a
    // partial-update patch. The Put requires the full record echoed
    // back; any field the patch doesn't override gets passed through
    // verbatim. RFBPassword is special — the firmware never echoes it
    // back, so we only include it on the Put when the patch sets it.
    const QString to = client ? client->endpoint().toString() : QString();
    auto cb = std::make_shared<std::function<void(InvokeResult)>>(std::move(callback));
    auto p = std::make_shared<KvmSettingsPatch>(patch);

    const QByteArray getEnv = buildGetEnvelope(
        QString::fromLatin1(kKvmRedirectionSettingDataResource),
        {}, to, newMessageId());
    auto *reply = client->sendEnvelope(getEnv);
    QObject::connect(reply, &WsmanReply::finished, client,
        [reply, client, to, cb, p]() {
            const QByteArray body = reply->readAll();
            const bool err = reply->hasError();
            const QString errStr = reply->errorString();
            reply->deleteLater();
            InvokeResult r;
            if (err) { r.error = errStr; (*cb)(r); return; }
            const SoapResponse soap = parseResponse(body);
            if (soap.isFault()) { r.error = soap.fault; (*cb)(r); return; }

            QHash<QString, QString> props;
            const QList<QString> echoKeys = {
                QStringLiteral("EnabledByMEBx"),
                QStringLiteral("Is5900PortEnabled"),
                QStringLiteral("OptInPolicy"),
                QStringLiteral("OptInPolicyTimeout"),
                QStringLiteral("SessionTimeout"),
                QStringLiteral("DefaultScreen"),
                QStringLiteral("InitialDecimationModeForLowResScreens"),
                QStringLiteral("GreyscalePixelFormatRequested"),
                QStringLiteral("BackToBackFeatureSupported"),
            };
            for (const QString &k : echoKeys) {
                const QString v = findScalar(soap.bodyXml, k);
                if (!v.isEmpty()) props.insert(k, v);
            }

            if (p->setOptInPolicy)
                props.insert(QStringLiteral("OptInPolicy"),
                             p->optInPolicy ? QStringLiteral("true")
                                            : QStringLiteral("false"));
            if (p->setIs5900PortEnabled)
                props.insert(QStringLiteral("Is5900PortEnabled"),
                             p->is5900PortEnabled ? QStringLiteral("true")
                                                  : QStringLiteral("false"));
            if (p->setSessionTimeoutMinutes)
                props.insert(QStringLiteral("SessionTimeout"),
                             QString::number(p->sessionTimeoutMinutes));
            if (p->setGreyscaleRequested)
                props.insert(QStringLiteral("GreyscalePixelFormatRequested"),
                             p->greyscaleRequested ? QStringLiteral("true")
                                                   : QStringLiteral("false"));
            if (p->setRfbPassword)
                props.insert(QStringLiteral("RFBPassword"), p->rfbPassword);

            QHash<QString, QString> sel;
            sel.insert(QStringLiteral("InstanceID"),
                       QStringLiteral("Intel(r) KVM Redirection Settings"));

            const QByteArray putEnv = buildPutEnvelope(
                QString::fromLatin1(kKvmRedirectionSettingDataResource),
                QStringLiteral("IPS_KVMRedirectionSettingData"),
                sel, props, to, newMessageId());

            auto *r2 = client->sendEnvelope(putEnv);
            QObject::connect(r2, &WsmanReply::finished, client,
                [r2, cb]() {
                    const QByteArray body2 = r2->readAll();
                    const bool err2 = r2->hasError();
                    const QString errStr2 = r2->errorString();
                    r2->deleteLater();
                    InvokeResult res;
                    if (err2) { res.error = errStr2; (*cb)(res); return; }
                    const SoapResponse s = parseResponse(body2);
                    if (s.isFault()) { res.error = s.fault; (*cb)(res); return; }
                    res.ok = true;
                    res.returnValue = 0;
                    (*cb)(res);
                });
        });
}

void setKvmRedirectionEnabled(WsmanClient *client, bool enabled,
                              std::function<void(InvokeResult)> callback)
{
    // CIM_KVMRedirectionSAP.RequestStateChange — RequestedState 2 = Enabled,
    // 3 = Disabled per the DMTF state-machine enum.
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"),
                     QStringLiteral("Intel(r) KVM Redirection SAP"));
    selectors.insert(QStringLiteral("SystemCreationClassName"),
                     QStringLiteral("CIM_ComputerSystem"));
    selectors.insert(QStringLiteral("SystemName"), QStringLiteral("Intel(r) AMT"));
    selectors.insert(QStringLiteral("CreationClassName"),
                     QStringLiteral("CIM_KVMRedirectionSAP"));
    QHash<QString, QString> params;
    params.insert(QStringLiteral("RequestedState"),
                  QString::number(enabled ? 2 : 3));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kKvmRedirectionSapResource),
        QStringLiteral("RequestStateChange"),
        selectors, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            r.ok = conv && (r.returnValue == 0 || r.returnValue == 4096);
            if (!r.ok && r.error.isEmpty())
                r.error = QStringLiteral("RequestStateChange returned %1").arg(rv);
        },
        std::move(callback));
}

namespace {

// --- DMTF lookup tables (subset matching legacy/source/Commander.htm) -

QString dmtfCpuStatusLabel(int code)
{
    switch (code) {
    case 0: return QStringLiteral("Unknown");
    case 1: return QStringLiteral("Enabled");
    case 2: return QStringLiteral("Disabled by User");
    case 3: return QStringLiteral("Disabled by BIOS (POST error)");
    case 4: return QStringLiteral("Idle");
    case 5: return QStringLiteral("Other");
    default: return QStringLiteral("CPU status %1").arg(code);
    }
}

QString dmtfCpuFamilyLabel(int code)
{
    // The DMTF spec has hundreds of codes; the legacy app only had the
    // small i-series subset because that's what AMT-managed boxes
    // typically reported in 2015. Fall back to the raw code for the
    // ones we don't know — still useful, just less pretty.
    switch (code) {
    case 191: return QStringLiteral("Intel® Core™ 2 Duo Processor");
    case 192: return QStringLiteral("Intel® Core™ 2 Solo Processor");
    case 193: return QStringLiteral("Intel® Core™ 2 Extreme Processor");
    case 194: return QStringLiteral("Intel® Core™ 2 Quad Processor");
    case 195: return QStringLiteral("Intel® Core™ 2 Extreme Mobile Processor");
    case 196: return QStringLiteral("Intel® Core™ 2 Duo Mobile Processor");
    case 197: return QStringLiteral("Intel® Core™ 2 Solo Mobile Processor");
    case 198: return QStringLiteral("Intel® Core™ i7 Processor");
    case 199: return QStringLiteral("Dual-Core Intel® Celeron® Processor");
    default:
        return QStringLiteral("Family 0x%1")
                    .arg(QString::number(code, 16).toUpper());
    }
}

QString dmtfMemFormFactorLabel(int code)
{
    static const char *kTable[] = {
        "", "Other", "Unknown", "SIMM", "SIP", "Chip", "DIP", "ZIP",
        "Proprietary Card", "DIMM", "TSOP", "Row of chips", "RIMM",
        "SODIMM", "SRIMM", "FB-DIMM"
    };
    if (code >= 0 && code < int(sizeof(kTable) / sizeof(kTable[0])))
        return QString::fromLatin1(kTable[code]);
    return QStringLiteral("Form 0x%1").arg(QString::number(code, 16).toUpper());
}

QString dmtfMemTypeLabel(int code)
{
    static const char *kTable[] = {
        "Unknown", "Other", "DRAM", "Synchronous DRAM", "Cache DRAM",
        "EDO", "EDRAM", "VRAM", "SRAM", "RAM", "ROM", "Flash", "EEPROM",
        "FEPROM", "EPROM", "CDRAM", "3DRAM", "SDRAM", "SGRAM", "RDRAM",
        "DDR", "DDR-2", "BRAM", "FB-DIMM", "DDR3", "FBD2", "DDR4",
        "LPDDR", "LPDDR2", "LPDDR3", "LPDDR4"
    };
    if (code >= 0 && code < int(sizeof(kTable) / sizeof(kTable[0])))
        return QString::fromLatin1(kTable[code]);
    return QStringLiteral("Type 0x%1").arg(QString::number(code, 16).toUpper());
}

QString dmtfBatteryChemistryLabel(int code)
{
    static const char *kTable[] = {
        "Other", "Unknown", "Lead Acid", "Nickel Cadmium",
        "Nickel Metal Hydride", "Lithium-ion", "Zinc air",
        "Lithium Polymer"
    };
    if (code >= 0 && code < int(sizeof(kTable) / sizeof(kTable[0])))
        return QString::fromLatin1(kTable[code]);
    return QStringLiteral("Chemistry %1").arg(code);
}

/// Byte-swap a hex-string PlatformGUID into RFC4122 UUID form. The
/// first three dword components are little-endian on the wire; the
/// last two are big-endian. Mirrors `legacy/source/Commander.htm`
/// `guidToStr` at line ~53087.
QString platformGuidToString(const QString &raw)
{
    if (raw.size() < 32) return raw;
    auto g = raw.toLower();
    return g.mid(6, 2) + g.mid(4, 2) + g.mid(2, 2) + g.mid(0, 2) + QLatin1Char('-')
         + g.mid(10, 2) + g.mid(8, 2) + QLatin1Char('-')
         + g.mid(14, 2) + g.mid(12, 2) + QLatin1Char('-')
         + g.mid(16, 4) + QLatin1Char('-')
         + g.mid(20);
}

/// Enumerate every instance of `resourceUri` and hand the raw item
/// XML bodies to `onItems`. Used by `getHardwareInventory` ten times
/// over. On error / fault the items list is empty and `error` is set.
void enumerateAll(WsmanClient *client, const char *resourceUri,
                  std::function<void(QList<QByteArray>, QString)> onDone)
{
    struct Acc { QList<QByteArray> items; };
    auto acc = std::make_shared<Acc>();
    auto onDoneShared =
        std::make_shared<std::function<void(QList<QByteArray>, QString)>>(
            std::move(onDone));
    const QString uri = QString::fromLatin1(resourceUri);

    auto pullStep = std::make_shared<std::function<void(const QString &)>>();
    *pullStep = [client, uri, acc, pullStep, onDoneShared](const QString &context) mutable {
        const QByteArray env = buildPullEnvelope(uri, context, 64,
                                                  client->endpoint().toString(),
                                                  newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc, pullStep, onDoneShared]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                if (err) { (*onDoneShared)({}, errString); return; }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) { (*onDoneShared)({}, soap.fault); return; }
                const PullChunk chunk = parsePullResponse(soap.bodyXml);
                for (const QByteArray &it : chunk.items) acc->items.append(it);
                if (chunk.endOfSequence || chunk.enumerationContext.isEmpty()) {
                    (*onDoneShared)(std::move(acc->items), {});
                    return;
                }
                (*pullStep)(chunk.enumerationContext);
            });
    };

    if (client == nullptr) {
        (*onDoneShared)({}, QStringLiteral("client is null"));
        return;
    }
    const QByteArray env = buildEnumerateEnvelope(uri,
                                                   client->endpoint().toString(),
                                                   newMessageId());
    WsmanReply *reply = client->sendEnvelope(env);
    QObject::connect(reply, &WsmanReply::finished, client,
        [reply, pullStep, onDoneShared, acc]() mutable {
            const QByteArray body = reply->readAll();
            const auto err = reply->hasError();
            const auto errString = reply->errorString();
            reply->deleteLater();
            if (err) { (*onDoneShared)({}, errString); return; }
            const SoapResponse soap = parseResponse(body);
            if (soap.isFault()) { (*onDoneShared)({}, soap.fault); return; }
            const QString ctx = parseEnumerateContext(soap.bodyXml);
            if (ctx.isEmpty()) {
                (*onDoneShared)(std::move(acc->items), {});
                return;
            }
            (*pullStep)(ctx);
        });
}

} // namespace

namespace {

// AMT audit-string table, ported from `legacy/source/Commander.htm`
// `_AmtAuditStringTable`. Lookup keys: the bare AuditAppID (gives the
// "audit application" label) and `AuditAppID * 100 + EventID` (gives
// the specific event label).
QString auditLookup(int key)
{
    switch (key) {
    // Audit apps.
    case 16: return QStringLiteral("Security Admin");
    case 17: return QStringLiteral("RCO");
    case 18: return QStringLiteral("Redirection Manager");
    case 19: return QStringLiteral("Firmware Update Manager");
    case 20: return QStringLiteral("Security Audit Log");
    case 21: return QStringLiteral("Network Time");
    case 22: return QStringLiteral("Network Administration");
    case 23: return QStringLiteral("Storage Administration");
    case 24: return QStringLiteral("Event Manager");
    case 25: return QStringLiteral("Circuit Breaker Manager");
    case 26: return QStringLiteral("Agent Presence Manager");
    case 27: return QStringLiteral("Wireless Configuration");
    case 28: return QStringLiteral("EAC");
    case 29: return QStringLiteral("KVM");
    case 30: return QStringLiteral("User Opt-In Events");
    case 32: return QStringLiteral("Screen Blanking");
    case 33: return QStringLiteral("Watchdog Events");
    // Events (AuditAppID * 100 + EventID).
    case 1600: return QStringLiteral("Provisioning Started");
    case 1601: return QStringLiteral("Provisioning Completed");
    case 1602: return QStringLiteral("ACL Entry Added");
    case 1603: return QStringLiteral("ACL Entry Modified");
    case 1604: return QStringLiteral("ACL Entry Removed");
    case 1605: return QStringLiteral("ACL Access with Invalid Credentials");
    case 1606: return QStringLiteral("ACL Entry State");
    case 1607: return QStringLiteral("TLS State Changed");
    case 1608: return QStringLiteral("TLS Server Certificate Set");
    case 1609: return QStringLiteral("TLS Server Certificate Removed");
    case 1610: return QStringLiteral("TLS Trusted Root Certificate Added");
    case 1611: return QStringLiteral("TLS Trusted Root Certificate Removed");
    case 1612: return QStringLiteral("TLS Preshared Key Set");
    case 1613: return QStringLiteral("Kerberos Settings Modified");
    case 1614: return QStringLiteral("Kerberos Main Key Modified");
    case 1615: return QStringLiteral("Flash Wear-out Counters Reset");
    case 1616: return QStringLiteral("Power Package Modified");
    case 1617: return QStringLiteral("Set Realm Authentication Mode");
    case 1618: return QStringLiteral("Upgrade Client to Admin Control Mode");
    case 1619: return QStringLiteral("Unprovisioning Started");
    case 1700: return QStringLiteral("Performed Power Up");
    case 1701: return QStringLiteral("Performed Power Down");
    case 1702: return QStringLiteral("Performed Power Cycle");
    case 1703: return QStringLiteral("Performed Reset");
    case 1704: return QStringLiteral("Set Boot Options");
    case 1705: return QStringLiteral("Remote graceful power down initiated");
    case 1706: return QStringLiteral("Remote graceful reset initiated");
    case 1707: return QStringLiteral("Remote Standby initiated");
    case 1708: return QStringLiteral("Remote Hibernate initiated");
    case 1709: return QStringLiteral("Remote NMI initiated");
    case 1800: return QStringLiteral("IDER Session Opened");
    case 1801: return QStringLiteral("IDER Session Closed");
    case 1802: return QStringLiteral("IDER Enabled");
    case 1803: return QStringLiteral("IDER Disabled");
    case 1804: return QStringLiteral("SoL Session Opened");
    case 1805: return QStringLiteral("SoL Session Closed");
    case 1806: return QStringLiteral("SoL Enabled");
    case 1807: return QStringLiteral("SoL Disabled");
    case 1808: return QStringLiteral("KVM Session Started");
    case 1809: return QStringLiteral("KVM Session Ended");
    case 1810: return QStringLiteral("KVM Enabled");
    case 1811: return QStringLiteral("KVM Disabled");
    case 1812: return QStringLiteral("VNC Password Failed 3 Times");
    case 1900: return QStringLiteral("Firmware Updated");
    case 1901: return QStringLiteral("Firmware Update Failed");
    case 2000: return QStringLiteral("Security Audit Log Cleared");
    case 2001: return QStringLiteral("Security Audit Policy Modified");
    case 2002: return QStringLiteral("Security Audit Log Disabled");
    case 2003: return QStringLiteral("Security Audit Log Enabled");
    case 2004: return QStringLiteral("Security Audit Log Exported");
    case 2005: return QStringLiteral("Security Audit Log Recovered");
    case 2100: return QStringLiteral("Intel® ME Time Set");
    case 2200: return QStringLiteral("TCPIP Parameters Set");
    case 2201: return QStringLiteral("Host Name Set");
    case 2202: return QStringLiteral("Domain Name Set");
    case 2203: return QStringLiteral("VLAN Parameters Set");
    case 2204: return QStringLiteral("Link Policy Set");
    case 2205: return QStringLiteral("IPv6 Parameters Set");
    case 2300: return QStringLiteral("Global Storage Attributes Set");
    case 2301: return QStringLiteral("Storage EACL Modified");
    case 2302: return QStringLiteral("Storage FPACL Modified");
    case 2303: return QStringLiteral("Storage Write Operation");
    case 2400: return QStringLiteral("Alert Subscribed");
    case 2401: return QStringLiteral("Alert Unsubscribed");
    case 2402: return QStringLiteral("Event Log Cleared");
    case 2403: return QStringLiteral("Event Log Frozen");
    case 2500: return QStringLiteral("CB Filter Added");
    case 2501: return QStringLiteral("CB Filter Removed");
    case 2502: return QStringLiteral("CB Policy Added");
    case 2503: return QStringLiteral("CB Policy Removed");
    case 2504: return QStringLiteral("CB Default Policy Set");
    case 2505: return QStringLiteral("CB Heuristics Option Set");
    case 2506: return QStringLiteral("CB Heuristics State Cleared");
    case 2600: return QStringLiteral("Agent Watchdog Added");
    case 2601: return QStringLiteral("Agent Watchdog Removed");
    case 2602: return QStringLiteral("Agent Watchdog Action Set");
    case 2700: return QStringLiteral("Wireless Profile Added");
    case 2701: return QStringLiteral("Wireless Profile Removed");
    case 2702: return QStringLiteral("Wireless Profile Updated");
    case 2800: return QStringLiteral("EAC Posture Signer Set");
    case 2801: return QStringLiteral("EAC Enabled");
    case 2802: return QStringLiteral("EAC Disabled");
    case 2803: return QStringLiteral("EAC Posture State");
    case 2804: return QStringLiteral("EAC Set Options");
    case 2900: return QStringLiteral("KVM Opt-in Enabled");
    case 2901: return QStringLiteral("KVM Opt-in Disabled");
    case 2902: return QStringLiteral("KVM Password Changed");
    case 2903: return QStringLiteral("KVM Consent Succeeded");
    case 2904: return QStringLiteral("KVM Consent Failed");
    case 3000: return QStringLiteral("Opt-In Policy Change");
    case 3001: return QStringLiteral("Send Consent Code Event");
    case 3002: return QStringLiteral("Start Opt-In Blocked Event");
    case 3301: return QStringLiteral("User has modified the Watchdog Action settings");
    default: return {};
    }
}

quint16 readBeU16(const QByteArray &b, int off)
{
    if (off + 1 >= b.size()) return 0;
    return (quint16(quint8(b[off])) << 8) | quint8(b[off + 1]);
}

quint32 readBeU32(const QByteArray &b, int off)
{
    if (off + 3 >= b.size()) return 0;
    return (quint32(quint8(b[off]))     << 24)
         | (quint32(quint8(b[off + 1])) << 16)
         | (quint32(quint8(b[off + 2])) <<  8)
         |  quint32(quint8(b[off + 3]));
}

bool parseAuditRecord(const QByteArray &raw, AuditLogEntry &out)
{
    if (raw.size() < 5) return false;
    out.auditAppId   = readBeU16(raw, 0);
    out.eventId      = readBeU16(raw, 2);
    out.initiatorType = quint8(raw[4]);
    out.auditAppLabel = auditLookup(out.auditAppId);
    out.eventLabel    = auditLookup(out.auditAppId * 100 + out.eventId);
    if (out.eventLabel.isEmpty())
        out.eventLabel = QStringLiteral("#%1").arg(out.eventId);

    int ptr = 5;
    switch (out.initiatorType) {
    case 0: { // HTTP digest
        if (ptr >= raw.size()) return false;
        const int userlen = quint8(raw[ptr++]);
        if (ptr + userlen > raw.size()) return false;
        out.initiator = QString::fromUtf8(raw.constData() + ptr, userlen);
        ptr += userlen;
        break;
    }
    case 1: { // Kerberos — skip the 4-byte domain id, then SID
        if (ptr + 5 > raw.size()) return false;
        ptr += 4;
        const int userlen = quint8(raw[ptr++]);
        if (ptr + userlen > raw.size()) return false;
        // SID is a packed binary blob; for now stringify as hex.
        out.initiator = QString::fromLatin1(
            QByteArray(raw.constData() + ptr, userlen).toHex());
        ptr += userlen;
        break;
    }
    case 2: out.initiator = QStringLiteral("Local"); break;
    case 3: out.initiator = QStringLiteral("KVM Default Port"); break;
    default: out.initiator = QStringLiteral("Unknown initiator %1")
                                  .arg(out.initiatorType);
    }

    if (ptr + 4 > raw.size()) return true;
    out.unixSeconds = readBeU32(raw, ptr);
    ptr += 4;

    if (ptr >= raw.size()) return true;
    out.mcLocationType = quint8(raw[ptr++]);
    if (ptr >= raw.size()) return true;
    const int netlen = quint8(raw[ptr++]);
    if (ptr + netlen > raw.size()) return true;
    out.netAddress = QString::fromUtf8(raw.constData() + ptr, netlen);
    ptr += netlen;

    if (ptr >= raw.size()) return true;
    const int exlen = quint8(raw[ptr++]);
    if (ptr + exlen > raw.size()) return true;
    out.ex = QByteArray(raw.constData() + ptr, exlen);
    return true;
}

} // namespace

void getAuditLogState(WsmanClient *client,
                      std::function<void(AuditLogState)> callback)
{
    const QByteArray env = buildGetEnvelope(
        QString::fromLatin1(kAuditLogResource), {},
        client ? client->endpoint().toString() : QString(),
        newMessageId());
    runRequest<AuditLogState>(client, env, {},
        [](const QByteArray &body, AuditLogState &r) {
            const auto pickInt = [&](const QString &name, int &dst) {
                const QString s = findScalar(body, name);
                if (s.isEmpty()) return;
                bool conv = false;
                const int v = s.toInt(&conv);
                if (conv) dst = v;
            };
            pickInt(QStringLiteral("AuditState"),             r.auditState);
            pickInt(QStringLiteral("OverwritePolicy"),        r.overwritePolicy);
            pickInt(QStringLiteral("CurrentNumberOfRecords"), r.currentNumberOfRecords);
            pickInt(QStringLiteral("PercentageFree"),         r.percentageFree);
            pickInt(QStringLiteral("MaxAllowedAuditors"),     r.maxAllowedAuditors);
            pickInt(QStringLiteral("EnabledState"),           r.enabledState);
            r.ok = true;
        },
        std::move(callback));
}

void enumerateAuditLog(WsmanClient *client,
                       std::function<void(AuditLogResult)> callback)
{
    struct Acc {
        QList<AuditLogEntry> entries;
        int totalReported = -1;
        std::function<void(AuditLogResult)> cb;
        bool fired = false;
    };
    auto acc = std::make_shared<Acc>();
    acc->cb = std::move(callback);

    if (client == nullptr) {
        AuditLogResult r;
        r.error = QStringLiteral("client is null");
        acc->cb(std::move(r));
        return;
    }

    auto finish = [acc](QString error) {
        if (acc->fired) return;
        acc->fired = true;
        AuditLogResult r;
        r.ok = error.isEmpty();
        r.error = std::move(error);
        r.entries = std::move(acc->entries);
        acc->cb(std::move(r));
    };

    auto pullStep = std::make_shared<std::function<void(int)>>();
    *pullStep = [client, acc, pullStep, finish](int startIndex) mutable {
        QHash<QString, QString> params;
        params.insert(QStringLiteral("StartIndex"),
                      QString::number(startIndex));
        const QByteArray env = buildInvokeEnvelope(
            QString::fromLatin1(kAuditLogResource),
            QStringLiteral("ReadRecords"), {}, params,
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, acc, pullStep, finish, startIndex]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                if (err) { finish(errString); return; }
                const SoapResponse soap = parseResponse(body);
                if (soap.isFault()) { finish(soap.fault); return; }

                bool conv = false;
                const int total = findScalar(soap.bodyXml,
                                              QStringLiteral("TotalRecordCount")).toInt(&conv);
                if (conv) acc->totalReported = total;

                // EventRecords come back as repeated base64-encoded
                // <g:EventRecords>...</g:EventRecords> children.
                // Walk the body XML element-by-element to pick them up
                // by local name (any namespace) — substring scanning
                // gets fooled by the matching close-tag suffix.
                QXmlStreamReader reader(soap.bodyXml);
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isStartElement()
                        && reader.name() == QStringLiteral("EventRecords")) {
                        QByteArray b64 = reader.readElementText().toLatin1();
                        b64.replace('\n', "").replace('\r', "").replace(' ', "");
                        const QByteArray raw = QByteArray::fromBase64(b64);
                        if (!raw.isEmpty()) {
                            AuditLogEntry e;
                            if (parseAuditRecord(raw, e)) acc->entries.append(e);
                        }
                    }
                }

                // Continue paging until we've collected every record
                // the firmware claims exist. Guard against infinite
                // loops if the server reports a larger total than it
                // actually returns.
                const int next = static_cast<int>(acc->entries.size()) + 1;
                const bool more = acc->totalReported >= next - 1
                                && acc->totalReported > 0
                                && acc->entries.size() < acc->totalReported;
                if (more && next > startIndex) {
                    (*pullStep)(next);
                } else {
                    finish({});
                }
            });
    };

    (*pullStep)(1);
}

void getHardwareInventory(WsmanClient *client,
                          std::function<void(HardwareInventoryResult)> callback)
{
    // Ten enumerations in parallel, then merge once they've all
    // returned. Each section is tolerant of empty / faulted classes:
    // we only fail the whole call if *every* enumeration failed.
    enum class Kind {
        Chassis, Card, Bios, SysPkg, Processor, Chip,
        PhysMem, Media, PhysPkg, Battery, _Count
    };
    constexpr int kCount = int(Kind::_Count);

    struct State {
        std::array<bool, kCount> done{};
        std::array<bool, kCount> ok{};
        std::array<QList<QByteArray>, kCount> items;
        std::array<QString, kCount> errors;
        std::function<void(HardwareInventoryResult)> cb;
        bool fired = false;
    };
    auto st = std::make_shared<State>();
    st->cb = std::move(callback);

    if (client == nullptr) {
        HardwareInventoryResult r;
        r.error = QStringLiteral("client is null");
        st->cb(std::move(r));
        return;
    }

    auto maybeFire = [st]() {
        if (st->fired) return;
        for (bool d : st->done) if (!d) return;
        st->fired = true;

        HardwareInventoryResult r;

        // Whole-call ok if at least one enumeration succeeded.
        bool anyOk = false;
        for (bool ok : st->ok) if (ok) { anyOk = true; break; }
        r.ok = anyOk;
        if (!anyOk) {
            for (const QString &e : st->errors) if (!e.isEmpty()) {
                r.error = e; break;
            }
        }

        // -- Platform / Chassis / SystemPackaging ------------------
        const auto &chassis = st->items[int(Kind::Chassis)];
        if (!chassis.isEmpty()) {
            const QByteArray &c = chassis.first();
            r.platformModel        = findScalar(c, QStringLiteral("Model"));
            r.platformManufacturer = findScalar(c, QStringLiteral("Manufacturer"));
            r.platformVersion      = findScalar(c, QStringLiteral("Version"));
            r.platformSerialNumber = findScalar(c, QStringLiteral("SerialNumber"));
        }
        const auto &syspkg = st->items[int(Kind::SysPkg)];
        if (!syspkg.isEmpty()) {
            r.platformSystemId = platformGuidToString(
                findScalar(syspkg.first(), QStringLiteral("PlatformGUID")));
        }

        // -- Baseboard / Card --------------------------------------
        const auto &cards = st->items[int(Kind::Card)];
        if (!cards.isEmpty()) {
            const QByteArray &c = cards.first();
            r.baseboardManufacturer = findScalar(c, QStringLiteral("Manufacturer"));
            r.baseboardModel        = findScalar(c, QStringLiteral("Model"));
            r.baseboardVersion      = findScalar(c, QStringLiteral("Version"));
            r.baseboardSerialNumber = findScalar(c, QStringLiteral("SerialNumber"));
            r.baseboardAssetTag     = findScalar(c, QStringLiteral("Tag"));
            const QString frued     = findScalar(c, QStringLiteral("CanBeFRUed"));
            r.baseboardCanBeFRUedKnown = !frued.isEmpty();
            r.baseboardReplaceable  = (frued == QStringLiteral("true"));
        }

        // -- BIOS ---------------------------------------------------
        const auto &bios = st->items[int(Kind::Bios)];
        if (!bios.isEmpty()) {
            const QByteArray &b = bios.first();
            r.biosVendor      = findScalar(b, QStringLiteral("Manufacturer"));
            r.biosVersion     = findScalar(b, QStringLiteral("SoftwareElementID"));
            // ReleaseDate is nested CIM datetime; the legacy code reads
            // the inner Datetime text. `findScalar` matches by local
            // name so it returns the first inner text — which is what
            // we want.
            r.biosReleaseDate = findScalar(b, QStringLiteral("Datetime"));
            if (r.biosReleaseDate.isEmpty())
                r.biosReleaseDate = findScalar(b, QStringLiteral("ReleaseDate"));
        }

        // -- Processors (CIM_Processor zipped with CIM_Chip) -------
        const auto &cpus  = st->items[int(Kind::Processor)];
        const auto &chips = st->items[int(Kind::Chip)];
        for (int i = 0; i < cpus.size(); ++i) {
            HardwareCpu cpu;
            const QByteArray &p = cpus[i];
            bool conv = false;
            const int fam = findScalar(p, QStringLiteral("Family")).toInt(&conv);
            if (conv) {
                cpu.family = fam;
                cpu.familyLabel = dmtfCpuFamilyLabel(fam);
            }
            cpu.maxClockSpeedMhz = findScalar(p, QStringLiteral("MaxClockSpeed")).toInt();
            conv = false;
            const int status = findScalar(p, QStringLiteral("CPUStatus")).toInt(&conv);
            if (conv) {
                cpu.cpuStatus = status;
                cpu.cpuStatusLabel = dmtfCpuStatusLabel(status);
            }
            if (i < chips.size()) {
                const QByteArray &q = chips[i];
                cpu.manufacturer = findScalar(q, QStringLiteral("Manufacturer"));
                cpu.version      = findScalar(q, QStringLiteral("Version"));
            }
            r.processors.append(cpu);
        }

        // -- Memory ------------------------------------------------
        for (const QByteArray &m : st->items[int(Kind::PhysMem)]) {
            HardwareDimm d;
            d.bankLabel     = findScalar(m, QStringLiteral("BankLabel"));
            d.manufacturer  = findScalar(m, QStringLiteral("Manufacturer"));
            d.serialNumber  = findScalar(m, QStringLiteral("SerialNumber"));
            d.capacityBytes = findScalar(m, QStringLiteral("Capacity")).toLongLong();
            bool conv = false;
            const int ff = findScalar(m, QStringLiteral("FormFactor")).toInt(&conv);
            if (conv) {
                d.formFactor = ff;
                d.formFactorLabel = dmtfMemFormFactorLabel(ff);
            }
            conv = false;
            const int mt = findScalar(m, QStringLiteral("MemoryType")).toInt(&conv);
            if (conv) {
                d.memoryType = mt;
                d.memoryTypeLabel = dmtfMemTypeLabel(mt);
            }
            d.assetTag    = findScalar(m, QStringLiteral("Tag"));
            d.partNumber  = findScalar(m, QStringLiteral("PartNumber"));
            r.memoryModules.append(d);
        }

        // -- Storage (CIM_MediaAccessDevice + CIM_PhysicalPackage) -
        // Legacy assumes `CIM_PhysicalPackage[i+1]` matches
        // `CIM_MediaAccessDevice[i]` because the first package is the
        // chassis. Replicate that shortcut here.
        const auto &media = st->items[int(Kind::Media)];
        const auto &pkgs  = st->items[int(Kind::PhysPkg)];
        for (int i = 0; i < media.size(); ++i) {
            HardwareStorage s;
            const QByteArray &m = media[i];
            s.maxMediaSizeKb = findScalar(m, QStringLiteral("MaxMediaSize")).toLongLong();
            if (i + 1 < pkgs.size()) {
                const QByteArray &n = pkgs[i + 1];
                s.model = findScalar(n, QStringLiteral("Model"));
                s.serialNumber = findScalar(n, QStringLiteral("SerialNumber"));
            }
            r.storageDevices.append(s);
        }

        // -- Battery -----------------------------------------------
        const auto &batt = st->items[int(Kind::Battery)];
        if (!batt.isEmpty()) {
            const QByteArray &b = batt.first();
            HardwareBattery bb;
            bb.present = true;
            bb.deviceId = findScalar(b, QStringLiteral("DeviceID"));
            bool conv = false;
            const int chem = findScalar(b, QStringLiteral("Chemistry")).toInt(&conv);
            if (conv) {
                bb.chemistry = chem;
                bb.chemistryLabel = dmtfBatteryChemistryLabel(chem);
            }
            bb.designCapacityMwh = findScalar(b, QStringLiteral("DesignCapacity")).toLongLong();
            bb.designVoltageMv   = findScalar(b, QStringLiteral("DesignVoltage")).toLongLong();
            // Find the matching physical package by PackageType==11.
            for (const QByteArray &pkg : pkgs) {
                if (findScalar(pkg, QStringLiteral("PackageType")) == QStringLiteral("11")) {
                    bb.manufacturer = findScalar(pkg, QStringLiteral("Manufacturer"));
                    bb.serialNumber = findScalar(pkg, QStringLiteral("SerialNumber"));
                    bb.manufactureDate = findScalar(pkg, QStringLiteral("Datetime"));
                    bb.otherIdentifyingInfo =
                        findScalar(pkg, QStringLiteral("OtherIdentifyingInfo"));
                    break;
                }
            }
            r.battery = std::move(bb);
        }

        st->cb(std::move(r));
    };

    auto kick = [client, st, maybeFire](Kind k, const char *uri) {
        enumerateAll(client, uri,
            [st, k, maybeFire](QList<QByteArray> items, QString error) mutable {
                const int idx = int(k);
                st->done[idx]   = true;
                st->items[idx]  = std::move(items);
                st->errors[idx] = error;
                st->ok[idx]     = error.isEmpty();
                maybeFire();
            });
    };

    kick(Kind::Chassis,   kChassisResource);
    kick(Kind::Card,      kCardResource);
    kick(Kind::Bios,      kBiosElementResource);
    kick(Kind::SysPkg,    kSystemPackagingResource);
    kick(Kind::Processor, kProcessorResource);
    kick(Kind::Chip,      kChipResource);
    kick(Kind::PhysMem,   kPhysicalMemoryResource);
    kick(Kind::Media,     kMediaAccessDeviceResource);
    kick(Kind::PhysPkg,   kPhysicalPackageResource);
    kick(Kind::Battery,   kBatteryResource);
}

QHash<QString, QString> splitDn(const QString &dn)
{
    QHash<QString, QString> out;
    const QStringList parts = dn.split(QLatin1Char(','));
    for (const QString &raw : parts) {
        const int eq = raw.indexOf(QLatin1Char('='));
        if (eq < 0) continue;
        const QString key = raw.left(eq).trimmed();
        const QString val = raw.mid(eq + 1).trimmed();
        if (!key.isEmpty()) out.insert(key, val);
    }
    return out;
}

namespace {

DeviceCertificate parseCertItem(const QByteArray &item)
{
    DeviceCertificate c;
    c.instanceId = findScalar(item, QStringLiteral("InstanceID"));
    c.subjectRaw = findScalar(item, QStringLiteral("Subject"));
    c.issuerRaw  = findScalar(item, QStringLiteral("Issuer"));
    // AMT firmware (depending on revision) sends either "TrustedRootCertficate"
    // (with the typo preserved by the legacy MeshCommander) or the
    // corrected "TrustedRootCertificate". Accept both.
    const QString trTypo = findScalar(item, QStringLiteral("TrustedRootCertficate"));
    const QString trGood = findScalar(item, QStringLiteral("TrustedRootCertificate"));
    c.trustedRoot = (trTypo == QStringLiteral("true"))
                 || (trGood == QStringLiteral("true"));
    c.x509Base64 = findScalar(item, QStringLiteral("X509Certificate"));
    c.derSizeBytes = QByteArray::fromBase64(c.x509Base64.toLatin1()).size();
    c.subjectCn = splitDn(c.subjectRaw).value(QStringLiteral("CN"));
    c.issuerCn  = splitDn(c.issuerRaw).value(QStringLiteral("CN"));
    return c;
}

DeviceKeyPair parseKeyPairItem(const QByteArray &item)
{
    DeviceKeyPair p;
    p.instanceId = findScalar(item, QStringLiteral("InstanceID"));
    const QString der = findScalar(item, QStringLiteral("DERKey"));
    p.derSizeBytes = QByteArray::fromBase64(der.toLatin1()).size();
    return p;
}

TlsSettingsRow parseTlsRow(const QByteArray &item)
{
    TlsSettingsRow t;
    t.instanceId = findScalar(item, QStringLiteral("InstanceID"));
    t.enabled = findScalar(item, QStringLiteral("Enabled")) == QStringLiteral("true");
    t.mutualAuthentication = findScalar(item,
        QStringLiteral("MutualAuthentication")) == QStringLiteral("true");
    t.acceptNonSecureConnections = findScalar(item,
        QStringLiteral("AcceptNonSecureConnections")) == QStringLiteral("true");
    QXmlStreamReader r(item);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("TrustedCN")) {
            const QString s = r.readElementText();
            if (!s.isEmpty()) t.trustedCn.append(s);
        }
    }
    return t;
}

/// Pull the certificate InstanceID referenced by a
/// `AMT_TLSCredentialContext` row's `ElementInContext` EPR.
QString parseContextCertInstanceId(const QByteArray &item)
{
    QXmlStreamReader r(item);
    bool inElementInContext = false;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QStringLiteral("ElementInContext"))
                inElementInContext = true;
            // The InstanceID selector inside the EPR identifies the
            // certificate this context binds.
            else if (inElementInContext
                  && r.name() == QStringLiteral("Selector")) {
                const auto attrs = r.attributes();
                if (attrs.value(QStringLiteral("Name"))
                        == QStringLiteral("InstanceID")) {
                    return r.readElementText();
                }
            }
        } else if (r.isEndElement()
                && r.name() == QStringLiteral("ElementInContext")) {
            inElementInContext = false;
        }
    }
    return {};
}

} // namespace

void getDeviceCertStore(WsmanClient *client,
                        std::function<void(DeviceCertResult)> callback)
{
    enum class Kind { Cert, KeyPair, Tls, Ctx, _Count };
    constexpr int kCount = int(Kind::_Count);
    struct State {
        std::array<bool, kCount> done{};
        std::array<QList<QByteArray>, kCount> items;
        std::array<QString, kCount> errors;
        std::function<void(DeviceCertResult)> cb;
        bool fired = false;
    };
    auto st = std::make_shared<State>();
    st->cb = std::move(callback);

    if (client == nullptr) {
        DeviceCertResult r;
        r.error = QStringLiteral("client is null");
        st->cb(std::move(r));
        return;
    }

    auto maybeFire = [st]() {
        if (st->fired) return;
        for (bool d : st->done) if (!d) return;
        st->fired = true;
        DeviceCertResult r;
        for (const auto &item : st->items[int(Kind::Cert)])
            r.certificates.append(parseCertItem(item));
        for (const auto &item : st->items[int(Kind::KeyPair)])
            r.keyPairs.append(parseKeyPairItem(item));
        for (const auto &item : st->items[int(Kind::Tls)])
            r.tlsSettings.append(parseTlsRow(item));
        for (const auto &item : st->items[int(Kind::Ctx)]) {
            const QString id = parseContextCertInstanceId(item);
            if (!id.isEmpty()) r.activeCertInstanceIds.append(id);
        }
        // Whole-call ok if at least one class returned, or if all
        // four are empty without errors (fresh-from-factory firmware).
        bool anyError = false;
        for (const QString &e : st->errors) if (!e.isEmpty()) { anyError = true; break; }
        if (anyError && r.certificates.isEmpty() && r.tlsSettings.isEmpty()) {
            // Surface the first error we saw.
            for (const QString &e : st->errors) if (!e.isEmpty()) { r.error = e; break; }
            r.ok = false;
        } else {
            r.ok = true;
        }
        st->cb(std::move(r));
    };

    auto kick = [client, st, maybeFire](Kind k, const char *uri) {
        enumerateAll(client, uri,
            [st, k, maybeFire](QList<QByteArray> items, QString error) mutable {
                const int idx = int(k);
                st->items[idx]  = std::move(items);
                st->errors[idx] = error;
                st->done[idx]   = true;
                maybeFire();
            });
    };
    kick(Kind::Cert,    kPublicKeyCertificateResource);
    kick(Kind::KeyPair, kPublicPrivateKeyPairResource);
    kick(Kind::Tls,     kTlsSettingDataResource);
    kick(Kind::Ctx,     kTlsCredentialContextResource);
}

QString userInitiatedCiraLabel(int code)
{
    switch (code) {
    case 32768: return QStringLiteral("Disabled");
    case 32769: return QStringLiteral("BIOS enabled");
    case 32770: return QStringLiteral("OS enabled");
    case 32771: return QStringLiteral("BIOS + OS enabled");
    default:    return QStringLiteral("State %1").arg(code);
    }
}

namespace {

QStringList parseDetectionStrings(const QByteArray &bodyXml)
{
    QStringList out;
    QXmlStreamReader r(bodyXml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("DetectionStrings")) {
            const QString s = r.readElementText();
            if (!s.isEmpty()) out.append(s);
        }
    }
    return out;
}

auto deviceCertReturnValueExtractor(const QString &what)
{
    return [what](const QByteArray &body, InvokeResult &r) {
        const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
        r.returnValue = rv.toInt();
        r.ok = (r.returnValue == 0);
        if (!r.ok) {
            r.error = QStringLiteral("%1 returned %2").arg(what,
                rv.isEmpty() ? QStringLiteral("(no ReturnValue)") : rv);
        }
    };
}

} // namespace (close the outer anon so the five Phase B writers
// below — addTrustedRootCertificate, addCertificate,
// deleteDeviceCertificate, deleteDeviceKeyPair, setTlsSettings — are
// reachable from outside the translation unit. Re-opened below for
// the rest of the file's local helpers.

void addTrustedRootCertificate(WsmanClient *client, const QString &certB64,
                                std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("CertificateBlob"), certB64);
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kPublicKeyManagementServiceResource),
        QStringLiteral("AddTrustedRootCertificate"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        deviceCertReturnValueExtractor(QStringLiteral("AddTrustedRootCertificate")),
        std::move(callback));
}

void addCertificate(WsmanClient *client, const QString &certB64,
                    std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("CertificateBlob"), certB64);
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kPublicKeyManagementServiceResource),
        QStringLiteral("AddCertificate"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        deviceCertReturnValueExtractor(QStringLiteral("AddCertificate")),
        std::move(callback));
}

void deleteDeviceCertificate(WsmanClient *client, const QString &instanceId,
                              std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kPublicKeyCertificateResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            // WS-Transfer Delete: absence of fault = success.
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void deleteDeviceKeyPair(WsmanClient *client, const QString &instanceId,
                          std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kPublicPrivateKeyPairResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void setTlsSettings(WsmanClient *client, const QString &instanceId,
                    bool enabled, bool mutualAuth,
                    bool acceptNonSecureConnections,
                    const QStringList &trustedCn,
                    std::function<void(InvokeResult)> callback)
{
    // Build the Put body by hand so we can emit the TrustedCN list as
    // repeated elements — buildPutEnvelope's QHash<QString, QString>
    // would collapse them. The structure is:
    //
    //   <r:AMT_TLSSettingData>
    //     <r:InstanceID>…</r:InstanceID>
    //     <r:Enabled>true|false</r:Enabled>
    //     <r:MutualAuthentication>…</r:MutualAuthentication>
    //     <r:AcceptNonSecureConnections>…</r:AcceptNonSecureConnections>
    //     <r:TrustedCN>cn-1</r:TrustedCN>
    //     <r:TrustedCN>cn-2</r:TrustedCN>
    //     …
    //   </r:AMT_TLSSettingData>
    //
    // AMT's Put validator requires every existing scalar on the
    // record; the four toggles plus the TrustedCN list cover the
    // editable fields. Other read-only fields (NonSecureConnections-
    // Supported, etc.) are populated by firmware and don't need to
    // be echoed.
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);
    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource = QString::fromLatin1(kTlsSettingDataResource);
    const QString to = client ? client->endpoint().toString() : QString();
    const QString msgId = newMessageId();

    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("r"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/09/transfer/Put"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing), QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), msgId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("SelectorSet"));
    for (auto it = selectors.constBegin(); it != selectors.constEnd(); ++it) {
        w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), it.key());
        w.writeCharacters(it.value());
        w.writeEndElement(); // Selector
    }
    w.writeEndElement(); // SelectorSet
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource, QStringLiteral("AMT_TLSSettingData"));
    w.writeTextElement(resource, QStringLiteral("InstanceID"), instanceId);
    w.writeTextElement(resource, QStringLiteral("Enabled"),
                        enabled ? QStringLiteral("true") : QStringLiteral("false"));
    w.writeTextElement(resource, QStringLiteral("MutualAuthentication"),
                        mutualAuth ? QStringLiteral("true") : QStringLiteral("false"));
    w.writeTextElement(resource, QStringLiteral("AcceptNonSecureConnections"),
                        acceptNonSecureConnections
                            ? QStringLiteral("true")
                            : QStringLiteral("false"));
    if (mutualAuth) {
        // TrustedCN only meaningful when mutual auth is requested.
        for (const QString &cn : trustedCn)
            w.writeTextElement(resource, QStringLiteral("TrustedCN"), cn);
    }
    w.writeEndElement(); // AMT_TLSSettingData
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();

    runRequest<InvokeResult>(client, out, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            // Put has no ReturnValue scalar; absence of fault = success.
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

namespace {

// Walk a `<KeyPair>` EPR returned by `GenerateKeyPair_OUTPUT`. The
// shape (lifted from go-wsman-messages and AMT's
// `AMT_PublicKeyManagementService.GenerateKeyPair_OUTPUT` reference
// XML) is:
//
//   <g:KeyPair>
//     <a:Address>...</a:Address>
//     <a:ReferenceParameters>
//       <w:ResourceURI>...AMT_PublicPrivateKeyPair</w:ResourceURI>
//       <w:SelectorSet>
//         <w:Selector Name="InstanceID">Intel(r) AMT Key: Handle: N</w:Selector>
//       </w:SelectorSet>
//     </a:ReferenceParameters>
//   </g:KeyPair>
//
// We pull out the InstanceID selector value — it's the only piece
// we need to address the new pair on follow-up calls.
QString parseGenerateKeyPairInstanceId(const QByteArray &bodyXml)
{
    QXmlStreamReader r(bodyXml);
    bool inSelector = false;
    QString currentSelectorName;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QStringLiteral("Selector")) {
            inSelector = true;
            currentSelectorName = r.attributes().value(QStringLiteral("Name")).toString();
            continue;
        }
        if (inSelector && r.isCharacters()
            && currentSelectorName == QStringLiteral("InstanceID")) {
            const QString v = r.text().toString();
            if (!v.trimmed().isEmpty()) return v;
        }
        if (r.isEndElement() && r.name() == QStringLiteral("Selector")) {
            inSelector = false;
            currentSelectorName.clear();
        }
    }
    return {};
}

// Hand-rolled `Put` envelope for `AMT_TLSCredentialContext`. The
// binding fields are EPRs (`ElementInContext` → the cert, and
// `ElementProvidingContext` → the TLS endpoint collection); neither
// is expressible through `buildPutEnvelope`'s QHash<QString,QString>
// property map. Same shape pattern as `setTlsSettings`'s hand-rolled
// envelope above.
QByteArray buildTlsCredentialContextPutEnvelope(
    const QString &certInstanceId,
    const QString &tlsEndpointCollectionId,
    const QString &to,
    const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);
    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource = QString::fromLatin1(kTlsCredentialContextResource);
    const QString certResource = QString::fromLatin1(kPublicKeyCertificateResource);
    const QString endpointCollectionResource = QStringLiteral(
        "http://intel.com/wbem/wscim/1/amt-schema/1/"
        "AMT_TLSProtocolEndpointCollection");

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("r"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header.
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/09/transfer/Put"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing), QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeEndElement(); // Header

    // Body.
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource, QStringLiteral("AMT_TLSCredentialContext"));

    auto writeEpr = [&](const QString &elementName,
                        const QString &eprResource,
                        const QString &selectorName,
                        const QString &selectorValue) {
        w.writeStartElement(resource, elementName);
        w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("Address"));
        w.writeCharacters(QStringLiteral(
            "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
        w.writeEndElement(); // Address
        w.writeStartElement(QString::fromLatin1(kNsAddressing),
                             QStringLiteral("ReferenceParameters"));
        w.writeTextElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("ResourceURI"), eprResource);
        w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("SelectorSet"));
        w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), selectorName);
        w.writeCharacters(selectorValue);
        w.writeEndElement(); // Selector
        w.writeEndElement(); // SelectorSet
        w.writeEndElement(); // ReferenceParameters
        w.writeEndElement(); // <elementName>
    };

    writeEpr(QStringLiteral("ElementInContext"),
             QString::fromLatin1(kPublicKeyCertificateResource),
             QStringLiteral("InstanceID"), certInstanceId);
    writeEpr(QStringLiteral("ElementProvidingContext"),
             endpointCollectionResource,
             QStringLiteral("ElementName"), tlsEndpointCollectionId);

    w.writeEndElement(); // AMT_TLSCredentialContext
    w.writeEndElement(); // Body
    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    Q_UNUSED(certResource);
    return out;
}

// Hand-rolled `GeneratePKCS10RequestEx` invoke envelope. The
// `KeyPair` parameter is an EPR pointing at AMT_PublicPrivateKeyPair;
// `buildInvokeEnvelope`'s `params` map can't represent it.
QByteArray buildGeneratePkcs10Envelope(const QString &keyPairInstanceId,
                                        int signingAlgorithm,
                                        const QByteArray &nullSignedCsrDer,
                                        const QString &to,
                                        const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);
    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource = QString::fromLatin1(kPublicKeyManagementServiceResource);
    const QString keyPairResource = QString::fromLatin1(kPublicPrivateKeyPairResource);

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("h"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"),
                        resource + QStringLiteral("/GeneratePKCS10RequestEx"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing), QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeEndElement(); // Header

    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource, QStringLiteral("GeneratePKCS10RequestEx_INPUT"));

    // <h:KeyPair> EPR.
    w.writeStartElement(resource, QStringLiteral("KeyPair"));
    w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("Address"));
    w.writeCharacters(QStringLiteral(
        "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // Address
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                         QStringLiteral("ReferenceParameters"));
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), keyPairResource);
    w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("SelectorSet"));
    w.writeStartElement(QString::fromLatin1(kNsWsman), QStringLiteral("Selector"));
    w.writeAttribute(QStringLiteral("Name"), QStringLiteral("InstanceID"));
    w.writeCharacters(keyPairInstanceId);
    w.writeEndElement(); // Selector
    w.writeEndElement(); // SelectorSet
    w.writeEndElement(); // ReferenceParameters
    w.writeEndElement(); // KeyPair

    w.writeTextElement(resource, QStringLiteral("SigningAlgorithm"),
                        QString::number(signingAlgorithm));
    w.writeTextElement(resource, QStringLiteral("NullSignedCertificateRequest"),
                        QString::fromLatin1(nullSignedCsrDer.toBase64()));

    w.writeEndElement(); // GeneratePKCS10RequestEx_INPUT
    w.writeEndElement(); // Body
    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

} // namespace

void generateKeyPair(WsmanClient *client, int keyAlgorithm, int keyLength,
                     std::function<void(GenerateKeyPairResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("KeyAlgorithm"), QString::number(keyAlgorithm));
    params.insert(QStringLiteral("KeyLength"), QString::number(keyLength));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kPublicKeyManagementServiceResource),
        QStringLiteral("GenerateKeyPair"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<GenerateKeyPairResult>(client, env, {},
        [](const QByteArray &body, GenerateKeyPairResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("GenerateKeyPair response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            if (!conv || r.returnValue != 0) {
                r.error = QStringLiteral("GenerateKeyPair returned %1").arg(rv);
                return;
            }
            r.keyPairInstanceId = parseGenerateKeyPairInstanceId(body);
            if (r.keyPairInstanceId.isEmpty()) {
                r.error = QStringLiteral("GenerateKeyPair: missing InstanceID in KeyPair EPR");
                return;
            }
            r.ok = true;
        },
        std::move(callback));
}

void getPublicPrivateKeyPair(WsmanClient *client, const QString &instanceId,
                              std::function<void(PublicPrivateKeyPairGetResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"), instanceId);
    const QByteArray env = buildGetEnvelope(
        QString::fromLatin1(kPublicPrivateKeyPairResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<PublicPrivateKeyPairGetResult>(client, env, {},
        [](const QByteArray &body, PublicPrivateKeyPairGetResult &r) {
            const QString b64 = findScalar(body, QStringLiteral("DERKey"));
            if (b64.isEmpty()) {
                r.error = QStringLiteral("AMT_PublicPrivateKeyPair.DERKey missing");
                return;
            }
            r.derKey = QByteArray::fromBase64(b64.toLatin1());
            if (r.derKey.isEmpty()) {
                r.error = QStringLiteral("AMT_PublicPrivateKeyPair.DERKey base64 decode failed");
                return;
            }
            r.ok = true;
        },
        std::move(callback));
}

void generatePkcs10Request(WsmanClient *client,
                            const QString &keyPairInstanceId,
                            int signingAlgorithm,
                            const QByteArray &nullSignedCsrDer,
                            std::function<void(GeneratePkcs10RequestResult)> callback)
{
    const QByteArray env = buildGeneratePkcs10Envelope(
        keyPairInstanceId, signingAlgorithm, nullSignedCsrDer,
        client ? client->endpoint().toString() : QString(),
        newMessageId());
    runRequest<GeneratePkcs10RequestResult>(client, env, {},
        [](const QByteArray &body, GeneratePkcs10RequestResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral("GeneratePKCS10RequestEx response had no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            if (!conv || r.returnValue != 0) {
                r.error = QStringLiteral("GeneratePKCS10RequestEx returned %1").arg(rv);
                return;
            }
            const QString b64 = findScalar(body, QStringLiteral("SignedCertificateRequest"));
            if (b64.isEmpty()) {
                r.error = QStringLiteral("GeneratePKCS10RequestEx: SignedCertificateRequest missing");
                return;
            }
            r.signedRequestDer = QByteArray::fromBase64(b64.toLatin1());
            if (r.signedRequestDer.isEmpty()) {
                r.error = QStringLiteral("GeneratePKCS10RequestEx: empty CSR DER after decode");
                return;
            }
            r.ok = true;
        },
        std::move(callback));
}

void bindCertToTlsEndpoint(WsmanClient *client,
                            const QString &certInstanceId,
                            const QString &tlsEndpointCollectionId,
                            bool replaceExisting,
                            std::function<void(InvokeResult)> callback)
{
    // AMT exposes the binding as a single `AMT_TLSCredentialContext`
    // instance per endpoint collection. When `replaceExisting` is
    // true the existing instance is updated via Put; when false (the
    // collection has no cert bound yet) we still send the same Put —
    // AMT's behavior is to upsert. The flag is plumbed through so
    // future versions can switch to a Create + Delete pattern if a
    // firmware revision needs it.
    Q_UNUSED(replaceExisting);
    const QByteArray env = buildTlsCredentialContextPutEnvelope(
        certInstanceId, tlsEndpointCollectionId,
        client ? client->endpoint().toString() : QString(),
        newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            // WS-Transfer Put: absence of fault = success. AMT may
            // echo the new TLSCredentialContext but doesn't carry a
            // ReturnValue here.
            Q_UNUSED(body);
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

// `decodeExtendedData` / `buildExtendedData` are declared in
// `wsman/include/wsman/operations.h` (public, so tests can call
// either side directly) and implemented further down in the
// `qumesh::wsman` namespace.

namespace {

/// Walk a `AMT_RemoteAccessPolicyAppliesToMPS` item-XML and pull the
/// two embedded EPRs out: the policy rule's `PolicyRuleName` and the
/// MPS server's `Name`. Returns {policyRuleName, mpsServerName}.
std::pair<QString, QString> parseLinkRow(const QByteArray &item)
{
    QString policyName, mpsName;
    QXmlStreamReader r(item);
    int currentEprIsPolicy = -1; // 1 = policy, 0 = mps, -1 = unknown
    bool sawResourceUri = false;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QStringLiteral("ResourceURI")) {
                const QString uri = r.readElementText();
                if (uri.endsWith(QStringLiteral("AMT_RemoteAccessPolicyRule"))) {
                    currentEprIsPolicy = 1;
                } else if (uri.endsWith(QStringLiteral("AMT_ManagementPresenceRemoteSAP"))) {
                    currentEprIsPolicy = 0;
                } else {
                    currentEprIsPolicy = -1;
                }
                sawResourceUri = true;
            } else if (r.name() == QStringLiteral("Selector") && sawResourceUri) {
                const auto attrs = r.attributes();
                const QString name = attrs.value(QStringLiteral("Name")).toString();
                const QString val = r.readElementText();
                if (currentEprIsPolicy == 1
                    && name == QStringLiteral("PolicyRuleName"))
                    policyName = val;
                if (currentEprIsPolicy == 0 && name == QStringLiteral("Name"))
                    mpsName = val;
            }
        }
    }
    return {policyName, mpsName};
}

} // namespace

void getRemoteAccess(WsmanClient *client,
                     std::function<void(RemoteAccessResult)> callback)
{
    enum class Kind {
        EnvDetect, UserInit, PolicyRule, PolicyApplies, Mps,
        MpsAuth, ProxyService, ProxyAccessPoint, _Count
    };
    constexpr int kCount = int(Kind::_Count);

    struct State {
        std::array<bool, kCount> done{};
        std::array<QList<QByteArray>, kCount> items;
        std::array<QString, kCount> errors;
        std::function<void(RemoteAccessResult)> cb;
        bool fired = false;
    };
    auto st = std::make_shared<State>();
    st->cb = std::move(callback);

    if (client == nullptr) {
        RemoteAccessResult r;
        r.error = QStringLiteral("client is null");
        st->cb(std::move(r));
        return;
    }

    auto maybeFire = [st]() {
        if (st->fired) return;
        for (bool d : st->done) if (!d) return;
        st->fired = true;
        RemoteAccessResult r;

        // Environment detection.
        if (!st->items[int(Kind::EnvDetect)].isEmpty()) {
            r.envDetection.domains = parseDetectionStrings(
                st->items[int(Kind::EnvDetect)].first());
        }

        // User-initiated.
        if (!st->items[int(Kind::UserInit)].isEmpty()) {
            const QByteArray &b = st->items[int(Kind::UserInit)].first();
            bool conv = false;
            const int v = findScalar(b, QStringLiteral("EnabledState")).toInt(&conv);
            if (conv) r.userInitiated.enabledState = v;
        }

        // MPS server list.
        for (const QByteArray &item : st->items[int(Kind::Mps)]) {
            MpsServer s;
            s.name       = findScalar(item, QStringLiteral("Name"));
            s.accessInfo = findScalar(item, QStringLiteral("AccessInfo"));
            bool conv = false;
            const int p = findScalar(item, QStringLiteral("Port")).toInt(&conv);
            if (conv) s.port = p;
            s.cn = findScalar(item, QStringLiteral("CN"));
            conv = false;
            const int mt = findScalar(item, QStringLiteral("MpsType")).toInt(&conv);
            if (conv) s.mpsType = mt;
            r.servers.append(s);
        }

        // Policy rules (User Initiated / Alert / Periodic).
        QHash<QString, RemoteAccessPolicy> policyByName;
        for (const QByteArray &item : st->items[int(Kind::PolicyRule)]) {
            RemoteAccessPolicy p;
            p.name = findScalar(item, QStringLiteral("PolicyRuleName"));
            bool conv = false;
            const int tr = findScalar(item, QStringLiteral("Trigger")).toInt(&conv);
            if (conv) p.trigger = tr;
            const int tl = findScalar(item, QStringLiteral("TunnelLifeTime")).toInt(&conv);
            if (conv) p.tunnelLifeTime = tl;
            const QString ext = findScalar(item, QStringLiteral("ExtendedData"));
            if (!ext.isEmpty()) decodeExtendedData(ext, p);
            policyByName.insert(p.name, p);
        }

        // Linkage policy → MPS via PolicyAppliesToMPS.
        for (const QByteArray &item : st->items[int(Kind::PolicyApplies)]) {
            auto [pn, mn] = parseLinkRow(item);
            if (pn.isEmpty() || mn.isEmpty()) continue;
            if (!policyByName.contains(pn)) continue;
            policyByName[pn].mpsNames.append(mn);
        }

        // Emit policies in a stable order.
        const QStringList order = {
            QStringLiteral("User Initiated"),
            QStringLiteral("Alert"),
            QStringLiteral("Periodic"),
        };
        for (const QString &n : order)
            if (policyByName.contains(n))
                r.policies.append(policyByName.value(n));
        // Any out-of-band policies the firmware may add.
        for (auto it = policyByName.constBegin(); it != policyByName.constEnd(); ++it)
            if (!order.contains(it.key())) r.policies.append(it.value());

        // HTTP proxies.
        if (!st->items[int(Kind::ProxyService)].isEmpty()) {
            r.httpProxySupported = true;
            for (const QByteArray &item : st->items[int(Kind::ProxyAccessPoint)]) {
                MpsHttpProxy p;
                p.name = findScalar(item, QStringLiteral("Name"));
                p.accessInfo = findScalar(item, QStringLiteral("AccessInfo"));
                bool conv = false;
                const int port = findScalar(item, QStringLiteral("Port")).toInt(&conv);
                if (conv) p.port = port;
                p.networkDnsSuffix = findScalar(item,
                                                 QStringLiteral("NetworkDnsSuffix"));
                r.httpProxies.append(p);
            }
        }

        // Whole-call ok if at least the env-detection or user-initiated
        // Get succeeded — anything else is optional.
        r.ok = !st->items[int(Kind::EnvDetect)].isEmpty()
            || !st->items[int(Kind::UserInit)].isEmpty();
        if (!r.ok)
            for (const QString &e : st->errors)
                if (!e.isEmpty()) { r.error = e; break; }
        st->cb(std::move(r));
    };

    auto kickEnum = [client, st, maybeFire](Kind k, const char *uri) {
        enumerateAll(client, uri,
            [st, k, maybeFire](QList<QByteArray> items, QString error) mutable {
                const int idx = int(k);
                st->items[idx]  = std::move(items);
                st->errors[idx] = error;
                st->done[idx]   = true;
                maybeFire();
            });
    };
    auto kickGet = [client, st, maybeFire](Kind k, const char *uri) {
        const QByteArray env = buildGetEnvelope(QString::fromLatin1(uri), {},
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, k, st, maybeFire]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                const int idx = int(k);
                if (!err) {
                    const SoapResponse soap = parseResponse(body);
                    if (!soap.isFault())
                        st->items[idx].append(soap.bodyXml);
                    else
                        st->errors[idx] = soap.fault;
                } else {
                    st->errors[idx] = errString;
                }
                st->done[idx] = true;
                maybeFire();
            });
    };

    kickGet(Kind::EnvDetect,         kEnvironmentDetectionResource);
    kickGet(Kind::UserInit,          kUserInitiatedConnectionResource);
    kickEnum(Kind::PolicyRule,       kRemoteAccessPolicyRuleResource);
    kickEnum(Kind::PolicyApplies,    kRemoteAccessPolicyAppliesToMpsResource);
    kickEnum(Kind::Mps,              kManagementPresenceRemoteSapResource);
    kickEnum(Kind::MpsAuth,          kMpsUsernamePasswordResource);
    kickGet(Kind::ProxyService,      kHttpProxyServiceResource);
    kickEnum(Kind::ProxyAccessPoint, kHttpProxyAccessPointResource);
}

int classifyAccessInfo(const QString &accessInfo)
{
    QHostAddress addr;
    if (addr.setAddress(accessInfo)) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            return 3;
        if (addr.protocol() == QAbstractSocket::IPv6Protocol)
            return 4;
    }
    // Intel's IPS_HTTPProxyAccessPoint extends the CIM InfoFormat enum
    // with 201 = "DNS name" for FQDN proxies. Anything that's not a
    // literal IP gets that code; AMT validates the string against
    // the format on the server side.
    return 201;
}

void addHttpProxy(WsmanClient *client, const QString &accessInfo,
                  int infoFormat, int port,
                  const QString &networkDnsSuffix,
                  std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("AccessInfo"),       accessInfo);
    params.insert(QStringLiteral("InfoFormat"),       QString::number(infoFormat));
    params.insert(QStringLiteral("Port"),             QString::number(port));
    params.insert(QStringLiteral("NetworkDnsSuffix"), networkDnsSuffix);

    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kHttpProxyServiceResource),
        QStringLiteral("AddProxyAccessPoint"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());

    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            r.returnValue = rv.toInt();
            r.ok = (r.returnValue == 0);
            if (!r.ok) {
                r.error = QStringLiteral(
                    "AddProxyAccessPoint returned %1").arg(rv);
            }
        },
        std::move(callback));
}

void deleteHttpProxy(WsmanClient *client, const QString &name,
                     std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"), name);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kHttpProxyAccessPointResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());

    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            // WS-Transfer Delete has no ReturnValue — the absence of a
            // SOAP Fault means success. runRequest already short-circuits
            // on Fault and on transport errors, so reaching here means
            // the instance was deleted.
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

QString wifiAuthMethodLabel(int code)
{
    switch (code) {
    case 1:     return QStringLiteral("Other");
    case 2:     return QStringLiteral("Open");
    case 3:     return QStringLiteral("Shared Key");
    case 4:     return QStringLiteral("WPA PSK");
    case 5:     return QStringLiteral("WPA 802.1x");
    case 6:     return QStringLiteral("WPA2 PSK");
    case 7:     return QStringLiteral("WPA2 802.1x");
    case 32768: return QStringLiteral("WPA3 SAE 802.1x");
    case 32769: return QStringLiteral("WPA3 OWE 802.1x");
    default:    return QStringLiteral("Auth %1").arg(code);
    }
}

QString wifiEncryptionLabel(int code)
{
    switch (code) {
    case 1: return QStringLiteral("Other");
    case 2: return QStringLiteral("WEP");
    case 3: return QStringLiteral("TKIP-RC4");
    case 4: return QStringLiteral("CCMP-AES");
    case 5: return QStringLiteral("None");
    default: return QStringLiteral("Enc %1").arg(code);
    }
}

QString wifiPortStateLabel(int code)
{
    switch (code) {
    case 3:     return QStringLiteral("Disabled");
    case 32768: return QStringLiteral("Enabled in S0");
    case 32769: return QStringLiteral("Enabled in S0, Sx/AC");
    default:    return QStringLiteral("State %1").arg(code);
    }
}

QString wifiRadioStateLabel(int code)
{
    switch (code) {
    case 2: return QStringLiteral("On, Connected");
    case 3: return QStringLiteral("Off");
    case 6: return QStringLiteral("On, Disconnected");
    default: return QStringLiteral("Radio %1").arg(code);
    }
}

QString eap8021xProtocolLabel(int code)
{
    static const char *const kTable[] = {
        "EAP-TLS",
        "EAP-TTLS/MSCHAPv2",
        "PEAPv0/EAP-MSCHAPv2",
        "PEAPv1/EAP-GTC",
        "EAP-FAST/MSCHAPv2",
        "EAP-FAST/GTC",
        "EAP-MD5",
        "EAP-PSK",
        "EAP-SIM",
        "EAP-AKA",
        "EAP-FAST/TLS",
    };
    if (code < 0 || code >= int(sizeof(kTable) / sizeof(kTable[0])))
        return QStringLiteral("EAP %1").arg(code);
    return QString::fromLatin1(kTable[code]);
}

void getWireless(WsmanClient *client,
                 std::function<void(WirelessResult)> callback)
{
    enum class Kind {
        Port, Endpoint, ConfigSvc, EndpointSettings,
        IEEE8021xSettings, Wired8021x, _Count
    };
    constexpr int kCount = int(Kind::_Count);
    struct State {
        std::array<bool, kCount> done{};
        std::array<QList<QByteArray>, kCount> items;
        std::array<QString, kCount> errors;
        std::function<void(WirelessResult)> cb;
        bool fired = false;
    };
    auto st = std::make_shared<State>();
    st->cb = std::move(callback);

    if (client == nullptr) {
        WirelessResult r;
        r.error = QStringLiteral("client is null");
        st->cb(std::move(r));
        return;
    }

    auto maybeFire = [st]() {
        if (st->fired) return;
        for (bool d : st->done) if (!d) return;
        st->fired = true;
        WirelessResult r;

        // Port + endpoint + config service.
        if (!st->items[int(Kind::Port)].isEmpty()) {
            const QByteArray &b = st->items[int(Kind::Port)].first();
            r.port.present = true;
            bool conv = false;
            const int v = findScalar(b, QStringLiteral("EnabledState")).toInt(&conv);
            if (conv) r.port.portState = v;
        }
        if (!st->items[int(Kind::Endpoint)].isEmpty()) {
            const QByteArray &b = st->items[int(Kind::Endpoint)].first();
            bool conv = false;
            const int v = findScalar(b, QStringLiteral("EnabledState")).toInt(&conv);
            if (conv) r.port.radioState = v;
            r.port.currentSsid = findScalar(b, QStringLiteral("LANID"));
        }
        if (!st->items[int(Kind::ConfigSvc)].isEmpty()) {
            const QByteArray &b = st->items[int(Kind::ConfigSvc)].first();
            const QString lps = findScalar(b,
                QStringLiteral("localProfileSynchronizationEnabled"));
            if (!lps.isEmpty()) {
                bool conv = false;
                const int v = lps.toInt(&conv);
                if (conv) r.port.localProfileSyncEnabled = v;
            }
            const QString ueg = findScalar(b,
                QStringLiteral("UEFIWiFiProfileShareEnabled"));
            if (!ueg.isEmpty()) {
                bool conv = false;
                const int v = ueg.toInt(&conv);
                if (conv) r.port.uefiProfileShareEnabled = v;
            }
        }

        // 802.1x settings keyed by ElementName so we can join into
        // the matching WiFi profiles.
        QHash<QString, int> eapByName;
        for (const QByteArray &item : st->items[int(Kind::IEEE8021xSettings)]) {
            const QString en = findScalar(item, QStringLiteral("ElementName"));
            bool conv = false;
            const int v = findScalar(item,
                QStringLiteral("AuthenticationProtocol")).toInt(&conv);
            if (conv && !en.isEmpty()) eapByName.insert(en, v);
        }

        // Endpoint settings → WiFi profiles. Skip AuthMethod==1
        // ("Endpoint User Settings") per the legacy.
        for (const QByteArray &item : st->items[int(Kind::EndpointSettings)]) {
            WiFiProfile p;
            p.elementName = findScalar(item, QStringLiteral("ElementName"));
            p.ssid        = findScalar(item, QStringLiteral("SSID"));
            bool conv = false;
            const int am = findScalar(item,
                QStringLiteral("AuthenticationMethod")).toInt(&conv);
            if (conv) p.authenticationMethod = am;
            if (am == 1) continue;
            conv = false;
            const int em = findScalar(item,
                QStringLiteral("EncryptionMethod")).toInt(&conv);
            if (conv) p.encryptionMethod = em;
            conv = false;
            const int pr = findScalar(item,
                QStringLiteral("Priority")).toInt(&conv);
            if (conv) p.priority = pr;
            if (eapByName.contains(p.elementName))
                p.eap8021xProtocol = eapByName.value(p.elementName);
            r.profiles.append(p);
        }
        // Sort by priority ascending (legacy iterates 0..255).
        std::sort(r.profiles.begin(), r.profiles.end(),
                  [](const WiFiProfile &a, const WiFiProfile &b) {
                      return a.priority < b.priority;
                  });

        // Wired 802.1x.
        if (!st->items[int(Kind::Wired8021x)].isEmpty()) {
            const QByteArray &b = st->items[int(Kind::Wired8021x)].first();
            r.wired.present = true;
            r.wired.enabled = findScalar(b,
                QStringLiteral("Enabled")) == QStringLiteral("true");
            bool conv = false;
            const int v = findScalar(b,
                QStringLiteral("AuthenticationProtocol")).toInt(&conv);
            if (conv) r.wired.authenticationProtocol = v;
        }

        // ok if at least the port or the wired entry returned (i.e.
        // there's *some* wireless story on this device). Empty
        // everywhere → still ok=true but the QML shows the no-WiFi
        // state.
        r.ok = true;
        st->cb(std::move(r));
    };

    auto kickEnum = [client, st, maybeFire](Kind k, const char *uri) {
        enumerateAll(client, uri,
            [st, k, maybeFire](QList<QByteArray> items, QString error) mutable {
                const int idx = int(k);
                st->items[idx]  = std::move(items);
                st->errors[idx] = error;
                st->done[idx]   = true;
                maybeFire();
            });
    };
    auto kickGet = [client, st, maybeFire](Kind k, const char *uri) {
        const QByteArray env = buildGetEnvelope(QString::fromLatin1(uri), {},
            client->endpoint().toString(), newMessageId());
        WsmanReply *reply = client->sendEnvelope(env);
        QObject::connect(reply, &WsmanReply::finished, client,
            [reply, k, st, maybeFire]() mutable {
                const QByteArray body = reply->readAll();
                const auto err = reply->hasError();
                const auto errString = reply->errorString();
                reply->deleteLater();
                const int idx = int(k);
                if (!err) {
                    const SoapResponse soap = parseResponse(body);
                    if (!soap.isFault())
                        st->items[idx].append(soap.bodyXml);
                    else
                        st->errors[idx] = soap.fault;
                } else {
                    st->errors[idx] = errString;
                }
                st->done[idx] = true;
                maybeFire();
            });
    };

    kickGet(Kind::Port,                 kWiFiPortResource);
    kickGet(Kind::Endpoint,             kWiFiEndpointResource);
    kickGet(Kind::ConfigSvc,            kWiFiPortConfigServiceResource);
    kickEnum(Kind::EndpointSettings,    kWiFiEndpointSettingsResource);
    kickEnum(Kind::IEEE8021xSettings,   kIEEE8021xSettingsResource);
    kickGet(Kind::Wired8021x,           kWired8021xProfileResource);
}

namespace {

/// Common 4-part selector set for `CIM_WiFiPort.RequestStateChange`.
/// AMT exposes a single WiFi port with the standard CIM naming.
QHash<QString, QString> wifiPortSelectors()
{
    return {
        { QStringLiteral("Name"),                    QStringLiteral("WiFi Port 0") },
        { QStringLiteral("SystemCreationClassName"), QStringLiteral("CIM_ComputerSystem") },
        { QStringLiteral("SystemName"),              QStringLiteral("Intel(r) AMT") },
        { QStringLiteral("CreationClassName"),       QStringLiteral("CIM_WiFiPort") },
    };
}

/// Standard return-value extractor shared by every wireless invoke:
/// 0 (Completed) and 4096 (Method Parameters Checked - Job Started) are
/// the documented success codes for state-change and provider methods.
auto wirelessReturnValueExtractor(const QString &what)
{
    return [what](const QByteArray &body, InvokeResult &r) {
        const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
        if (rv.isEmpty()) {
            r.error = QStringLiteral("%1: no ReturnValue in response").arg(what);
            return;
        }
        bool conv = false;
        r.returnValue = rv.toInt(&conv);
        r.ok = conv && (r.returnValue == 0 || r.returnValue == 4096);
        if (!r.ok)
            r.error = QStringLiteral("%1 returned %2").arg(what, rv);
    };
}

/// Build an `AddWiFiSettings` / `UpdateWiFiSettings` envelope by hand.
/// AMT expects the `WiFiEndpointSettingsInput` element to wrap the
/// SSID / auth / encryption / priority / PSK fields, and (for
/// AddWiFiSettings) a `WiFiEndpoint` EPR pointing at the single AMT
/// WiFi endpoint. We omit the optional `IEEE8021xSettingsInput` and
/// credential EPRs — those are reserved for the Phase C enterprise
/// flow.
QByteArray buildWiFiSettingsEnvelope(const QString &methodName,
                                      const WiFiPskProfile &profile,
                                      const QString &to,
                                      const QString &messageId)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource =
        QString::fromLatin1(kWiFiPortConfigServiceResource);
    const QString action = resource + QLatin1Char('/') + methodName;

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("r"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), messageId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource, methodName + QStringLiteral("_INPUT"));

    // WiFiEndpoint EPR — AddWiFiSettings only.
    if (methodName == QStringLiteral("AddWiFiSettings")) {
        w.writeStartElement(resource, QStringLiteral("WiFiEndpoint"));
        w.writeTextElement(QString::fromLatin1(kNsAddressing),
                            QStringLiteral("Address"),
                            QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
        w.writeStartElement(QString::fromLatin1(kNsAddressing),
                            QStringLiteral("ReferenceParameters"));
        w.writeTextElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("ResourceURI"),
                            QString::fromLatin1(kWiFiEndpointResource));
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("SelectorSet"));
        // CIM_WiFiEndpoint singleton on AMT — keyed by its standard
        // 4-part name. Match the read side's enumeration.
        for (const auto &kv : std::initializer_list<std::pair<QString, QString>>{
                 { QStringLiteral("Name"),                    QStringLiteral("WiFi Endpoint 0") },
                 { QStringLiteral("SystemCreationClassName"), QStringLiteral("CIM_ComputerSystem") },
                 { QStringLiteral("SystemName"),              QStringLiteral("Intel(r) AMT") },
                 { QStringLiteral("CreationClassName"),       QStringLiteral("CIM_WiFiEndpoint") },
             }) {
            w.writeStartElement(QString::fromLatin1(kNsWsman),
                                QStringLiteral("Selector"));
            w.writeAttribute(QStringLiteral("Name"), kv.first);
            w.writeCharacters(kv.second);
            w.writeEndElement(); // Selector
        }
        w.writeEndElement(); // SelectorSet
        w.writeEndElement(); // ReferenceParameters
        w.writeEndElement(); // WiFiEndpoint
    }

    // WiFiEndpointSettingsInput — the new/updated profile body.
    w.writeStartElement(resource, QStringLiteral("WiFiEndpointSettingsInput"));
    // AMT requires the InstanceID-shaped key to be the legacy
    // "Intel(r) AMT:WiFi Endpoint Settings <ElementName>" pattern.
    // Sending it on Update lets AMT match the existing row.
    w.writeTextElement(resource, QStringLiteral("ElementName"),
                        profile.elementName);
    w.writeTextElement(resource, QStringLiteral("InstanceID"),
                        QStringLiteral("Intel(r) AMT:WiFi Endpoint Settings %1")
                            .arg(profile.elementName));
    w.writeTextElement(resource, QStringLiteral("AuthenticationMethod"),
                        QString::number(profile.authenticationMethod));
    w.writeTextElement(resource, QStringLiteral("EncryptionMethod"),
                        QString::number(profile.encryptionMethod));
    w.writeTextElement(resource, QStringLiteral("SSID"), profile.ssid);
    w.writeTextElement(resource, QStringLiteral("Priority"),
                        QString::number(profile.priority));
    // PSK is only meaningful for the PSK auth methods (6 = WPA2-PSK,
    // 7 = WPA3-PSK). For enterprise methods AMT ignores it and looks
    // at the IEEE8021xSettingsInput instead. Always emit (omitting it
    // would change the wire shape vs Phase B and AMT happily accepts
    // a stale value when it isn't going to use it).
    w.writeTextElement(resource, QStringLiteral("PSKPassPhrase"),
                        profile.psk);
    w.writeEndElement(); // WiFiEndpointSettingsInput

    // Phase C (#223) — emit `IEEE8021xSettingsInput` and the two
    // credential EPRs when `enterpriseEnabled` is set. Order matches
    // the legacy MeshCommander emitter + go-wsman-messages: the EAP
    // settings come right after the WiFi settings, the credentials
    // are siblings, not nested.
    if (profile.enterpriseEnabled) {
        const QString cimNs = QStringLiteral(
            "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
            "CIM_IEEE8021xSettings");
        const QString certResource = QString::fromLatin1(kPublicKeyCertificateResource);

        // IEEE8021xSettingsInput — wraps the EAP-side knobs. The
        // outer element lives in the WiFi service namespace; the
        // inner fields (AuthenticationProtocol etc.) inherit CIM's
        // 8021x schema. AMT accepts both q: and the parent's r:
        // prefix; using r: keeps the namespace list short.
        w.writeStartElement(resource, QStringLiteral("IEEE8021xSettingsInput"));
        // ElementName + InstanceID echo the parent WiFi profile;
        // AMT correlates the two by ElementName.
        w.writeTextElement(resource, QStringLiteral("ElementName"),
                            profile.elementName);
        w.writeTextElement(resource, QStringLiteral("InstanceID"),
                            QStringLiteral("Intel(r) AMT:IEEE 802.1x Settings %1")
                                .arg(profile.elementName));
        w.writeTextElement(resource, QStringLiteral("AuthenticationProtocol"),
                            QString::number(profile.authenticationProtocol));
        // Username/Password only matter for PEAP / TTLS / EAP-FAST
        // (auth protocols 1..5). Emit always when non-empty —
        // harmless for EAP-TLS, simplifies the QML side.
        if (!profile.eapUsername.isEmpty()) {
            w.writeTextElement(resource, QStringLiteral("Username"),
                                profile.eapUsername);
        }
        if (!profile.eapPassword.isEmpty()) {
            w.writeTextElement(resource, QStringLiteral("Password"),
                                profile.eapPassword);
        }
        if (!profile.eapServerCertificateName.isEmpty()) {
            w.writeTextElement(resource, QStringLiteral("ServerCertificateName"),
                                profile.eapServerCertificateName);
            w.writeTextElement(resource,
                                QStringLiteral("ServerCertificateNameComparison"),
                                QString::number(profile.eapServerCertificateNameComparison));
        }
        w.writeEndElement(); // IEEE8021xSettingsInput

        // ClientCredential EPR — EAP-TLS only. Same EPR shape as
        // the cert-bind path in #222 (see
        // `buildTlsCredentialContextPutEnvelope`).
        auto writeCredentialEpr = [&](const QString &name,
                                       const QString &certInstanceId) {
            w.writeStartElement(resource, name);
            w.writeTextElement(QString::fromLatin1(kNsAddressing),
                                QStringLiteral("Address"),
                                QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
            w.writeStartElement(QString::fromLatin1(kNsAddressing),
                                QStringLiteral("ReferenceParameters"));
            w.writeTextElement(QString::fromLatin1(kNsWsman),
                                QStringLiteral("ResourceURI"), certResource);
            w.writeStartElement(QString::fromLatin1(kNsWsman),
                                QStringLiteral("SelectorSet"));
            w.writeStartElement(QString::fromLatin1(kNsWsman),
                                QStringLiteral("Selector"));
            w.writeAttribute(QStringLiteral("Name"),
                              QStringLiteral("InstanceID"));
            w.writeCharacters(certInstanceId);
            w.writeEndElement(); // Selector
            w.writeEndElement(); // SelectorSet
            w.writeEndElement(); // ReferenceParameters
            w.writeEndElement(); // <name>
        };

        if (!profile.clientCertificateInstanceId.isEmpty()) {
            writeCredentialEpr(QStringLiteral("ClientCredential"),
                                profile.clientCertificateInstanceId);
        }
        if (!profile.caCertificateInstanceId.isEmpty()) {
            writeCredentialEpr(QStringLiteral("CACredential"),
                                profile.caCertificateInstanceId);
        }
        Q_UNUSED(cimNs);
    }

    w.writeEndElement(); // <method>_INPUT
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();
    return out;
}

} // namespace

QByteArray buildWiFiSettingsEnvelopeForTesting(const QString &methodName,
                                                const WiFiPskProfile &profile,
                                                const QString &to,
                                                const QString &messageId)
{
    return buildWiFiSettingsEnvelope(methodName, profile, to, messageId);
}

void addWiFiSettingsPsk(WsmanClient *client, const WiFiPskProfile &profile,
                         std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildWiFiSettingsEnvelope(
        QStringLiteral("AddWiFiSettings"), profile,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        wirelessReturnValueExtractor(QStringLiteral("AddWiFiSettings")),
        std::move(callback));
}

void updateWiFiSettingsPsk(WsmanClient *client, const WiFiPskProfile &profile,
                            std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildWiFiSettingsEnvelope(
        QStringLiteral("UpdateWiFiSettings"), profile,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        wirelessReturnValueExtractor(QStringLiteral("UpdateWiFiSettings")),
        std::move(callback));
}

void deleteWiFiProfile(WsmanClient *client, const QString &elementName,
                        std::function<void(InvokeResult)> callback)
{
    // CIM_WiFiEndpointSettings is keyed by InstanceID on Intel AMT;
    // the legacy shape is "Intel(r) AMT:WiFi Endpoint Settings <name>".
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"),
                     QStringLiteral("Intel(r) AMT:WiFi Endpoint Settings %1")
                         .arg(elementName));
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kWiFiEndpointSettingsResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void deleteAllITWiFiProfiles(WsmanClient *client,
                              std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kWiFiPortConfigServiceResource),
        QStringLiteral("DeleteAllITProfiles"),
        /*selectors*/ {}, /*params*/ {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        wirelessReturnValueExtractor(QStringLiteral("DeleteAllITProfiles")),
        std::move(callback));
}

void deleteAllUserWiFiProfiles(WsmanClient *client,
                                std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kWiFiPortConfigServiceResource),
        QStringLiteral("DeleteAllUserProfiles"),
        /*selectors*/ {}, /*params*/ {},
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        wirelessReturnValueExtractor(QStringLiteral("DeleteAllUserProfiles")),
        std::move(callback));
}

void setWiFiPortState(WsmanClient *client, bool enabled,
                       std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    // 32768 = "Enabled in S0"  — the practical "on" value AMT exposes
    //   for WiFi (the legacy code's choice).
    // 3     = "Disabled".
    params.insert(QStringLiteral("RequestedState"),
                  QString::number(enabled ? 32768 : 3));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kWiFiPortResource),
        QStringLiteral("RequestStateChange"),
        wifiPortSelectors(), params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        wirelessReturnValueExtractor(
            QStringLiteral("CIM_WiFiPort.RequestStateChange")),
        std::move(callback));
}

void setWiFiSyncSettings(WsmanClient *client,
                          int localProfileSynchronization,
                          int uefiWiFiProfileShare,
                          std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> props;
    props.insert(QStringLiteral("LocalProfileSynchronizationEnabled"),
                 QString::number(localProfileSynchronization));
    props.insert(QStringLiteral("UEFIWiFiProfileShareEnabled"),
                 QString::number(uefiWiFiProfileShare));
    const QByteArray env = buildPutEnvelope(
        QString::fromLatin1(kWiFiPortConfigServiceResource),
        QStringLiteral("AMT_WiFiPortConfigurationService"),
        /*selectors*/ {}, props,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            // Put has no ReturnValue scalar; absence of fault = success.
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void setWired8021xProfile(WsmanClient *client, bool enabled,
                           int authenticationProtocol,
                           std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> props;
    props.insert(QStringLiteral("Enabled"),
                 enabled ? QStringLiteral("true") : QStringLiteral("false"));
    props.insert(QStringLiteral("AuthenticationProtocol"),
                 QString::number(authenticationProtocol));
    const QByteArray env = buildPutEnvelope(
        QString::fromLatin1(kWired8021xProfileResource),
        QStringLiteral("AMT_8021XProfile"),
        /*selectors*/ {}, props,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void setEnvironmentDetection(WsmanClient *client,
                              const QStringList &detectionStrings,
                              std::function<void(InvokeResult)> callback)
{
    // DetectionStrings is a repeated XML element; hand-roll the Put
    // envelope so we can emit one <DetectionStrings>…</DetectionStrings>
    // child per entry. AMT validates the rest of the record server-
    // side; ElementName and the InstanceID are firmware-fixed and not
    // sent back.
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource =
        QString::fromLatin1(kEnvironmentDetectionResource);
    const QString to = client ? client->endpoint().toString() : QString();
    const QString msgId = newMessageId();

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("r"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/09/transfer/Put"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), msgId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing), QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource,
                         QStringLiteral("AMT_EnvironmentDetectionSettingData"));
    for (const QString &s : detectionStrings)
        w.writeTextElement(resource, QStringLiteral("DetectionStrings"), s);
    w.writeEndElement(); // AMT_EnvironmentDetectionSettingData
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();

    runRequest<InvokeResult>(client, out, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

namespace {

ActiveRedirectionSession parseActiveSessionRow(const QByteArray &item)
{
    ActiveRedirectionSession s;
    s.sourceAddress = findScalar(item, QStringLiteral("SourceAddress"));
    bool conv = false;
    const int p = findScalar(item, QStringLiteral("SourcePort")).toInt(&conv);
    if (conv) s.sourcePort = p;
    // The Dependent / Antecedent EPRs both carry a SAP InstanceID — pull
    // the first non-empty one we see. Useful for differentiating
    // multiple-of-the-same-kind sessions (rare on AMT but possible).
    s.sessionInstanceId = findScalar(item, QStringLiteral("InstanceID"));
    return s;
}

} // namespace

void getActiveSessions(WsmanClient *client,
                       std::function<void(ActiveSessionsResult)> callback)
{
    enum class Kind { Sol, Kvm, Ider, _Count };
    constexpr int kCount = int(Kind::_Count);
    struct State {
        std::array<bool, kCount> done{};
        std::array<QList<QByteArray>, kCount> items;
        std::array<QString, kCount> errors;
        std::function<void(ActiveSessionsResult)> cb;
    };
    auto st = std::make_shared<State>();
    st->cb = std::move(callback);

    auto maybeFire = [st]() mutable {
        for (bool d : st->done)
            if (!d) return;
        ActiveSessionsResult r;
        auto collect = [&](const QList<QByteArray> &items,
                            QList<ActiveRedirectionSession> &out) {
            for (const QByteArray &it : items)
                out.append(parseActiveSessionRow(it));
        };
        collect(st->items[int(Kind::Sol)],  r.sol);
        collect(st->items[int(Kind::Kvm)],  r.kvm);
        collect(st->items[int(Kind::Ider)], r.ider);
        // Whole-call ok unless every enumeration faulted with the same
        // error — partial faults just leave that channel empty.
        const auto &errs = st->errors;
        r.ok = !(errs[0].length() && errs[1].length() && errs[2].length()
                 && errs[0] == errs[1] && errs[1] == errs[2]);
        if (!r.ok) r.error = errs[0];
        st->cb(std::move(r));
    };

    auto kick = [client, st, maybeFire](Kind k, const char *uri) {
        enumerateAll(client, uri,
            [st, k, maybeFire](QList<QByteArray> items, QString error) mutable {
                const int idx = int(k);
                st->items[idx]  = std::move(items);
                st->errors[idx] = error;
                st->done[idx]   = true;
                maybeFire();
            });
    };

    kick(Kind::Sol,  kSolSessionUsingPortResource);
    kick(Kind::Kvm,  kKvmSessionUsingPortResource);
    kick(Kind::Ider, kIderSessionUsingPortResource);
}

void setUserInitiatedConnectionState(WsmanClient *client, int requestedState,
                                      std::function<void(InvokeResult)> callback)
{
    // CIM-style 4-part selectors for the singleton service.
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"),
                     QStringLiteral("Intel(r) AMT User Initiated Connection Service"));
    selectors.insert(QStringLiteral("SystemCreationClassName"),
                     QStringLiteral("CIM_ComputerSystem"));
    selectors.insert(QStringLiteral("SystemName"), QStringLiteral("Intel(r) AMT"));
    selectors.insert(QStringLiteral("CreationClassName"),
                     QStringLiteral("AMT_UserInitiatedConnectionService"));
    QHash<QString, QString> params;
    params.insert(QStringLiteral("RequestedState"),
                  QString::number(requestedState));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kUserInitiatedConnectionResource),
        QStringLiteral("RequestStateChange"),
        selectors, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            if (rv.isEmpty()) {
                r.error = QStringLiteral(
                    "AMT_UserInitiatedConnectionService.RequestStateChange:"
                    " no ReturnValue");
                return;
            }
            bool conv = false;
            r.returnValue = rv.toInt(&conv);
            // 0 = Completed, 4096 = Method Parameters Checked - Job Started.
            r.ok = conv && (r.returnValue == 0 || r.returnValue == 4096);
            if (!r.ok)
                r.error = QStringLiteral("RequestStateChange returned %1").arg(rv);
        },
        std::move(callback));
}

void addMpServer(WsmanClient *client, const MpServerInput &input,
                  std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> params;
    params.insert(QStringLiteral("AccessInfo"),  input.accessInfo);
    params.insert(QStringLiteral("InfoFormat"),  QString::number(input.infoFormat));
    params.insert(QStringLiteral("Port"),        QString::number(input.port));
    params.insert(QStringLiteral("AuthMethod"),  QString::number(input.authMethod));
    if (input.authMethod != 1) {
        params.insert(QStringLiteral("Username"), input.username);
        params.insert(QStringLiteral("Password"), input.password);
    }
    params.insert(QStringLiteral("CommonName"),  input.commonName);
    params.insert(QStringLiteral("MpsType"),     QString::number(input.mpsType));
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kRemoteAccessServiceResource),
        QStringLiteral("AddMpServer"),
        /*selectors*/ {}, params,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            r.returnValue = rv.toInt();
            r.ok = (r.returnValue == 0);
            if (!r.ok) {
                r.error = QStringLiteral("AddMpServer returned %1").arg(
                    rv.isEmpty() ? QStringLiteral("(no ReturnValue)") : rv);
            }
        },
        std::move(callback));
}

void updateMpServer(WsmanClient *client, const QString &name,
                    const QString &accessInfo, int infoFormat, int port,
                    const QString &commonName,
                    std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"), name);
    QHash<QString, QString> props;
    props.insert(QStringLiteral("Name"),        name);
    props.insert(QStringLiteral("AccessInfo"),  accessInfo);
    props.insert(QStringLiteral("InfoFormat"),  QString::number(infoFormat));
    props.insert(QStringLiteral("Port"),        QString::number(port));
    props.insert(QStringLiteral("CN"),          commonName);
    const QByteArray env = buildPutEnvelope(
        QString::fromLatin1(kManagementPresenceRemoteSapResource),
        QStringLiteral("AMT_ManagementPresenceRemoteSAP"),
        selectors, props,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void removeMpServer(WsmanClient *client, const QString &name,
                     std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("Name"), name);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kManagementPresenceRemoteSapResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

void decodeExtendedData(const QString &b64, RemoteAccessPolicy &p)
{
    const QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
    if (raw.size() < 8) return;
    const auto be32 = [](const QByteArray &b, int o) {
        if (o + 3 >= b.size()) return quint32{0};
        return (quint32(quint8(b[o]))     << 24)
             | (quint32(quint8(b[o + 1])) << 16)
             | (quint32(quint8(b[o + 2])) <<  8)
             |  quint32(quint8(b[o + 3]));
    };
    const quint32 type = be32(raw, 0);
    if (type == 0) {
        p.periodicInterval = true;
        p.periodicSeconds  = int(be32(raw, 4));
    } else if (type == 1 && raw.size() >= 12) {
        p.periodicTimeOfDay = true;
        p.periodicHour   = int(be32(raw, 4));
        p.periodicMinute = int(be32(raw, 8));
    }
}

QString buildExtendedData(bool intervalMode, int intervalSeconds,
                          int hour, int minute)
{
    const auto append32 = [](QByteArray &b, quint32 v) {
        b.append(char((v >> 24) & 0xFF));
        b.append(char((v >> 16) & 0xFF));
        b.append(char((v >>  8) & 0xFF));
        b.append(char( v        & 0xFF));
    };
    QByteArray raw;
    if (intervalMode) {
        raw.reserve(8);
        append32(raw, 0u);
        // Clamp at uint32 — AMT's field is 32 bits and the editor's
        // QML can't produce a negative seconds value anyway.
        append32(raw, intervalSeconds < 0 ? 0u : quint32(intervalSeconds));
    } else {
        raw.reserve(12);
        append32(raw, 1u);
        append32(raw, hour   < 0 ? 0u : quint32(hour));
        append32(raw, minute < 0 ? 0u : quint32(minute));
    }
    return QString::fromLatin1(raw.toBase64());
}

void addRemoteAccessPolicyRule(WsmanClient *client,
                                const CiraPolicyInput &policy,
                                std::function<void(InvokeResult)> callback)
{
    // Hand-rolled envelope. AMT's `AddRemoteAccessPolicyRule` carries
    // two EPR-shaped array parameters (`CIRAServers[]`, `CILAServers[]`)
    // pointing at `AMT_ManagementPresenceRemoteSAP` rows; the generic
    // `buildInvokeEnvelope` helper handles flat string params only.
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);

    constexpr char kNsSoap[] = "http://www.w3.org/2003/05/soap-envelope";
    constexpr char kNsAddressing[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing";
    constexpr char kNsWsman[] = "http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd";
    const QString resource =
        QString::fromLatin1(kRemoteAccessServiceResource);
    const QString action  = resource + QStringLiteral("/AddRemoteAccessPolicyRule");
    const QString sapResource =
        QString::fromLatin1(kManagementPresenceRemoteSapResource);
    const QString to     = client ? client->endpoint().toString() : QString();
    const QString msgId  = newMessageId();

    w.writeStartDocument();
    w.writeNamespace(QString::fromLatin1(kNsSoap),       QStringLiteral("s"));
    w.writeNamespace(QString::fromLatin1(kNsAddressing), QStringLiteral("a"));
    w.writeNamespace(QString::fromLatin1(kNsWsman),      QStringLiteral("w"));
    w.writeNamespace(resource,                            QStringLiteral("r"));
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Envelope"));

    // Header
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Header"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Action"), action);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("To"), to);
    w.writeTextElement(QString::fromLatin1(kNsWsman),
                        QStringLiteral("ResourceURI"), resource);
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("MessageID"), msgId);
    w.writeStartElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("ReplyTo"));
    w.writeTextElement(QString::fromLatin1(kNsAddressing),
                        QStringLiteral("Address"),
                        QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
    w.writeEndElement(); // ReplyTo
    w.writeEndElement(); // Header

    // Body
    w.writeStartElement(QString::fromLatin1(kNsSoap), QStringLiteral("Body"));
    w.writeStartElement(resource,
                         QStringLiteral("AddRemoteAccessPolicyRule_INPUT"));

    // AMT's XSD requires this exact ordering: Trigger → TunnelLifeTime
    // → ExtendedData → MpServer[]. Reordering yields SchemaValidationError.
    w.writeTextElement(resource, QStringLiteral("Trigger"),
                        QString::number(policy.trigger));
    w.writeTextElement(resource, QStringLiteral("TunnelLifeTime"),
                        QString::number(policy.tunnelLifeTime));

    // ExtendedData is only meaningful for the Periodic trigger; the
    // other two ignore it but still require the element to be present.
    QString extData;
    if (policy.trigger == 2) {
        if (policy.periodicInterval) {
            extData = buildExtendedData(true, policy.periodicSeconds, 0, 0);
        } else if (policy.periodicTimeOfDay) {
            extData = buildExtendedData(false, 0,
                                        policy.periodicHour,
                                        policy.periodicMinute);
        }
    }
    w.writeTextElement(resource, QStringLiteral("ExtendedData"), extData);

    // CIRA / CILA servers go in two separately-named repeating EPR
    // arrays. AMT keys the bucket off the element name: <MpServer>
    // for CIRA, <InternalMpServer> for CILA. Each EPR points at an
    // existing AMT_ManagementPresenceRemoteSAP row by Name selector.
    const auto writeMpServerEpr = [&](const QString &tag, const QString &name) {
        w.writeStartElement(resource, tag);
        w.writeTextElement(QString::fromLatin1(kNsAddressing),
                            QStringLiteral("Address"),
                            QStringLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));
        w.writeStartElement(QString::fromLatin1(kNsAddressing),
                            QStringLiteral("ReferenceParameters"));
        w.writeTextElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("ResourceURI"), sapResource);
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("SelectorSet"));
        w.writeStartElement(QString::fromLatin1(kNsWsman),
                            QStringLiteral("Selector"));
        w.writeAttribute(QStringLiteral("Name"), QStringLiteral("Name"));
        w.writeCharacters(name);
        w.writeEndElement(); // Selector
        w.writeEndElement(); // SelectorSet
        w.writeEndElement(); // ReferenceParameters
        w.writeEndElement(); // <MpServer> / <InternalMpServer>
    };
    for (const QString &n : policy.ciraMpsNames)
        writeMpServerEpr(QStringLiteral("MpServer"), n);
    for (const QString &n : policy.cilaMpsNames)
        writeMpServerEpr(QStringLiteral("InternalMpServer"), n);

    w.writeEndElement(); // AddRemoteAccessPolicyRule_INPUT
    w.writeEndElement(); // Body

    w.writeEndElement(); // Envelope
    w.writeEndDocument();

    runRequest<InvokeResult>(client, out, {},
        [](const QByteArray &body, InvokeResult &r) {
            const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
            r.returnValue = rv.toInt();
            r.ok = (r.returnValue == 0);
            if (!r.ok) {
                r.error = QStringLiteral("AddRemoteAccessPolicyRule returned %1")
                    .arg(rv.isEmpty() ? QStringLiteral("(no ReturnValue)") : rv);
            }
        },
        std::move(callback));
}

void removeRemoteAccessPolicyRule(WsmanClient *client,
                                   const QString &policyRuleName,
                                   std::function<void(InvokeResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("PolicyRuleName"), policyRuleName);
    const QByteArray env = buildDeleteEnvelope(
        QString::fromLatin1(kRemoteAccessPolicyRuleResource), selectors,
        client ? client->endpoint().toString() : QString(), newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray & /*body*/, InvokeResult &r) {
            r.returnValue = 0;
            r.ok = true;
        },
        std::move(callback));
}

namespace {

// Shared parser for a "ClearLog" reply. AMT's ClearLog methods are
// parameterless and return a single `ReturnValue` element — 0 means
// the firmware accepted the clear. The parsed result fills the same
// `InvokeResult` shape as every other invoke op so the batch runner
// can treat them uniformly.
void parseClearLogReply(const QByteArray &body, InvokeResult &r,
                        const QString &methodLabel)
{
    const QString rv = findScalar(body, QStringLiteral("ReturnValue"));
    if (rv.isEmpty()) {
        r.error = QStringLiteral("response had no ReturnValue");
        return;
    }
    bool conv = false;
    r.returnValue = rv.toInt(&conv);
    r.ok = conv && r.returnValue == 0;
    if (!r.ok && r.error.isEmpty())
        r.error = QStringLiteral("%1 returned %2").arg(methodLabel, rv);
}

} // namespace

void clearAuditLog(WsmanClient *client,
                   std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kAuditLogResource),
        QStringLiteral("ClearLog"), {}, {},
        client ? client->endpoint().toString() : QString(),
        newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            parseClearLogReply(body, r, QStringLiteral("AMT_AuditLog.ClearLog"));
        },
        std::move(callback));
}

void clearEventLog(WsmanClient *client,
                   std::function<void(InvokeResult)> callback)
{
    const QByteArray env = buildInvokeEnvelope(
        QString::fromLatin1(kMessageLogResource),
        QStringLiteral("ClearLog"), {}, {},
        client ? client->endpoint().toString() : QString(),
        newMessageId());
    runRequest<InvokeResult>(client, env, {},
        [](const QByteArray &body, InvokeResult &r) {
            parseClearLogReply(body, r, QStringLiteral("AMT_MessageLog.ClearLog"));
        },
        std::move(callback));
}

} // namespace qumesh::wsman
