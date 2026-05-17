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
    void getAuditLogStateDecodesBitmask();
    void enumerateAuditLogParsesBinaryRecords();
    void enumerateUserAccountsMergesAdminAndAclEntries();

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

constexpr char kAuditLogStateResponse[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
    " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuditLog\">"
    "<s:Header/><s:Body>"
    "<a:AMT_AuditLog>"
    // 0x05 = bit 0 (enabled) + bit 2 (almost full)
    "<a:AuditState>5</a:AuditState>"
    "<a:OverwritePolicy>1</a:OverwritePolicy>"
    "<a:CurrentNumberOfRecords>42</a:CurrentNumberOfRecords>"
    "<a:PercentageFree>15</a:PercentageFree>"
    "<a:MaxAllowedAuditors>4</a:MaxAllowedAuditors>"
    "<a:EnabledState>2</a:EnabledState>"
    "</a:AMT_AuditLog>"
    "</s:Body></s:Envelope>";

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

void TestWsmanClient::getAuditLogStateDecodesBitmask()
{
    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &) {
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                QByteArray(kAuditLogStateResponse));
                 });
    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    AuditLogState s;
    QEventLoop loop;
    getAuditLogState(&client, [&](AuditLogState r) { s = r; loop.quit(); });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(s.ok, qPrintable(s.error));
    QCOMPARE(s.auditState, 5);
    QCOMPARE(s.currentNumberOfRecords, 42);
    QCOMPARE(s.percentageFree, 15);
}

void TestWsmanClient::enumerateAuditLogParsesBinaryRecords()
{
    // Hand-build two records into one chunk and verify the parser
    // walks the binary fields correctly. Record one is a Local-
    // initiator event with an event ID that lives in the audit string
    // table; record two is an HTTP-digest event with a user name.
    auto makeBE16 = [](quint16 v) {
        QByteArray r; r.resize(2);
        r[0] = char(v >> 8); r[1] = char(v & 0xFF);
        return r;
    };
    auto makeBE32 = [](quint32 v) {
        QByteArray r; r.resize(4);
        r[0] = char(v >> 24); r[1] = char((v >> 16) & 0xFF);
        r[2] = char((v >>  8) & 0xFF); r[3] = char(v & 0xFF);
        return r;
    };

    const quint16 auditApp = 20;   // Security Audit Log
    const quint16 eventOk  = 3;    // 2003 → "Security Audit Log Enabled"
    const quint32 t        = 1700000000u;

    QByteArray rec1;
    rec1 += makeBE16(auditApp);
    rec1 += makeBE16(eventOk);
    rec1.append(char(2));          // InitiatorType: Local
    rec1 += makeBE32(t);
    rec1.append(char(0));          // MCLocationType
    rec1.append(char(9));          // netlen
    rec1.append("192.0.2.1", 9);
    rec1.append(char(0));          // exlen

    QByteArray rec2;
    rec2 += makeBE16(17);          // RCO
    rec2 += makeBE16(0);           // → 1700 "Performed Power Up"
    rec2.append(char(0));          // HTTP digest
    rec2.append(char(5));          // userlen
    rec2.append("admin", 5);
    rec2 += makeBE32(t);
    rec2.append(char(0));          // MCLocationType
    rec2.append(char(0));          // netlen
    rec2.append(char(0));          // exlen

    const QString b64a = QString::fromLatin1(rec1.toBase64());
    const QString b64b = QString::fromLatin1(rec2.toBase64());

    const QString response = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:g=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuditLog\">"
        "<s:Header/><s:Body>"
        "<g:ReadRecords_OUTPUT>"
        "<g:TotalRecordCount>2</g:TotalRecordCount>"
        "<g:RecordsReturned>2</g:RecordsReturned>"
        "<g:EventRecords>%1</g:EventRecords>"
        "<g:EventRecords>%2</g:EventRecords>"
        "<g:ReturnValue>0</g:ReturnValue>"
        "</g:ReadRecords_OUTPUT>"
        "</s:Body></s:Envelope>").arg(b64a, b64b);
    const QByteArray responseBytes = response.toUtf8();

    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &) {
                     return QHttpServerResponse(QByteArrayLiteral("application/soap+xml"),
                                                responseBytes);
                 });
    auto tcp = std::make_unique<QTcpServer>();
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    const quint16 port = tcp->serverPort();
    QVERIFY(server.bind(tcp.get()));
    tcp.release();

    WsmanClient client;
    client.setEndpoint(endpointFor(port));

    AuditLogResult res;
    QEventLoop loop;
    enumerateAuditLog(&client, [&](AuditLogResult r) { res = r; loop.quit(); });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(res.ok, qPrintable(res.error));
    QCOMPARE(res.entries.size(), 2);

    QCOMPARE(res.entries[0].auditAppId, 20);
    QCOMPARE(res.entries[0].eventId,    3);
    QCOMPARE(res.entries[0].auditAppLabel, QStringLiteral("Security Audit Log"));
    QCOMPARE(res.entries[0].eventLabel,    QStringLiteral("Security Audit Log Enabled"));
    QCOMPARE(res.entries[0].initiator,     QStringLiteral("Local"));
    QCOMPARE(res.entries[0].unixSeconds,   1700000000LL);
    QCOMPARE(res.entries[0].netAddress,    QStringLiteral("192.0.2.1"));

    QCOMPARE(res.entries[1].auditAppId, 17);
    QCOMPARE(res.entries[1].eventLabel, QStringLiteral("Performed Power Up"));
    QCOMPARE(res.entries[1].initiator,  QStringLiteral("admin"));
}

