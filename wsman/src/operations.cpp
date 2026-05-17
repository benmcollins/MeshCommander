// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include "wsman/soap_envelope.h"
#include "wsman/wsman_client.h"


#include <QObject>
#include <QUuid>
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

constexpr char kAccountResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_Account";

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

void getEthernetSettings(WsmanClient *client,
                         std::function<void(EthernetSettingsResult)> callback)
{
    QHash<QString, QString> selectors;
    selectors.insert(QStringLiteral("InstanceID"),
                     QStringLiteral("Intel(r) AMT Ethernet Port Settings 0"));
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kEthernetSettingsResource),
                                             selectors,
                                             client ? client->endpoint().toString() : QString(),
                                             newMessageId());
    runRequest<EthernetSettingsResult>(client, env, {},
        [](const QByteArray &body, EthernetSettingsResult &r) {
            r.macAddress       = findScalar(body, QStringLiteral("MACAddress"));
            r.dhcpEnabled      = findScalar(body, QStringLiteral("DHCPEnabled")) == QStringLiteral("true");
            r.ipv4Enabled      = findScalar(body, QStringLiteral("IpSyncEnabled")) == QStringLiteral("true")
                                  || !findScalar(body, QStringLiteral("IPAddress")).isEmpty();
            r.ipAddress        = findScalar(body, QStringLiteral("IPAddress"));
            r.subnetMask       = findScalar(body, QStringLiteral("SubnetMask"));
            r.defaultGateway   = findScalar(body, QStringLiteral("DefaultGateway"));
            r.primaryDns       = findScalar(body, QStringLiteral("PrimaryDNS"));
            r.secondaryDns     = findScalar(body, QStringLiteral("SecondaryDNS"));
            r.linkPolicy       = findScalar(body, QStringLiteral("LinkPolicy"));
            r.ok = !r.macAddress.isEmpty();
            if (!r.ok)
                r.error = QStringLiteral("AMT_EthernetPortSettings body had no MACAddress");
        },
        std::move(callback));
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

void enumerateUserAccounts(WsmanClient *client,
                            std::function<void(UserAccountsResult)> callback)
{
    struct Acc { QList<UserAccount> items; };
    auto acc = std::make_shared<Acc>();
    auto onDone = std::make_shared<std::function<void(QString)>>();
    auto cb = std::make_shared<std::function<void(UserAccountsResult)>>(std::move(callback));

    *onDone = [acc, cb](QString error) {
        UserAccountsResult r;
        r.ok = error.isEmpty();
        r.error = std::move(error);
        r.accounts = std::move(acc->items);
        (*cb)(std::move(r));
    };

    auto pullStep = std::make_shared<std::function<void(const QString &)>>();
    *pullStep = [client, acc, pullStep, onDone](const QString &context) mutable {
        const QByteArray env = buildPullEnvelope(QString::fromLatin1(kAccountResource),
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
                    UserAccount u;
                    u.instanceID  = findScalar(item, QStringLiteral("InstanceID"));
                    u.name        = findScalar(item, QStringLiteral("Name"));
                    u.elementName = findScalar(item, QStringLiteral("ElementName"));
                    const QString es = findScalar(item, QStringLiteral("EnabledState"));
                    // CIM EnabledState: 2 = Enabled. Anything else (3/4
                    // disabled, 5 not applicable, etc.) we surface as
                    // disabled in the QML.
                    u.enabled = (es == QStringLiteral("2"));
                    acc->items.append(u);
                }
                if (chunk.endOfSequence || chunk.enumerationContext.isEmpty()) {
                    (*onDone)({});
                    return;
                }
                (*pullStep)(chunk.enumerationContext);
            });
    };

    if (client == nullptr) { (*onDone)(QStringLiteral("client is null")); return; }
    const QByteArray env = buildEnumerateEnvelope(QString::fromLatin1(kAccountResource),
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

} // namespace qumesh::wsman
