// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/operations.h"
#include "wsman/wsman_client.h"

#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QtTest>

using namespace qumesh::wsman;

class TestWsmanClient : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void identifyRoundTripsAgainstMockServer();
    void getPowerStateRoundTripsAgainstMockServer();
    void httpErrorPropagates();
    void getGeneralSettingsExtractsPowerSource();
    void getSetupAndConfigurationDecodesProvisioning();
    void getMeVersionPicksAmtInstanceFromEnumeration();
    void getRedirectionStatusSplitsEnabledStateBitmask();

private:
    QUrl endpointFor(quint16 port) const;
};

namespace {

constexpr char kIdentifyResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsmid=\"http://schemas.dmtf.org/wbem/wsman/identity/1/wsmanidentity.xsd\">"
    "<s:Header/><s:Body>"
    "<wsmid:IdentifyResponse>"
    "<wsmid:ProtocolVersion>http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd</wsmid:ProtocolVersion>"
    "<wsmid:ProductVendor>Intel(r) AMT</wsmid:ProductVendor>"
    "<wsmid:ProductVersion>16.0.0</wsmid:ProductVersion>"
    "</wsmid:IdentifyResponse>"
    "</s:Body></s:Envelope>";

constexpr char kPowerStateResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
    " xmlns:cim=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_AssociatedPowerManagementService\">"
    "<s:Header><a:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/GetResponse</a:Action></s:Header>"
    "<s:Body>"
    "<cim:CIM_AssociatedPowerManagementService>"
    "<cim:PowerState>2</cim:PowerState>"
    "</cim:CIM_AssociatedPowerManagementService>"
    "</s:Body></s:Envelope>";

constexpr char kGeneralSettingsResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:g=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_GeneralSettings\">"
    "<s:Header/><s:Body>"
    "<g:AMT_GeneralSettings>"
    "<g:HostName>laptop-7</g:HostName>"
    "<g:DomainName>example.com</g:DomainName>"
    "<g:DigestRealm>Digest:0123</g:DigestRealm>"
    "<g:PowerSource>1</g:PowerSource>"
    "</g:AMT_GeneralSettings>"
    "</s:Body></s:Envelope>";

constexpr char kSetupAndConfigCcmResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:c=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_SetupAndConfigurationService\">"
    "<s:Header/><s:Body>"
    "<c:AMT_SetupAndConfigurationService>"
    "<c:ProvisioningState>2</c:ProvisioningState>"
    "<c:ProvisioningMode>4</c:ProvisioningMode>"
    "</c:AMT_SetupAndConfigurationService>"
    "</s:Body></s:Envelope>";

constexpr char kEnumerateResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\">"
    "<s:Header/><s:Body>"
    "<wsen:EnumerateResponse>"
    "<wsen:EnumerationContext>ctx-1</wsen:EnumerationContext>"
    "</wsen:EnumerateResponse>"
    "</s:Body></s:Envelope>";

constexpr char kSoftwareIdentityPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_SoftwareIdentity\">"
    "<s:Header/><s:Body>"
    "<wsen:PullResponse>"
    "<wsen:Items>"
    "<c:CIM_SoftwareIdentity>"
    "<c:InstanceID>BIOS</c:InstanceID>"
    "<c:VersionString>1.2.3</c:VersionString>"
    "</c:CIM_SoftwareIdentity>"
    "<c:CIM_SoftwareIdentity>"
    "<c:InstanceID>AMT</c:InstanceID>"
    "<c:VersionString>16.1.25</c:VersionString>"
    "</c:CIM_SoftwareIdentity>"
    "<c:CIM_SoftwareIdentity>"
    "<c:InstanceID>Sku</c:InstanceID>"
    "<c:VersionString>X</c:VersionString>"
    "</c:CIM_SoftwareIdentity>"
    "</wsen:Items>"
    "<wsen:EndOfSequence/>"
    "</wsen:PullResponse>"
    "</s:Body></s:Envelope>";

constexpr char kRedirectionServiceResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:r=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_RedirectionService\">"
    "<s:Header/><s:Body>"
    "<r:AMT_RedirectionService>"
    "<r:ListenerEnabled>true</r:ListenerEnabled>"
    // EnabledState 32771 = base 32768 + 1 (IDE-R) + 2 (SOL) — same shape
    // the legacy app writes on toggle (`legacy/source/Commander.htm`
    // line ~50098).
    "<r:EnabledState>32771</r:EnabledState>"
    "</r:AMT_RedirectionService>"
    "</s:Body></s:Envelope>";

constexpr char kKvmSapEnabledResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:k=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_KVMRedirectionSAP\">"
    "<s:Header/><s:Body>"
    "<k:CIM_KVMRedirectionSAP>"
    "<k:EnabledState>6</k:EnabledState>"
    "</k:CIM_KVMRedirectionSAP>"
    "</s:Body></s:Envelope>";

} // namespace