void TestWsmanClient::enumerateUserAccountsMergesAdminAndAclEntries()
{
    // Three invokes hit the mock: GetAdminAclEntry, EnumerateUserAclEntries,
    // and per-handle GetUserAclEntryEx + GetAclEnabledState. The mock
    // routes on the method name embedded in the body.
    static const char *adminResponse =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService\">"
        "<s:Header/><s:Body>"
        "<a:GetAdminAclEntry_OUTPUT>"
        "<a:Username>admin</a:Username>"
        "<a:ReturnValue>0</a:ReturnValue>"
        "</a:GetAdminAclEntry_OUTPUT>"
        "</s:Body></s:Envelope>";

    static const char *enumResponse =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService\">"
        "<s:Header/><s:Body>"
        "<a:EnumerateUserAclEntries_OUTPUT>"
        "<a:Handles>1</a:Handles>"
        "<a:Handles>2</a:Handles>"
        "<a:ReturnValue>0</a:ReturnValue>"
        "</a:EnumerateUserAclEntries_OUTPUT>"
        "</s:Body></s:Envelope>";

    static const char *userEntryDigest =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService\">"
        "<s:Header/><s:Body>"
        "<a:GetUserAclEntryEx_OUTPUT>"
        "<a:DigestUsername>operator</a:DigestUsername>"
        "<a:AccessPermission>2</a:AccessPermission>"
        "<a:Realms>2</a:Realms>"   // Redirection
        "<a:Realms>5</a:Realms>"   // Remote Control
        "<a:Realms>13</a:Realms>"  // General Information
        "<a:ReturnValue>0</a:ReturnValue>"
        "</a:GetUserAclEntryEx_OUTPUT>"
        "</s:Body></s:Envelope>";

    static const char *enabledTrue =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService\">"
        "<s:Header/><s:Body>"
        "<a:GetAclEnabledState_OUTPUT>"
        "<a:Enabled>true</a:Enabled>"
        "<a:ReturnValue>0</a:ReturnValue>"
        "</a:GetAclEnabledState_OUTPUT>"
        "</s:Body></s:Envelope>";

    // Distinguish the two handles by tracking which Handle value appears
    // in the request body. Handle=1 returns a digest user (enabled);
    // Handle=2 returns a Kerberos user (disabled) for variety.
    static const char *userEntryKerberos =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService\">"
        "<s:Header/><s:Body>"
        "<a:GetUserAclEntryEx_OUTPUT>"
        "<a:KerberosUserSid>AQUAAAAAAAUVAAAAAAAAAAAAAAA=</a:KerberosUserSid>"
        "<a:AccessPermission>1</a:AccessPermission>"
        "<a:Realms>3</a:Realms>"   // bit 3 = administrator
        "<a:ReturnValue>0</a:ReturnValue>"
        "</a:GetUserAclEntryEx_OUTPUT>"
        "</s:Body></s:Envelope>";

    static const char *enabledFalse =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://intel.com/wbem/wscim/1/amt-schema/1/AMT_AuthorizationService\">"
        "<s:Header/><s:Body>"
        "<a:GetAclEnabledState_OUTPUT>"
        "<a:Enabled>false</a:Enabled>"
        "<a:ReturnValue>0</a:ReturnValue>"
        "</a:GetAclEnabledState_OUTPUT>"
        "</s:Body></s:Envelope>";

    QHttpServer server;
    server.route(QStringLiteral("/wsman"), QHttpServerRequest::Method::Post,
                 [&](const QHttpServerRequest &req) {
                     const QByteArray body = req.body();
                     QByteArray response;
                     if (body.contains("GetAdminAclEntry")) {
                         response = adminResponse;
                     } else if (body.contains("EnumerateUserAclEntries")) {
                         response = enumResponse;
                     } else if (body.contains("GetUserAclEntryEx")) {
                         response = body.contains("<g:Handle>2</g:Handle>")
                              || body.contains("Handle>2</")
                                       ? QByteArray(userEntryKerberos)
                                       : QByteArray(userEntryDigest);
                     } else if (body.contains("GetAclEnabledState")) {
                         response = body.contains("<g:Handle>2</g:Handle>")
                              || body.contains("Handle>2</")
                                       ? QByteArray(enabledFalse)
                                       : QByteArray(enabledTrue);
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

    UserAccountsResult res;
    QEventLoop loop;
    enumerateUserAccounts(&client, [&](UserAccountsResult r) {
        res = r;
        loop.quit();
    });
    QTimer::singleShot(8000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY2(res.ok, qPrintable(res.error));

    // Three rows expected: admin (handle=-1), handle=1, handle=2.
    QCOMPARE(res.accounts.size(), 3);
    QCOMPARE(res.accounts[0].handle, -1);
    QCOMPARE(res.accounts[0].digestUsername, QStringLiteral("admin"));
    QCOMPARE(res.accounts[0].accessPermission, 999);

    // Handle=1 should have the digest user with three realms.
    const auto &u1 = res.accounts[1];
    QCOMPARE(u1.handle, 1);
    QCOMPARE(u1.digestUsername, QStringLiteral("operator"));
    QCOMPARE(u1.accessPermission, 2);
    QCOMPARE(u1.realms.size(), 3);
    QVERIFY(u1.realms.contains(2));
    QVERIFY(u1.realms.contains(5));
    QVERIFY(u1.enabled);

    // Handle=2 should be Kerberos, realm 3 (administrator bit), disabled.
    const auto &u2 = res.accounts[2];
    QCOMPARE(u2.handle, 2);
    QVERIFY(u2.digestUsername.isEmpty());
    QVERIFY(!u2.kerberosUserSidB64.isEmpty());
    QCOMPARE(u2.accessPermission, 1);
    QVERIFY(u2.realms.contains(3));
    QVERIFY(!u2.enabled);

    // Label helpers.
    QCOMPARE(accessPermissionLabel(0),   QStringLiteral("Local only"));
    QCOMPARE(accessPermissionLabel(2),   QStringLiteral("All (Local & Network)"));
    QCOMPARE(accessPermissionLabel(999), QStringLiteral("Administrator"));
    QCOMPARE(realmName(2),  QStringLiteral("Redirection"));
    QCOMPARE(realmName(20), QStringLiteral("Audit Log"));
    QCOMPARE(realmName(3),  QString()); // sentinel
}

QTEST_GUILESS_MAIN(TestWsmanClient)
#include "test_wsman_client.moc"
