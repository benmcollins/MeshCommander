// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

// Regression tests for the AMT user-consent (OptIn) state machine, #433.
//
// The bug: `SendOptInCode` moves the firmware to RECEIVED(3), and only
// a running redirection session takes it to IN_SESSION(4). Code that
// tested `OptInState != 4` therefore concluded consent was still
// needed the instant it was granted, re-issued `StartOptIn`, revoked
// the consent the operator had just entered, and dropped the KVM
// session they were in the middle of opening. Worse, `StartOptIn` →
// `refreshOptInStatus` → `optInStatusChanged` → `StartOptIn` closed a
// loop with no attempt cap, which hammered the ME until it stopped
// answering anything at all.
//
// These tests drive MachineDetailsController against a scripted HTTP
// endpoint that speaks just enough WS-Management to exercise the
// sequence.

#include "machinedetailscontroller.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using qumesh::app::MachineDetailsController;

namespace {

/// Minimal WS-Management endpoint. Answers `IPS_OptInService` GETs from
/// a caller-controlled `optInState` and records every invoked method so
/// the tests can assert on the call sequence.
class FakeAmt : public QObject
{
    Q_OBJECT
public:
    int optInState = 0;        ///< 0 NotStarted .. 4 InSession
    /// Raw `OptInRequired`: 0 = none, 1 = KVM only, 4294967295 = all.
    quint32 optInRequired = 1;
    quint32 sendCodeReturnValue = 0;
    quint32 startOptInReturnValue = 0;
    QStringList calls;         ///< method names, in order

    bool listen()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeAmt::onConnection);
        return m_server.listen(QHostAddress::LocalHost);
    }
    [[nodiscard]] quint16 port() const { return m_server.serverPort(); }

    [[nodiscard]] int countOf(const QString &method) const
    {
        return static_cast<int>(std::count(calls.cbegin(), calls.cend(), method));
    }

private slots:
    void onConnection()
    {
        while (QTcpSocket *sock = m_server.nextPendingConnection()) {
            connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
                m_buffers[sock].append(sock->readAll());
                serve(sock);
            });
            connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
                m_buffers.remove(sock);
                sock->deleteLater();
            });
        }
    }

private:
    /// Wait for a complete request (headers + Content-Length body),
    /// then answer it.
    void serve(QTcpSocket *sock)
    {
        QByteArray &buf = m_buffers[sock];
        const int headerEnd = buf.indexOf("\r\n\r\n");
        if (headerEnd < 0) return;

        int contentLength = 0;
        const QByteArray headers = buf.left(headerEnd);
        for (const QByteArray &line : headers.split('\n')) {
            const QByteArray l = line.trimmed().toLower();
            if (l.startsWith("content-length:"))
                contentLength = l.mid(15).trimmed().toInt();
        }
        const int bodyStart = headerEnd + 4;
        if (buf.size() < bodyStart + contentLength) return;

        const QByteArray body = buf.mid(bodyStart, contentLength);
        buf.remove(0, bodyStart + contentLength);

        sock->write(respond(body));
        sock->flush();
        // The client sends `Connection: close`; honour it so each
        // request gets its own connection exactly as against real AMT.
        sock->disconnectFromHost();
    }

    QByteArray respond(const QByteArray &body)
    {
        QByteArray payload;
        if (body.contains("StartOptIn")) {
            calls << QStringLiteral("StartOptIn");
            // Real firmware puts a code on the target's screen here.
            if (startOptInReturnValue == 0) optInState = 2; // DISPLAYED
            payload = invokeReply("StartOptIn", startOptInReturnValue);
        } else if (body.contains("SendOptInCode")) {
            calls << QStringLiteral("SendOptInCode");
            if (sendCodeReturnValue == 0) optInState = 3; // RECEIVED
            payload = invokeReply("SendOptInCode", sendCodeReturnValue);
        } else if (body.contains("CancelOptIn")) {
            calls << QStringLiteral("CancelOptIn");
            optInState = 0;
            payload = invokeReply("CancelOptIn", 0);
        } else if (body.contains("IPS_KVMRedirectionSettingData")) {
            calls << QStringLiteral("GetKvmSettings");
            payload = kvmSettingsReply();
        } else {
            calls << QStringLiteral("GetOptInService");
            payload = optInServiceReply();
        }

        QByteArray out = "HTTP/1.1 200 OK\r\n";
        out += "Content-Type: application/soap+xml;charset=UTF-8\r\n";
        out += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
        out += "Connection: close\r\n\r\n";
        out += payload;
        return out;
    }

    static QByteArray envelope(const QByteArray &inner)
    {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
               "<a:Envelope xmlns:a=\"http://www.w3.org/2003/05/soap-envelope\" "
               "xmlns:g=\"http://intel.com/wbem/wscim/1/ips-schema/1/IPS_OptInService\" "
               "xmlns:k=\"http://intel.com/wbem/wscim/1/ips-schema/1/IPS_KVMRedirectionSettingData\">"
               "<a:Header/><a:Body>" + inner + "</a:Body></a:Envelope>";
    }

    static QByteArray invokeReply(const QByteArray &method, quint32 rv)
    {
        return envelope("<g:" + method + "_OUTPUT><g:ReturnValue>"
                        + QByteArray::number(rv)
                        + "</g:ReturnValue></g:" + method + "_OUTPUT>");
    }

    QByteArray optInServiceReply() const
    {
        return envelope("<g:IPS_OptInService>"
                        "<g:OptInRequired>" + QByteArray::number(optInRequired) + "</g:OptInRequired>"
                        "<g:OptInState>" + QByteArray::number(optInState) + "</g:OptInState>"
                        "<g:CanModifyOptInPolicy>1</g:CanModifyOptInPolicy>"
                        "</g:IPS_OptInService>");
    }

    static QByteArray kvmSettingsReply()
    {
        return envelope("<k:IPS_KVMRedirectionSettingData>"
                        "<k:OptInPolicy>true</k:OptInPolicy>"
                        "<k:OptInPolicyTimeout>60</k:OptInPolicyTimeout>"
                        "<k:Is5900PortEnabled>false</k:Is5900PortEnabled>"
                        "<k:SessionTimeout>0</k:SessionTimeout>"
                        "</k:IPS_KVMRedirectionSettingData>");
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

/// Pump the event loop for `ms` with no success condition — used to
/// give queued status reads and timer ticks a chance to fire so a test
/// can assert that something did *not* happen.
void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

bool waitFor(int ms, const std::function<bool()> &pred)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        if (pred()) return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return pred();
}

