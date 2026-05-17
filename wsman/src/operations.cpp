// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include "wsman/soap_envelope.h"
#include "wsman/wsman_client.h"


#include <QObject>
#include <QUuid>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

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

constexpr char kEventLogEntryResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/"
    "AMT_EventLogEntry";

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

constexpr char kAuditLogResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuditLog";

constexpr char kAuthorizationServiceResource[] =
    "http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService";

constexpr char kIpv6PortSettingsResource[] =
    "http://intel.com/wbem/wscim/1/ips-schema/1/IPS_IPv6PortSettings";

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
            r.biosSetup           = truthy(QStringLiteral("BIOSSetup"));
            r.biosPause           = truthy(QStringLiteral("BIOSPause"));
            r.secureErase         = truthy(QStringLiteral("SecureErase"));
            r.forceUefiHttpsBoot  = truthy(QStringLiteral("ForceUEFIHTTPSBoot"));
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
    // CIM_SoftwareIdentity is a collection — `Enumerate` + `Pull`,
    // then pick the row whose InstanceID is "AMT". A `Get` with an
    // InstanceID selector works on some firmware but not all; the
    // legacy code enumerates the lot, so we mirror that for safety.
    struct Acc { QString version; bool found = false; };
    auto acc = std::make_shared<Acc>();
    auto onDone = std::make_shared<std::function<void(QString)>>();
    auto cb = std::make_shared<std::function<void(MeVersionResult)>>(std::move(callback));

    *onDone = [acc, cb](QString error) {
        MeVersionResult r;
        r.versionString = acc->version;
        r.ok = error.isEmpty() && acc->found;
        if (!r.ok && error.isEmpty())
            error = QStringLiteral(
                "CIM_SoftwareIdentity enumeration had no InstanceID='AMT'");
        r.error = std::move(error);
        (*cb)(std::move(r));
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
                    if (id == QStringLiteral("AMT")) {
                        acc->version = findScalar(item, QStringLiteral("VersionString"));
                        acc->found = true;
                        break;
                    }
                }
                if (acc->found || chunk.endOfSequence
                    || chunk.enumerationContext.isEmpty()) {
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

void enumerateEventLog(WsmanClient *client,
                       std::function<void(EventLogResult)> callback)
{
    // Wrap the templated runner with a closure that fills the right
    // result field. The runner template's generic `r.entries` line
    // would clash with the User-accounts result that uses `accounts`,
    // so we keep `done` as a thin shim.
    struct Acc { QList<EventLogEntry> items; };
    auto acc = std::make_shared<Acc>();
    auto onDone = std::make_shared<std::function<void(QString)>>();
    auto cb = std::make_shared<std::function<void(EventLogResult)>>(std::move(callback));

    *onDone = [acc, cb](QString error) {
        EventLogResult r;
        r.ok = error.isEmpty();
        r.error = std::move(error);
        r.entries = std::move(acc->items);
        (*cb)(std::move(r));
    };

    auto pullStep = std::make_shared<std::function<void(const QString &)>>();
    *pullStep = [client, acc, pullStep, onDone](const QString &context) mutable {
        const QByteArray env = buildPullEnvelope(QString::fromLatin1(kEventLogEntryResource),
                                                  context, 64,
                                                  client->endpoint().toString(),
                                                  newMessageId());
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
                    EventLogEntry e;
                    e.recordId  = findScalar(item, QStringLiteral("RecordID"));
                    e.timestamp = findScalar(item, QStringLiteral("CreationTimeStamp"));
                    e.severity  = findScalar(item, QStringLiteral("Severity"));
                    // Message is split across several fields in the
                    // schema; the most user-facing one is "Message"
                    // (free text). Fall back to "Description" otherwise.
                    e.message = findScalar(item, QStringLiteral("Message"));
                    if (e.message.isEmpty())
                        e.message = findScalar(item, QStringLiteral("Description"));
                    acc->items.append(e);
                }
                if (chunk.endOfSequence || chunk.enumerationContext.isEmpty()) {
                    (*onDone)({});
                    return;
                }
                (*pullStep)(chunk.enumerationContext);
            });
    };

    if (client == nullptr) { (*onDone)(QStringLiteral("client is null")); return; }
    const QByteArray env = buildEnumerateEnvelope(QString::fromLatin1(kEventLogEntryResource),
                                                   client->endpoint().toString(),
                                                   newMessageId());
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

} // namespace qumesh::wsman
