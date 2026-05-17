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
    void getHardwareInventoryStitchesAllSections();

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

// --- Hardware-inventory fixtures ----------------------------------

constexpr char kChassisPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Chassis\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_Chassis>"
    "<c:Model>ProBook 450 G7</c:Model>"
    "<c:Manufacturer>HP</c:Manufacturer>"
    "<c:Version>KBC Version 12.34</c:Version>"
    "<c:SerialNumber>5CD012ABCD</c:SerialNumber>"
    "</c:CIM_Chassis>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kSysPackagingPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_SystemPackaging\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_SystemPackaging>"
    // Bytes (no dashes) on the wire; first three dwords are
    // little-endian. The expected guidToStr output for this is
    // `33221100-5544-7766-8899-aabbccddeeff`.
    "<c:PlatformGUID>00112233445566778899AABBCCDDEEFF</c:PlatformGUID>"
    "</c:CIM_SystemPackaging>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kBiosPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_BIOSElement\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_BIOSElement>"
    "<c:Manufacturer>HP</c:Manufacturer>"
    "<c:SoftwareElementID>S70 Ver. 01.07.00</c:SoftwareElementID>"
    "<c:ReleaseDate><c:Datetime>20200115000000.000000+000</c:Datetime></c:ReleaseDate>"
    "</c:CIM_BIOSElement>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kProcessorPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Processor\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_Processor>"
    "<c:Family>198</c:Family>"
    "<c:MaxClockSpeed>3600</c:MaxClockSpeed>"
    "<c:CPUStatus>1</c:CPUStatus>"
    "</c:CIM_Processor>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kChipPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Chip\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_Chip>"
    "<c:Manufacturer>Intel</c:Manufacturer>"
    "<c:Version>Intel(R) Core(TM) i7-1065G7</c:Version>"
    "</c:CIM_Chip>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kPhysMemoryPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_PhysicalMemory\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_PhysicalMemory>"
    "<c:BankLabel>ChannelA-DIMM0</c:BankLabel>"
    "<c:Manufacturer>Samsung</c:Manufacturer>"
    "<c:SerialNumber>0xCAFEF00D</c:SerialNumber>"
    // 16 GiB
    "<c:Capacity>17179869184</c:Capacity>"
    "<c:FormFactor>13</c:FormFactor>"
    "<c:MemoryType>26</c:MemoryType>"
    "<c:Tag>9876</c:Tag>"
    "<c:PartNumber>M471A2K43DB1-CWE</c:PartNumber>"
    "</c:CIM_PhysicalMemory>"
    "<c:CIM_PhysicalMemory>"
    "<c:BankLabel>ChannelB-DIMM0</c:BankLabel>"
    "<c:Manufacturer>Samsung</c:Manufacturer>"
    "<c:SerialNumber>0xCAFEBABE</c:SerialNumber>"
    "<c:Capacity>17179869184</c:Capacity>"
    "<c:FormFactor>13</c:FormFactor>"
    "<c:MemoryType>26</c:MemoryType>"
    "<c:Tag>9877</c:Tag>"
    "<c:PartNumber>M471A2K43DB1-CWE</c:PartNumber>"
    "</c:CIM_PhysicalMemory>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kMediaPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_MediaAccessDevice\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_MediaAccessDevice>"
    "<c:MaxMediaSize>512110190</c:MaxMediaSize>"
    "</c:CIM_MediaAccessDevice>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kPhysPackagePullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_PhysicalPackage\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_PhysicalPackage>" // 0 — chassis
    "<c:PackageType>3</c:PackageType>"
    "<c:Manufacturer>HP</c:Manufacturer>"
    "</c:CIM_PhysicalPackage>"
    "<c:CIM_PhysicalPackage>" // 1 — storage (matches Media[0] per legacy +1 shift)
    "<c:PackageType>13</c:PackageType>"
    "<c:Model>Samsung SSD 970 EVO Plus 500GB</c:Model>"
    "<c:SerialNumber>S5GXNS0NA00001</c:SerialNumber>"
    "</c:CIM_PhysicalPackage>"
    "<c:CIM_PhysicalPackage>" // 2 — battery (PackageType=11)
    "<c:PackageType>11</c:PackageType>"
    "<c:Manufacturer>HP</c:Manufacturer>"
    "<c:SerialNumber>BAT-001</c:SerialNumber>"
    "<c:ManufactureDate><c:Datetime>20200110000000.000000+000</c:Datetime></c:ManufactureDate>"
    "</c:CIM_PhysicalPackage>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kBatteryPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Battery\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_Battery>"
    "<c:DeviceID>Battery 0</c:DeviceID>"
    "<c:Chemistry>5</c:Chemistry>"
    "<c:DesignCapacity>45000</c:DesignCapacity>"
    "<c:DesignVoltage>11400</c:DesignVoltage>"
    "</c:CIM_Battery>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

constexpr char kCardPullResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:wsen=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\""
    " xmlns:c=\"http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_Card\">"
    "<s:Header/><s:Body><wsen:PullResponse><wsen:Items>"
    "<c:CIM_Card>"
    "<c:Manufacturer>HP</c:Manufacturer>"
    "<c:Model>867D</c:Model>"
    "<c:Version>KBC Version 12.34</c:Version>"
    "<c:SerialNumber>PJTQ001SDS3SU</c:SerialNumber>"
    "<c:Tag>AssetXYZ</c:Tag>"
    "<c:CanBeFRUed>true</c:CanBeFRUed>"
    "</c:CIM_Card>"
    "</wsen:Items><wsen:EndOfSequence/></wsen:PullResponse></s:Body></s:Envelope>";

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

