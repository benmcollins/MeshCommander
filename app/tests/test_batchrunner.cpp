// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "batchrunner.h"

#include <QSignalSpy>
#include <QtTest>

#include <algorithm>

using qumesh::app::BatchAction;
using qumesh::app::BatchRunner;
using qumesh::app::BatchStatus;

class TestBatchRunner : public QObject
{
    Q_OBJECT
private slots:
    void startRefusesWithoutDispatcher();
    void startRefusesWhileRunning();
    void everyRowTransitionsPendingRunningDone();
    void failuresDontAbortTheBatch();
    void retryFailedOnlyRedispatchesFailures();
    void skippedJobsAreTerminalAndNeverDispatched();
    void concurrencyCapBoundsInflightJobs();
    void batchFinishedFiresExactlyOnce();
};

namespace {

// A dispatcher whose every reply is delayed until `flush()` runs them.
// Lets the test step through multi-host scheduling deterministically.
struct DeferredDispatcher
{
    struct Pending {
        int row;
        BatchRunner::ReplyCallback reply;
    };
    QList<Pending> queue;
    QList<int> rowOrder;
    QHash<int, BatchRunner::Reply> programmed;

    BatchRunner::Dispatcher asCallable()
    {
        return [this](int row, BatchAction, QVariantMap,
                       BatchRunner::ReplyCallback reply) {
            rowOrder.append(row);
            queue.append({row, std::move(reply)});
        };
    }

    void flushFor(int row)
    {
        for (int i = 0; i < queue.size(); ++i) {
            if (queue[i].row != row) continue;
            auto reply = std::move(queue[i].reply);
            queue.removeAt(i);
            BatchRunner::Reply r = programmed.value(row,
                BatchRunner::Reply{BatchStatus::DoneOk, {}});
            reply(std::move(r));
            return;
        }
    }

    void flushAll()
    {
        // Pop until empty — flushing may queue more (if a retry was kicked
        // off mid-flush), so loop on `queue.size()`.
        while (!queue.isEmpty()) {
            auto p = std::move(queue.takeFirst());
            BatchRunner::Reply r = programmed.value(p.row,
                BatchRunner::Reply{BatchStatus::DoneOk, {}});
            p.reply(std::move(r));
        }
    }
};

} // namespace

void TestBatchRunner::startRefusesWithoutDispatcher()
{
    BatchRunner r;
    QVERIFY(!r.start({1, 2, 3}, int(BatchAction::PowerOn)));
    QVERIFY(!r.isRunning());
    QCOMPARE(r.totalJobs(), 0);
}

void TestBatchRunner::startRefusesWhileRunning()
{
    BatchRunner r;
    DeferredDispatcher dd;
    r.setDispatcher(dd.asCallable());

    QVERIFY(r.start({1, 2, 3}, int(BatchAction::PowerOn)));
    QVERIFY(r.isRunning());
    QVERIFY(!r.start({4, 5}, int(BatchAction::PowerOn)));
    QCOMPARE(r.totalJobs(), 3);
}

void TestBatchRunner::everyRowTransitionsPendingRunningDone()
{
    BatchRunner r;
    DeferredDispatcher dd;
    r.setDispatcher(dd.asCallable());

    QSignalSpy spy(&r, &BatchRunner::jobChanged);
    QVERIFY(r.start({10, 20, 30}, int(BatchAction::SyncClock)));

    // All three are Running now (concurrency cap is 8 > 3).
    for (const auto &j : r.jobs()) {
        const auto m = j.toMap();
        QCOMPARE(m["status"].toInt(), int(BatchStatus::Running));
    }
    QCOMPARE(dd.queue.size(), 3);

    // Flush replies in arbitrary order.
    dd.flushFor(20);
    dd.flushFor(10);
    dd.flushFor(30);

    QVERIFY(!r.isRunning());
    QCOMPARE(r.doneCount(), 3);
    QCOMPARE(r.failedCount(), 0);
    // 3 Running emits + 3 Done emits = 6 transitions.
    QCOMPARE(spy.count(), 6);
}