/// Point a controller at the fake endpoint.
void aim(MachineDetailsController &c, const FakeAmt &amt)
{
    c.setHost(QStringLiteral("127.0.0.1"));
    c.setUser(QStringLiteral("admin"));
    c.setPassword(QStringLiteral("p"));
    c.setTls(false);
    c.setPortForTest(amt.port());
}

} // namespace

class TestOptInStateMachine : public QObject
{
    Q_OBJECT
private slots:
    void grantedConsentIsNotReRequested();
    void receivedStateCountsAsSatisfied();
    void startOptInIsNotReentrantWhileRoundIsLive();
    void repeatedStartFailuresGiveUpInsteadOfLooping();
    void rejectedCodeKeepsRoundOpen();
    void unreadableStatusResolvesToNotRequired();
    // #437 — OptInRequired is a tri-state, not a bool.
    void kvmOnlyPolicyDoesNotGateSolOrIder();
    void alwaysRequiredPolicyGatesEveryProtocol();
    void noPolicyGatesNothing();
};

// The core #433 regression: once the operator's code is accepted the
// firmware sits at RECEIVED(3). Nothing may ask for consent again —
// doing so revokes it and kills the session being opened.
void TestOptInStateMachine::grantedConsentIsNotReRequested()
{
    FakeAmt amt;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);

    QSignalSpy codeSpy(&c, &MachineDetailsController::optInCodeResult);

    c.startOptIn();
    QVERIFY(waitFor(5000, [&]() { return amt.countOf("StartOptIn") == 1; }));

    c.sendOptInCode(123456);
    QVERIFY(waitFor(5000, [&]() { return codeSpy.count() == 1; }));
    QVERIFY(codeSpy.first().at(0).toBool());

    // Let every queued status read and poll tick settle so a stray
    // StartOptIn would have landed by now.
    settle(2000);

    QCOMPARE(amt.optInState, 3);
    QCOMPARE(c.optInState(), 3);
    QVERIFY2(c.optInSatisfied(),
              "RECEIVED(3) must count as consent granted");
    QCOMPARE(amt.countOf("SendOptInCode"), 1);
    QVERIFY2(amt.countOf("StartOptIn") == 1,
              qPrintable(QStringLiteral("StartOptIn re-issued after a granted "
                                        "consent — call sequence was: %1")
                             .arg(amt.calls.join(QStringLiteral(", ")))));
}