void TestWsmanClient::getHardwareInventoryStitchesAllSections()
{
    // Ten classes, one route. Switch on resource URI in the request
    // body. The first POST per class is Enumerate, the second is Pull.
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &req) {
                     const QByteArray body = req.body();
                     // Pull responses are returned only when the body
                     // contains a Pull. Enumerate goes first.
                     const bool isPull = body.contains(":Pull>")
                                      || body.contains("<wsen:Pull");
                     QByteArray response;
                     auto matches = [&](const char *uri) { return body.contains(uri); };
                     if (!isPull) {
                         response = QByteArray(kEnumerateResponse);
                     } else if (matches("CIM_Chassis")) {
                         response = QByteArray(kChassisPullResponse);
                     } else if (matches("CIM_SystemPackaging")) {
                         response = QByteArray(kSysPackagingPullResponse);
                     } else if (matches("CIM_BIOSElement")) {
                         response = QByteArray(kBiosPullResponse);
                     } else if (matches("CIM_Processor")) {
                         response = QByteArray(kProcessorPullResponse);
                     } else if (matches("CIM_Chip")) {
                         response = QByteArray(kChipPullResponse);
                     } else if (matches("CIM_PhysicalMemory")) {
                         response = QByteArray(kPhysMemoryPullResponse);
                     } else if (matches("CIM_MediaAccessDevice")) {
                         response = QByteArray(kMediaPullResponse);
                     } else if (matches("CIM_PhysicalPackage")) {
                         response = QByteArray(kPhysPackagePullResponse);
                     } else if (matches("CIM_Battery")) {
                         response = QByteArray(kBatteryPullResponse);
                     } else if (matches("CIM_Card")) {
                         response = QByteArray(kCardPullResponse);
                     } else {
                         response = QByteArray(kEnumerateResponse);
                     }
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

    HardwareInventoryResult result;
    QEventLoop loop;
    getHardwareInventory(&client, [&](HardwareInventoryResult r) {
        result = r;
        loop.quit();
    });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(result.ok, qPrintable(result.error));

    // Platform / GUID byte-swap.
    QCOMPARE(result.platformModel, QStringLiteral("ProBook 450 G7"));
    QCOMPARE(result.platformManufacturer, QStringLiteral("HP"));
    QCOMPARE(result.platformSystemId,
             QStringLiteral("33221100-5544-7766-8899-aabbccddeeff"));

    // Baseboard
    QCOMPARE(result.baseboardManufacturer, QStringLiteral("HP"));
    QCOMPARE(result.baseboardModel, QStringLiteral("867D"));
    QVERIFY(result.baseboardCanBeFRUedKnown);
    QVERIFY(result.baseboardReplaceable);

    // BIOS — Datetime is nested under ReleaseDate; the parser picks it up.
    QCOMPARE(result.biosVendor, QStringLiteral("HP"));
    QCOMPARE(result.biosVersion, QStringLiteral("S70 Ver. 01.07.00"));
    QVERIFY(!result.biosReleaseDate.isEmpty());

    // Processors zipped with chips.
    QCOMPARE(result.processors.size(), 1);
    QCOMPARE(result.processors.first().family, 198);
    QCOMPARE(result.processors.first().familyLabel,
             QStringLiteral("Intel® Core™ i7 Processor"));
    QCOMPARE(result.processors.first().manufacturer, QStringLiteral("Intel"));
    QCOMPARE(result.processors.first().maxClockSpeedMhz, 3600);
    QCOMPARE(result.processors.first().cpuStatusLabel, QStringLiteral("Enabled"));

    // Two DIMMs — DDR4 (26), SODIMM (13), 16 GiB each.
    QCOMPARE(result.memoryModules.size(), 2);
    QCOMPARE(result.memoryModules.first().memoryTypeLabel, QStringLiteral("DDR4"));
    QCOMPARE(result.memoryModules.first().formFactorLabel, QStringLiteral("SODIMM"));
    QCOMPARE(result.memoryModules.first().capacityBytes, 17179869184LL);

    // One storage device — package index +1 zip.
    QCOMPARE(result.storageDevices.size(), 1);
    QCOMPARE(result.storageDevices.first().model,
             QStringLiteral("Samsung SSD 970 EVO Plus 500GB"));
    QCOMPARE(result.storageDevices.first().serialNumber,
             QStringLiteral("S5GXNS0NA00001"));

    // Battery — present, matched physical package by PackageType=11.
    QVERIFY(result.battery.present);
    QCOMPARE(result.battery.deviceId, QStringLiteral("Battery 0"));
    QCOMPARE(result.battery.chemistryLabel, QStringLiteral("Lithium-ion"));
    QCOMPARE(result.battery.designCapacityMwh, 45000LL);
    QCOMPARE(result.battery.designVoltageMv,   11400LL);
    QCOMPARE(result.battery.serialNumber,      QStringLiteral("BAT-001"));
}

QTEST_GUILESS_MAIN(TestWsmanClient)
#include "test_wsman_client.moc"
