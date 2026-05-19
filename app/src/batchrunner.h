// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

#include "wsman/operations.h"

namespace qumesh::app {

/// The actions a batch can fan out across multiple selected hosts.
/// Match the menu items in `ComputerListView.qml`. Action-specific
/// parameters travel through `BatchRunner::Job::params` so the enum
/// stays small.
enum class BatchAction : int {
    PowerOn = 1,        ///< `RequestPowerStateChange(2)`
    PowerOffSoft,       ///< `RequestPowerStateChange(8)` — graceful shutdown
    PowerOffHard,       ///< `RequestPowerStateChange(12)` — force off
    PowerReset,         ///< `RequestPowerStateChange(10)` — reset graceful
    PowerCycle,         ///< `RequestPowerStateChange(5)` — power cycle
    SyncClock,          ///< `SetHighAccuracyTimeSynch` — push host time to AMT
    ClearAuditLog,      ///< `AMT_AuditLog.ClearLog`
    ClearEventLog,      ///< `AMT_MessageLog.ClearLog`
};

/// Per-host outcome for a single job. Surfaces in `BatchRunner::jobs`
/// so QML can render the progress drawer with one row per pending /
/// running / done / skipped tuple.
enum class BatchStatus : int {
    Pending = 0,        ///< Queued, not yet picked up by the runner.
    Running,            ///< Dispatched; awaiting the dispatcher's reply.
    DoneOk,             ///< Dispatcher returned `InvokeResult{ok = true}`.
    DoneFailed,         ///< Dispatcher returned `InvokeResult{ok = false}`.
    Skipped,            ///< Host didn't qualify (e.g. no pinned cert,
                        ///< SSH-tunneled — see batch trust policy).
};

/// Engine that runs the same action against many hosts in parallel,
/// emits per-host progress, and lets the UI retry just the failed
/// subset.
///
/// The runner is **dispatch-agnostic**: it doesn't know how to talk
/// WSMAN itself. The caller hands it a `Dispatcher` callable that
/// takes a single (`row`, `action`, `params`) tuple and asynchronously
/// reports the outcome. The app-side controller plugs in a dispatcher
/// that builds a `WsmanClient` per row and calls the matching primitive;
/// the unit tests plug in a stub. That seam is the entire abstraction
/// the runner depends on.
class BatchRunner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int totalJobs READ totalJobs NOTIFY jobsChanged)
    Q_PROPERTY(int doneCount READ doneCount NOTIFY jobsChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY jobsChanged)
    Q_PROPERTY(int skippedCount READ skippedCount NOTIFY jobsChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)

public:
    /// Reply shape passed back through the dispatcher's reply callback.
    /// Mirrors `wsman::InvokeResult` plus a `skipped` flavor for hosts
    /// the dispatcher rejects before it even tries (e.g. unpinned
    /// certs).
    struct Reply {
        BatchStatus status = BatchStatus::DoneFailed;
        QString detail;
    };

    using ReplyCallback = std::function<void(Reply)>;

    /// Type of the dispatch hook the caller plugs in. Must be async-
    /// friendly: invoke `reply` exactly once, possibly later (the
    /// runner holds the callback alive). The runner tracks concurrency
    /// itself; the dispatcher should never block.
    using Dispatcher = std::function<void(int row, BatchAction action,
                                          QVariantMap params,
                                          ReplyCallback reply)>;

    explicit BatchRunner(QObject *parent = nullptr);

    /// Plug in the dispatcher. Required before `start`.
    void setDispatcher(Dispatcher d) { m_dispatcher = std::move(d); }

    /// Bounded parallelism. Defaults to 8 — keeps batches of 100+
    /// hosts from creating 100 simultaneous WSMAN sessions. Per-AMT
    /// concurrency caps (each box accepts ~2 WSMAN connections) are a
    /// separate concern; this is a global "don't drown the UI" cap.
    void setMaxConcurrency(int n);
    [[nodiscard]] int maxConcurrency() const { return m_maxConcurrency; }

    /// Start a new batch. `rows` is the list of `ComputerModel` row
    /// indices to act on. `action` is one of the `BatchAction` values.
    /// `params` carries action-specific knobs (e.g. for `SyncClock` the
    /// runner inserts `ta0/tm1/tm2` into params on its own — callers
    /// don't have to).
    ///
    /// If a batch is already running, this call is dropped silently
    /// and returns false (so QML can't accidentally double-fire).
    Q_INVOKABLE bool start(const QList<int> &rows, int action,
                            const QVariantMap &params = {});

    /// Re-dispatch only the jobs whose status is `DoneFailed`. No-op
    /// while a batch is still in flight or when no jobs have failed.
    Q_INVOKABLE bool retryFailed();

    /// Snapshot of the current run as a QVariantList of QVariantMaps —
    /// one per row, with keys `row`, `status` (int, `BatchStatus`),
    /// `detail` (string). Suitable for QML's Repeater / ListView.
    [[nodiscard]] QVariantList jobs() const;
    [[nodiscard]] bool isRunning() const { return m_running; }
    [[nodiscard]] int totalJobs() const { return m_jobs.size(); }
    [[nodiscard]] int doneCount() const;
    [[nodiscard]] int failedCount() const;
    [[nodiscard]] int skippedCount() const;

signals:
    /// Emitted whenever a single job transitions. The QML side can
    /// either rebind to `jobs` (cheap — one rebuild per emit) or
    /// react to specific transitions for animations.
    void jobChanged(int row, int status, const QString &detail);

    /// Emitted when the batch transitions from running → idle (every
    /// job is in a terminal state). The summary counts are reachable
    /// via the property triplet `doneCount` / `failedCount` /
    /// `skippedCount`.
    void batchFinished();

    void runningChanged();
    void jobsChanged();

private:
    struct Job {
        int row = -1;
        BatchStatus status = BatchStatus::Pending;
        QString detail;
    };

    /// Try to dispatch as many `Pending` jobs as the concurrency cap
    /// allows. Called whenever a slot frees up.
    void pumpQueue();
    void completeOne(int jobIndex, Reply reply);
    void emitJobChanged(int jobIndex);

    Dispatcher m_dispatcher;
    int m_maxConcurrency = 8;
    QList<Job> m_jobs;
    BatchAction m_currentAction = BatchAction::PowerOn;
    QVariantMap m_currentParams;
    int m_inflight = 0;
    bool m_running = false;
};

} // namespace qumesh::app
