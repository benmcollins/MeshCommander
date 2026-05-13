#include "wsman/operations.h"

#include "wsman/soap_envelope.h"
#include "wsman/wsman_client.h"

#include <QNetworkReply>
#include <QObject>
#include <QUuid>

namespace meshcommander::wsman {

namespace {

constexpr char kPowerMgmtResource[] =
    "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/"
    "CIM_AssociatedPowerManagementService";

QString newMessageId()
{
    return QStringLiteral("uuid:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
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
    if (client == nullptr) {
        callback(PowerStateResult{false, QStringLiteral("client is null"), -1});
        return;
    }
    const QByteArray env = buildGetEnvelope(QString::fromLatin1(kPowerMgmtResource), {},
                                            client->endpoint().toString(), newMessageId());
    QNetworkReply *reply = client->sendEnvelope(env);

    QObject::connect(reply, &QNetworkReply::finished, client,
                     [reply, cb = std::move(callback)]() mutable {
                         PowerStateResult r;
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
                         const QString ps = findScalar(soap.bodyXml,
                                                       QStringLiteral("PowerState"));
                         if (ps.isEmpty()) {
                             r.error = QStringLiteral("response had no PowerState element");
                             cb(std::move(r));
                             return;
                         }
                         bool conv = false;
                         r.powerState = ps.toInt(&conv);
                         r.ok = conv;
                         if (!conv) r.error = QStringLiteral("PowerState '%1' was not numeric").arg(ps);
                         cb(std::move(r));
                     });
}

} // namespace meshcommander::wsman
