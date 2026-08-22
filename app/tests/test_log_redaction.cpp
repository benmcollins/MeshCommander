// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

// Redaction guard for the diagnostic logging added in #438.
//
// README.md tells users their log "contains hostnames and usernames but
// never passwords" and invites them to paste it into a public GitHub
// issue. That promise is only as good as the code, and the failure mode
// is silent: a `<< m_password` added years from now leaks every time
// anyone follows the documented bug-reporting steps.
//
// These tests capture everything the logging system emits — with all
// categories forced on, exactly as a reporter would run it — and assert
// that known secret values never appear. `QTest::ignoreMessage` cannot
// express this: it matches and consumes one exact message, whereas the
// assertion here is "no line anywhere contains this substring", so a
// custom message handler is the only way.

#include "kvmcontroller.h"
#include "machinedetailscontroller.h"
#include "solcontroller.h"

#include <QLoggingCategory>
#include <QTcpServer>
#include <QtTest>

using qumesh::app::KvmController;
using qumesh::app::MachineDetailsController;
using qumesh::app::SolController;

namespace {

/// Everything the logging system emitted while installed.
QStringList g_captured;
QtMessageHandler g_previous = nullptr;

void capturingHandler(QtMsgType type, const QMessageLogContext &ctx,
                      const QString &msg)
{
    g_captured << msg;
    if (g_previous != nullptr) g_previous(type, ctx, msg);
}

class LogCapture
{
public:
    /// `verbose` mirrors QT_LOGGING_RULES='qumesh.*=true', i.e. what a
    /// reporter following README.md would actually send us. Pass false
    /// to observe what an ordinary user sees with nothing set.
    explicit LogCapture(bool verbose = true)
    {
        g_captured.clear();
        QLoggingCategory::setFilterRules(verbose
                                             ? QStringLiteral("qumesh.*=true")
                                             : QString());
        g_previous = qInstallMessageHandler(capturingHandler);
    }
    ~LogCapture()
    {
        qInstallMessageHandler(g_previous);
        g_previous = nullptr;
        QLoggingCategory::setFilterRules(QString());
    }

    [[nodiscard]] static QString all() { return g_captured.join(QLatin1Char('\n')); }
};

/// Pump the event loop briefly. Nothing here asserts on completion —
/// every line these tests check is emitted synchronously — so this
/// only exists to let any queued follow-on land.
void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

// Deliberately distinctive so a substring match can't false-negative
// against incidental output.
const QString kPassword = QStringLiteral("hunter2-SUPERSECRET-pw");
const int     kConsentPin = 483927;
const QString kRfbPassword = QStringLiteral("rfb-SUPERSECRET-vnc");

} // namespace

class TestLogRedaction : public QObject
{
    Q_OBJECT
private slots:
    void redirectionLoggingNeverCarriesThePassword();
    void consentPinIsNeverLogged();
    void kvmSettingsLogsKeysNotValues();
    void categoriesAreQuietByDefault();
};

void TestLogRedaction::redirectionLoggingNeverCarriesThePassword()
{
    LogCapture cap;

    // Deliberately does NOT depend on the connection failing. The
    // open-time lines are emitted synchronously inside open(), before
    // any socket activity, so this holds regardless of how a given
    // platform treats a connect to a closed port — Windows does not
    // refuse as promptly as Linux/macOS, which made an earlier
    // timing-dependent version of this test flaky there.
    KvmController kvm;
    kvm.setHost(QStringLiteral("127.0.0.1"));
    kvm.setUser(QStringLiteral("admin"));
    kvm.setPassword(kPassword);
    kvm.setPortForTest(1);      // reserved, nothing listens
    kvm.open();

    SolController sol;
    sol.setHost(QStringLiteral("127.0.0.1"));
    sol.setUser(QStringLiteral("admin"));
    sol.setPassword(kPassword);
    sol.setPortForTest(1);
    sol.open();

    // A brief pump so any synchronous follow-on lands too; nothing is
    // asserted about what does or doesn't complete.
    settle(250);
    kvm.close();
    sol.close();

    const QString log = LogCapture::all();
    QVERIFY2(!log.isEmpty(), "captured nothing — the test proves nothing");
    QVERIFY2(!log.contains(kPassword),
              qPrintable(QStringLiteral("the AMT password reached the log:\n%1").arg(log)));
    // The username is explicitly permitted, and asserting its presence
    // stops a capture that silently saw nothing from passing.
    QVERIFY2(log.contains(QStringLiteral("admin")),
              qPrintable(QStringLiteral("expected the username in the log:\n%1").arg(log)));
}

