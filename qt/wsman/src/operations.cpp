// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"

#include "wsman/soap_envelope.h"
#include "wsman/wsman_client.h"

#include <QNetworkReply>
#include <QObject>
#include <QUuid>

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
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kComputerSystemResource),
                                             {},
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
