// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include "wsman/soap_envelope.h"
#include "wsman/wsman_client.h"

#include <QNetworkReply>
#include <QObject>
#include <QUuid>
#include <QXmlStreamWriter>

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
    QNetworkReply *reply = client->sendEnvelope(envelope);
    QObject::connect(reply, &QNetworkReply::finished, client,
                     [reply, zero = std::move(zero), extract = std::forward<Extract>(extract),
                       cb = std::move(callback)]() mutable {
                         ResultT r = zero;
                         const QByteArray body = reply->readAll();
                         const auto err = reply->error();
                         const auto errString = reply->errorString();
                         reply->deleteLater();
                         if (err != QNetworkReply::NoError) {
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
    QNetworkReply *reply = client->sendEnvelope(env);

    QObject::connect(reply, &QNetworkReply::finished, client,
                     [reply, cb = std::move(callback)]() mutable {
                         IdentifyResult r;
                         const QByteArray body = reply->readAll();
                         const auto err = reply->error();
                         const auto errString = reply->errorString();
                         reply->deleteLater();
                         if (err != QNetworkReply::NoError) {
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
            // PlatformErase is a bitmask in newer firmware; "true" / "1" /
            // non-zero numeric all count as supported.
            const QString pe = findScalar(body, QStringLiteral("PlatformErase"));
            r.platformErase = (pe == QStringLiteral("true"))
                              || (!pe.isEmpty() && pe != QStringLiteral("0")
                                  && pe != QStringLiteral("false"));
            r.ok = true;
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
    QNetworkReply *reply = client->sendEnvelope(envelope);
    QObject::connect(reply, &QNetworkReply::finished, client,
                     [reply, name, extract = std::forward<ExtractRv>(extract),
                       onError = std::move(onError), next = std::move(next)]() mutable {
                         const QByteArray body = reply->readAll();
                         const auto err = reply->error();
                         const auto errString = reply->errorString();
                         reply->deleteLater();
                         if (err != QNetworkReply::NoError) {
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

} // namespace qumesh::wsman
