// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "wsman/local_amt_probe.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using namespace qumesh::wsman;

class TestLocalAmtProbe : public QObject
{
    Q_OBJECT
private slots:
    void availableWhenSomeoneListensOn16992();
    void unavailableWhenNobodyListens();
    void tlsFlagSetWhen16993Also();
};

namespace {

/// Listen on a fixed loopback port until destroyed. Helper for the
/// availability tests. Returns nullptr if the bind fails — e.g.
/// because the CI runner already has something on the port.
QTcpServer *startListener(quint16 port)
{
    auto *s = new QTcpServer;
    if (!s->listen(QHostAddress::LocalHost, port)) {
        delete s;
        return nullptr;
    }
    return s;
}

bool waitForFinished(LocalAmtProbe &p, int ms = 3000)
{
    QSignalSpy spy(&p, &LocalAmtProbe::finished);
    if (spy.count() > 0) return true;
    return spy.wait(ms);
}

} // namespace

void TestLocalAmtProbe::availableWhenSomeoneListensOn16992()
{
    auto *server = startListener(16992);
    if (!server) {
        QSKIP("Couldn't bind 127.0.0.1:16992 — skipping (port in use).");
    }
    LocalAmtProbe probe;
    probe.start();
    QVERIFY(waitForFinished(probe));
    QVERIFY(probe.available());
    delete server;
}

void TestLocalAmtProbe::unavailableWhenNobodyListens()
{
    // Sanity: with nothing on either AMT port, the probe rejects.
    // If the host machine has actual LMS running, this test is
    // meaningless — skip rather than fail.
    QTcpSocket sniff;
    sniff.connectToHost(QStringLiteral("127.0.0.1"), 16992);
    if (sniff.waitForConnected(200)) {
        QSKIP("Real AMT loopback is up on 16992 — skipping.");
    }

    LocalAmtProbe probe;
    probe.start();
    QVERIFY(waitForFinished(probe, 5000));
    QVERIFY(!probe.available());
    QVERIFY(!probe.tlsAvailable());
}

void TestLocalAmtProbe::tlsFlagSetWhen16993Also()
{
    auto *plain = startListener(16992);
    auto *tls   = startListener(16993);
    if (!plain || !tls) {
        delete plain;
        delete tls;
        QSKIP("Couldn't bind both 16992 and 16993 — skipping.");
    }
    LocalAmtProbe probe;
    probe.start();
    QVERIFY(waitForFinished(probe));
    QVERIFY(probe.available());
    QVERIFY(probe.tlsAvailable());
    delete plain;
    delete tls;
}

QTEST_MAIN(TestLocalAmtProbe)
#include "test_local_amt_probe.moc"
