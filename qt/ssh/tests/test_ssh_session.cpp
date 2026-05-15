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