void TestOptInStateMachine::receivedStateCountsAsSatisfied()
{
    FakeAmt amt;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);

    // Consent required, nothing granted yet.
    amt.optInRequired = 1;
    amt.optInState = 0;
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInStatusKnown(); }));
    QVERIFY(!c.optInSatisfied());

    // RECEIVED and IN_SESSION are both "go".
    amt.optInState = 3;
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInState() == 3; }));
    QVERIFY(c.optInSatisfied());

    amt.optInState = 4;
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInState() == 4; }));
    QVERIFY(c.optInSatisfied());

    // A machine with no consent policy is always satisfied.
    amt.optInRequired = 0;
    amt.optInState = 0;
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return !c.optInRequired(); }));
    QVERIFY(c.optInSatisfied());
}

void TestOptInStateMachine::startOptInIsNotReentrantWhileRoundIsLive()
{
    FakeAmt amt;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);

    c.startOptIn();
    // Every extra call while the round is outstanding must be dropped;
    // each one would otherwise put a fresh code on the target's screen.
    for (int i = 0; i < 5; ++i) c.startOptIn();

    QVERIFY(waitFor(5000, [&]() { return amt.countOf("StartOptIn") >= 1; }));
    settle(1500);
    QCOMPARE(amt.countOf("StartOptIn"), 1);
    QVERIFY(c.optInRoundActive());

    // Cancelling releases the latch so a later attempt is allowed.
    c.cancelOptIn();
    QVERIFY(waitFor(5000, [&]() { return amt.countOf("CancelOptIn") == 1; }));
    QVERIFY(!c.optInRoundActive());
}

void TestOptInStateMachine::repeatedStartFailuresGiveUpInsteadOfLooping()
{
    FakeAmt amt;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);

    // Firmware refuses every StartOptIn. Without a cap, each failure
    // fed the next attempt and the ME was hammered until it stopped
    // answering anything — the "can't connect at all any more" half of
    // #433.
    amt.startOptInReturnValue = 1;

    QSignalSpy gaveUpSpy(&c, &MachineDetailsController::optInGaveUp);
    QSignalSpy startedSpy(&c, &MachineDetailsController::optInStarted);

    // Drive far more attempts than the cap allows.
    for (int i = 0; i < 12; ++i) {
        c.startOptIn();
        settle(250);
    }
    settle(1000);

    QVERIFY2(gaveUpSpy.count() >= 1, "expected optInGaveUp after the cap");
    QVERIFY2(amt.countOf("StartOptIn") <= 3,
              qPrintable(QStringLiteral("StartOptIn attempted %1 times; the cap "
                                        "is 3. Sequence: %2")
                             .arg(amt.countOf("StartOptIn"))
                             .arg(amt.calls.join(QStringLiteral(", ")))));
    QVERIFY(!c.optInRoundActive());

    // Every attempt reported failure to the UI rather than silently retrying.
    for (int i = 0; i < startedSpy.count(); ++i)
        QVERIFY(!startedSpy.at(i).at(0).toBool());

    // An explicit reset lets the operator try again.
    c.resetOptInAttempts();
    amt.startOptInReturnValue = 0;
    c.startOptIn();
    QVERIFY(waitFor(5000, [&]() { return amt.countOf("StartOptIn") == 4; }));
}

void TestOptInStateMachine::rejectedCodeKeepsRoundOpen()
{
    FakeAmt amt;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);

    amt.sendCodeReturnValue = 1; // wrong code

    QSignalSpy rejectedSpy(&c, &MachineDetailsController::optInCodeRejected);
    QSignalSpy resultSpy(&c, &MachineDetailsController::optInCodeResult);

    c.startOptIn();
    QVERIFY(waitFor(5000, [&]() { return amt.countOf("StartOptIn") == 1; }));

    c.sendOptInCode(111111);
    QVERIFY(waitFor(5000, [&]() { return resultSpy.count() == 1; }));
    QVERIFY(!resultSpy.first().at(0).toBool());
    QCOMPARE(rejectedSpy.count(), 1);

    // A wrong code must not restart the consent round — the code on the
    // target's screen is still the right one to type.
    settle(1500);
    QCOMPARE(amt.countOf("StartOptIn"), 1);
    QVERIFY(c.optInRoundActive());
    QVERIFY(!c.optInSatisfied());
}