void TestBatchRunner::failuresDontAbortTheBatch()
{
    BatchRunner r;
    DeferredDispatcher dd;
    dd.programmed[2] = {BatchStatus::DoneFailed,
                        QStringLiteral("simulated 401")};
    r.setDispatcher(dd.asCallable());

    QSignalSpy finishedSpy(&r, &BatchRunner::batchFinished);
    QVERIFY(r.start({1, 2, 3, 4, 5}, int(BatchAction::ClearAuditLog)));
    dd.flushAll();

    QCOMPARE(r.doneCount(), 4);
    QCOMPARE(r.failedCount(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    // The failed row carries the detail.
    for (const auto &j : r.jobs()) {
        const auto m = j.toMap();
        if (m["row"].toInt() == 2) {
            QCOMPARE(m["status"].toInt(), int(BatchStatus::DoneFailed));
            QCOMPARE(m["detail"].toString(), QStringLiteral("simulated 401"));
        } else {
            QCOMPARE(m["status"].toInt(), int(BatchStatus::DoneOk));
        }
    }
}

void TestBatchRunner::retryFailedOnlyRedispatchesFailures()
{
    BatchRunner r;
    DeferredDispatcher dd;
    dd.programmed[2] = {BatchStatus::DoneFailed, QStringLiteral("transient")};
    dd.programmed[4] = {BatchStatus::DoneFailed, QStringLiteral("transient")};
    r.setDispatcher(dd.asCallable());

    QVERIFY(r.start({1, 2, 3, 4, 5}, int(BatchAction::PowerReset)));
    dd.flushAll();
    QCOMPARE(r.failedCount(), 2);
    QCOMPARE(dd.rowOrder.size(), 5);

    // Flip the programmed answers so the retry succeeds, then retry.
    dd.programmed.remove(2);
    dd.programmed.remove(4);
    dd.rowOrder.clear();
    QVERIFY(r.retryFailed());
    QVERIFY(r.isRunning());

    // Only the two previously-failed rows should appear in the new
    // dispatch order.
    dd.flushAll();
    QVERIFY(!r.isRunning());
    QCOMPARE(dd.rowOrder.size(), 2);
    std::sort(dd.rowOrder.begin(), dd.rowOrder.end());
    QCOMPARE(dd.rowOrder, (QList<int>{2, 4}));
    QCOMPARE(r.doneCount(), 5);
    QCOMPARE(r.failedCount(), 0);
}

void TestBatchRunner::skippedJobsAreTerminalAndNeverDispatched()
{
    // The runner has no built-in "skip if untrusted" gate — that's the
    // app-side dispatcher's job. Confirm that *if* the dispatcher
    // replies with Skipped, the runner treats it as terminal and never
    // re-dispatches via retryFailed.
    BatchRunner r;
    DeferredDispatcher dd;
    dd.programmed[7] = {BatchStatus::Skipped,
                        QStringLiteral("no pinned cert")};
    r.setDispatcher(dd.asCallable());

    QVERIFY(r.start({7}, int(BatchAction::PowerOn)));
    dd.flushAll();

    QCOMPARE(r.skippedCount(), 1);
    QCOMPARE(r.failedCount(), 0);
    // Skipped is not eligible for retry-failed.
    dd.rowOrder.clear();
    QVERIFY(!r.retryFailed());
    QCOMPARE(dd.rowOrder.size(), 0);
}

void TestBatchRunner::concurrencyCapBoundsInflightJobs()
{
    BatchRunner r;
    DeferredDispatcher dd;
    r.setDispatcher(dd.asCallable());
    r.setMaxConcurrency(3);

    QVERIFY(r.start({1, 2, 3, 4, 5, 6, 7, 8, 9}, int(BatchAction::PowerOn)));

    auto runningCount = [&]() {
        int n = 0;
        for (const auto &v : r.jobs()) {
            if (v.toMap()["status"].toInt() == int(BatchStatus::Running)) ++n;
        }
        return n;
    };

    // Only 3 should be Running at once.
    QCOMPARE(runningCount(), 3);
    QCOMPARE(dd.queue.size(), 3);

    // Complete one — another should immediately enter Running.
    dd.flushFor(dd.queue.first().row);
    QCOMPARE(runningCount(), 3);
    QCOMPARE(dd.queue.size(), 3);

    // Drain the rest.
    dd.flushAll();
    QVERIFY(!r.isRunning());
    QCOMPARE(r.doneCount(), 9);
}

void TestBatchRunner::batchFinishedFiresExactlyOnce()
{
    BatchRunner r;
    DeferredDispatcher dd;
    r.setDispatcher(dd.asCallable());
    QSignalSpy spy(&r, &BatchRunner::batchFinished);

    QVERIFY(r.start({1, 2}, int(BatchAction::ClearEventLog)));
    dd.flushAll();
    QCOMPARE(spy.count(), 1);

    // No second emit on idle re-emits or a refused retry.
    QVERIFY(!r.retryFailed());
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TestBatchRunner)
#include "test_batchrunner.moc"