void TestWsmanClient::initTestCase()
{
    // QHttpServer requires the event loop; QTEST_GUILESS_MAIN provides one.
}

QUrl TestWsmanClient::endpointFor(quint16 port) const
{
    QUrl u;
    u.setScheme(QStringLiteral("http"));
    u.setHost(QStringLiteral("127.0.0.1"));
    u.setPort(port);
    u.setPath(QStringLiteral("/wsman"));
    return u;
}

void TestWsmanClient::identifyRoundTripsAgainstMockServer()
{
    QHttpServer server;
    QByteArray receivedBody;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &req) {
                     receivedBody = req.body();
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                QByteArray(kIdentifyResponse));
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release(); // QHttpServer takes ownership

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    IdentifyResult result;
    QEventLoop loop;
    identify(&client, [&](IdentifyResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.productVendor, QStringLiteral("Intel(r) AMT"));
    QCOMPARE(result.productVersion, QStringLiteral("16.0.0"));
    QVERIFY(receivedBody.contains("Identify"));
}

void TestWsmanClient::getPowerStateRoundTripsAgainstMockServer()
{
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &) {
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                QByteArray(kPowerStateResponse));
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    PowerStateResult result;
    QEventLoop loop;
    getPowerState(&client, [&](PowerStateResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.powerState, 2);
}

void TestWsmanClient::httpErrorPropagates()
{
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest &) {
                     return QHttpServerResponse(QHttpServerResponder::StatusCode::InternalServerError);
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    IdentifyResult result;
    QEventLoop loop;
    identify(&client, [&](IdentifyResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

void TestWsmanClient::getGeneralSettingsExtractsPowerSource()
{
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &) {
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                QByteArray(kGeneralSettingsResponse));
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    GeneralSettingsResult result;
    QEventLoop loop;
    getGeneralSettings(&client, [&](GeneralSettingsResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.hostName, QStringLiteral("laptop-7"));
    QCOMPARE(result.powerSource, 1);
}

void TestWsmanClient::getSetupAndConfigurationDecodesProvisioning()
{
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &) {
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                QByteArray(kSetupAndConfigCcmResponse));
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    SetupAndConfigResult result;
    QEventLoop loop;
    getSetupAndConfiguration(&client, [&](SetupAndConfigResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.provisioningState, 2);
    QCOMPARE(result.provisioningMode, 4);
}

void TestWsmanClient::getMeVersionPicksAmtInstanceFromEnumeration()
{
    // The CIM_SoftwareIdentity enumeration POSTs twice — Enumerate then
    // Pull. The mock switches on the action header (cheaper than parsing
    // the body for `<wsen:Enumerate>` vs `<wsen:Pull>`).
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &req) {
                     const QByteArray body = req.body();
                     const QByteArray response = body.contains("<wsen:Pull")
                         || body.contains(":Pull>")
                             ? QByteArray(kSoftwareIdentityPullResponse)
                             : QByteArray(kEnumerateResponse);
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                response);
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    MeVersionResult result;
    QEventLoop loop;
    getMeVersion(&client, [&](MeVersionResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.versionString, QStringLiteral("16.1.25"));
}

void TestWsmanClient::getRedirectionStatusSplitsEnabledStateBitmask()
{
    // Two Gets — AMT_RedirectionService and CIM_KVMRedirectionSAP. The
    // request body carries the resource URI; route on it.
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &req) {
                     const QByteArray body = req.body();
                     const QByteArray response =
                         body.contains("CIM_KVMRedirectionSAP")
                             ? QByteArray(kKvmSapEnabledResponse)
                             : QByteArray(kRedirectionServiceResponse);
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                response);
                 });

    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    RedirectionStatusResult result;
    QEventLoop loop;
    getRedirectionStatus(&client, [&](RedirectionStatusResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(result.redirectionListenerEnabled);
    QVERIFY(result.solEnabled);
    QVERIFY(result.iderEnabled);
    QVERIFY(result.kvmAvailable);
    QVERIFY(result.kvmEnabled);
}

QTEST_GUILESS_MAIN(TestWsmanClient)
#include "test_wsman_client.moc"
