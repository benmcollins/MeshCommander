// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "ssh/ssh_session.h"

#include <QObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QtTest>

using namespace qumesh::ssh;

class TestSshSession : public QObject
{
    Q_OBJECT
private slots:
    void constructDestructDoesNotCrash();
    void openWithUnreachableHostFails();
    void openWithNonSshPortFails();
    void closeIsIdempotent();
    void aboutToDestroyFiresBeforeDestroyed();
    void destructorReturnsPromptlyMidConnect();
};

void TestSshSession::constructDestructDoesNotCrash()
{
    {
        SshSession s;
        QCOMPARE(s.state(), SshSession::Disconnected);
    }
    // If the worker thread didn't shut down cleanly, ASAN would
    // complain on the test runner; nothing else to assert here.
}

void TestSshSession::openWithUnreachableHostFails()
{
    SshSession s;
    QSignalSpy stateSpy(&s, &SshSession::stateChanged);

    SshSession::Params p;
    p.host = QStringLiteral("127.0.0.1");
    // RFC 6335 reserves 0; libssh's default fallback will error out.
    p.port = 1;
    p.user = QStringLiteral("nobody");
    p.password = QStringLiteral("nope");
    p.connectTimeoutMs = 2000;
    s.open(p);

    // Wait for any terminal state.
    QVERIFY(stateSpy.wait(8000));
    while (stateSpy.last().value(0).toInt() != SshSession::Failed
           && stateSpy.last().value(0).toInt() != SshSession::Disconnected) {
        if (!stateSpy.wait(8000)) break;
    }
    QCOMPARE(s.state(), SshSession::Failed);
    QVERIFY(!s.lastError().isEmpty());
}

void TestSshSession::openWithNonSshPortFails()
{
    // Listen on an ephemeral port but never answer SSH banner: libssh
    // should bail with a banner-read error.
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const quint16 port = server.serverPort();

    SshSession s;
    QSignalSpy stateSpy(&s, &SshSession::stateChanged);

    SshSession::Params p;
    p.host = QStringLiteral("127.0.0.1");
    p.port = port;
    p.user = QStringLiteral("nobody");
    p.password = QStringLiteral("nope");
    p.connectTimeoutMs = 2000;
    s.open(p);

    QVERIFY(stateSpy.wait(8000));
    while (stateSpy.last().value(0).toInt() != SshSession::Failed
           && stateSpy.last().value(0).toInt() != SshSession::Disconnected) {
        if (!stateSpy.wait(8000)) break;
    }
    QCOMPARE(s.state(), SshSession::Failed);
}

void TestSshSession::closeIsIdempotent()
{
    SshSession s;
    s.close();
    s.close();
    QCOMPARE(s.state(), SshSession::Disconnected);
}

void TestSshSession::destructorReturnsPromptlyMidConnect()
{
    // #233 regression guard. Closing a machine window (or otherwise
    // destroying an SshSession) while libssh is mid-`ssh_connect` /
    // `ssh_userauth_*` used to freeze the caller's thread for up to
    // `connectTimeoutMs` (≤15s default), because the destructor
    // synchronously joined the worker with a BlockingQueuedConnection
    // that couldn't proceed until libssh's blocking call returned.
    //
    // Fix moved libssh to non-blocking + a poll loop that checks a
    // cancellation atomic every ~50ms. This test pins that contract:
    // open against a TCP server that accepts but never speaks SSH,
    // give the worker a beat to enter `ssh_connect`, then destroy
    // and assert the destructor returns well inside the would-be
    // freeze window.
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const quint16 port = server.serverPort();

    SshSession::Params p;
    p.host = QStringLiteral("127.0.0.1");
    p.port = port;
    p.user = QStringLiteral("nobody");
    p.password = QStringLiteral("nope");
    // Pick a timeout longer than the latency budget we're asserting,
    // so a regressed destructor would actually demonstrate the freeze
    // rather than being masked by libssh's own internal abort.
    p.connectTimeoutMs = 15000;

    auto s = std::make_unique<SshSession>();
    QSignalSpy stateSpy(s.get(), &SshSession::stateChanged);
    s->open(p);
    // Wait for the worker to reach Connecting so we know it's actually
    // in flight when we destroy it.
    QVERIFY(stateSpy.wait(2000));
    QCOMPARE(stateSpy.last().value(0).toInt(), int(SshSession::Connecting));

    QElapsedTimer t;
    t.start();
    s.reset(); // ~SshSession
    const qint64 elapsed = t.elapsed();
    // 500 ms is comfortably above the 50 ms poll tick + Qt event-loop
    // round-trip, and comfortably below the 15 000 ms freeze window
    // we were seeing before #233's fix. Anything in between would
    // indicate the destructor is still waiting on libssh.
    QVERIFY2(elapsed < 500,
             qPrintable(QStringLiteral(
                 "~SshSession took %1 ms — expected <500 ms").arg(elapsed)));
}

void TestSshSession::aboutToDestroyFiresBeforeDestroyed()
{
    // SshTunnel relies on aboutToDestroy() running before the worker
    // thread is quit so its pump can join cleanly. Verify the contract:
    // exactly one aboutToDestroy() arrives, and it arrives before the
    // standard destroyed() signal (which fires from ~QObject after our
    // body runs).
    auto *s = new SshSession();
    QSignalSpy aboutSpy(s, &SshSession::aboutToDestroy);
    QSignalSpy destroyedSpy(s, &QObject::destroyed);
    delete s;
    QCOMPARE(aboutSpy.size(), 1);
    QCOMPARE(destroyedSpy.size(), 1);
}

QTEST_GUILESS_MAIN(TestSshSession)
#include "test_ssh_session.moc"
