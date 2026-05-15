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

QTEST_GUILESS_MAIN(TestWsmanClient)
#include "test_wsman_client.moc"