void TestLogRedaction::consentPinIsNeverLogged()
{
    LogCapture cap;

    QTcpServer dead;
    QVERIFY(dead.listen(QHostAddress::LocalHost));
    const quint16 port = dead.serverPort();
    dead.close();

    MachineDetailsController c;
    c.setHost(QStringLiteral("127.0.0.1"));
    c.setUser(QStringLiteral("admin"));
    c.setPassword(kPassword);
    c.setPortForTest(port);

    c.sendOptInCode(kConsentPin);
    settle(250);

    const QString log = LogCapture::all();
    QVERIFY2(!log.isEmpty(), "captured nothing — the test proves nothing");
    // The PIN is a live authorisation secret for the length of the
    // consent round; it must not appear in any form.
    QVERIFY2(!log.contains(QString::number(kConsentPin)),
              qPrintable(QStringLiteral("the consent PIN reached the log:\n%1").arg(log)));
    QVERIFY2(!log.contains(kPassword),
              qPrintable(QStringLiteral("the AMT password reached the log:\n%1").arg(log)));
    QVERIFY2(log.contains(QStringLiteral("redacted")),
              qPrintable(QStringLiteral("expected the SendOptInCode line:\n%1").arg(log)));
}

void TestLogRedaction::kvmSettingsLogsKeysNotValues()
{
    LogCapture cap;

    QTcpServer dead;
    QVERIFY(dead.listen(QHostAddress::LocalHost));
    const quint16 port = dead.serverPort();
    dead.close();

    MachineDetailsController c;
    c.setHost(QStringLiteral("127.0.0.1"));
    c.setUser(QStringLiteral("admin"));
    c.setPassword(kPassword);
    c.setPortForTest(port);

    QVariantMap fields;
    fields.insert(QStringLiteral("rfbPassword"), kRfbPassword);
    fields.insert(QStringLiteral("sessionTimeoutMinutes"), 5);
    c.setKvmSettings(fields);
    settle(250);

    const QString log = LogCapture::all();
    QVERIFY2(!log.contains(kRfbPassword),
              qPrintable(QStringLiteral("the RFB password reached the log:\n%1").arg(log)));
    // The key names are safe and are what a diagnosis actually needs.
    QVERIFY2(log.contains(QStringLiteral("rfbPassword")),
              qPrintable(QStringLiteral("expected the patched field names:\n%1").arg(log)));
}

void TestLogRedaction::categoriesAreQuietByDefault()
{
    // Q_LOGGING_CATEGORY's default severity is QtDebugMsg, so the
    // two-arg form ships the whole phase-by-phase transcript to every
    // user's stderr with no environment variable set. Every category
    // must be declared with the three-arg QtInfoMsg form instead (#438).
    //
    // Tested behaviourally rather than by constructing a
    // QLoggingCategory here — that would only measure the constructor's
    // own default, not what the declarations in the .cpp files chose.
    //
    // The empty-host path is used because it emits both a warning and a
    // state transition synchronously and opens no socket at all, so the
    // test says nothing about platform connect timing.
    auto runOnce = [](bool verbose) {
        LogCapture cap(verbose);
        KvmController kvm;
        kvm.setUser(QStringLiteral("admin"));
        kvm.setPassword(kPassword);
        kvm.open();          // host deliberately empty
        return LogCapture::all();
    };

    const QString quiet = runOnce(/*verbose=*/false);
    const QString verbose = runOnce(/*verbose=*/true);

    // The refusal is a warning and must always reach the user.
    QVERIFY2(quiet.contains(QStringLiteral("no host set")),
              qPrintable(QStringLiteral("warnings must survive the default "
                                        "filter:\n%1").arg(quiet)));
    // The state-transition transcript is qCDebug and must not.
    QVERIFY2(!quiet.contains(QStringLiteral("state Disconnected ->")),
              qPrintable(QStringLiteral("the phase transcript is on by default — "
                                        "declare the category with QtInfoMsg:\n%1")
                             .arg(quiet)));
    // ...but must appear once the documented env var is set, otherwise
    // the logging is useless to a reporter.
    QVERIFY2(verbose.contains(QStringLiteral("state Disconnected ->")),
              qPrintable(QStringLiteral("the phase transcript never appears even "
                                        "with qumesh.*=true:\n%1").arg(verbose)));
}

QTEST_MAIN(TestLogRedaction)
#include "test_log_redaction.moc"