// Firmware without IPS_OptInService (AMT <= 5), or a transport hiccup on
// the first read, must not leave callers waiting forever for a policy
// answer that never arrives. Gating Connect on `optInStatusKnown` made
// that a silent hang with no error and no spinner; the reference
// connects anyway on a failed read (Commander.htm:50753).
void TestOptInStateMachine::unreadableStatusResolvesToNotRequired()
{
    // A server that accepts the connection and closes without replying
    // stands in for "this class does not exist here".
    QTcpServer dead;
    QVERIFY(dead.listen(QHostAddress::LocalHost));
    QObject::connect(&dead, &QTcpServer::newConnection, &dead, [&]() {
        while (QTcpSocket *s = dead.nextPendingConnection()) {
            s->close();
            s->deleteLater();
        }
    });

    MachineDetailsController c;
    c.setHost(QStringLiteral("127.0.0.1"));
    c.setUser(QStringLiteral("admin"));
    c.setPassword(QStringLiteral("p"));
    c.setTls(false);
    c.setPortForTest(dead.serverPort());

    QSignalSpy unavailableSpy(&c, &MachineDetailsController::optInStatusUnavailable);

    QVERIFY(!c.optInStatusKnown());
    c.refreshOptInStatus();

    // The status must resolve one way or another — never stay unknown.
    QVERIFY2(waitFor(15000, [&]() { return c.optInStatusKnown(); }),
              "first IPS_OptInService read failed and optInStatusKnown never "
              "became true — Connect would hang silently forever");
    QCOMPARE(unavailableSpy.count(), 1);
    QVERIFY2(c.optInSatisfied(),
              "an unreadable consent policy must not block redirection");
}


// --- #437: OptInRequired granularity ---------------------------------
//
// AMT's OptInRequired is a tri-state (0 / 1 = KVM only / 0xFFFFFFFF =
// all). Flattening it to a bool made a KVM-only machine demand a
// consent PIN for Serial Console sessions that need none — a
// regression the #433 consent gating made reachable, since SOL and
// IDE-R now route through the same gate.

void TestOptInStateMachine::kvmOnlyPolicyDoesNotGateSolOrIder()
{
    FakeAmt amt;
    amt.optInRequired = 1;      // KVM only
    amt.optInState = 0;         // nothing granted
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInStatusKnown(); }));

    QVERIFY2(c.consentRequiredFor(MachineDetailsController::Kvm),
              "KVM must still be gated under a KVM-only policy");
    QVERIFY2(!c.consentRequiredFor(MachineDetailsController::Sol),
              "Serial Console must NOT be gated under a KVM-only policy");
    QVERIFY2(!c.consentRequiredFor(MachineDetailsController::Ider),
              "IDE-R must NOT be gated under a KVM-only policy");

    // ...and therefore SOL/IDE-R may connect immediately while KVM waits.
    QVERIFY(!c.consentSatisfiedFor(MachineDetailsController::Kvm));
    QVERIFY(c.consentSatisfiedFor(MachineDetailsController::Sol));
    QVERIFY(c.consentSatisfiedFor(MachineDetailsController::Ider));
}

void TestOptInStateMachine::alwaysRequiredPolicyGatesEveryProtocol()
{
    FakeAmt amt;
    amt.optInRequired = 4294967295u;   // 0xFFFFFFFF — always required
    amt.optInState = 0;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInStatusKnown(); }));

    for (const int p : {int(MachineDetailsController::Sol),
                        int(MachineDetailsController::Kvm),
                        int(MachineDetailsController::Ider)}) {
        QVERIFY2(c.consentRequiredFor(p),
                  qPrintable(QStringLiteral("protocol %1 must be gated when "
                                            "consent is always required").arg(p)));
        QVERIFY(!c.consentSatisfiedFor(p));
    }

    // A granted code satisfies all three at once.
    amt.optInState = 3;   // RECEIVED
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInState() == 3; }));
    for (const int p : {int(MachineDetailsController::Sol),
                        int(MachineDetailsController::Kvm),
                        int(MachineDetailsController::Ider)}) {
        QVERIFY(c.consentSatisfiedFor(p));
    }
}

void TestOptInStateMachine::noPolicyGatesNothing()
{
    FakeAmt amt;
    amt.optInRequired = 0;
    amt.optInState = 0;
    QVERIFY(amt.listen());

    MachineDetailsController c;
    aim(c, amt);
    c.refreshOptInStatus();
    QVERIFY(waitFor(5000, [&]() { return c.optInStatusKnown(); }));

    for (const int p : {int(MachineDetailsController::Sol),
                        int(MachineDetailsController::Kvm),
                        int(MachineDetailsController::Ider)}) {
        QVERIFY(!c.consentRequiredFor(p));
        QVERIFY(c.consentSatisfiedFor(p));
    }
}

QTEST_MAIN(TestOptInStateMachine)
#include "test_optin_statemachine.moc"
