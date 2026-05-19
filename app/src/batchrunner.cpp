// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "batchrunner.h"

#include <algorithm>

namespace qumesh::app {

BatchRunner::BatchRunner(QObject *parent) : QObject(parent) {}

void BatchRunner::setMaxConcurrency(int n)
{
    m_maxConcurrency = std::clamp(n, 1, 64);
}

bool BatchRunner::start(const QList<int> &rows, int action,
                         const QVariantMap &params)
{
    if (m_running) return false;
    if (!m_dispatcher) return false;
    if (rows.isEmpty()) return false;

    m_jobs.clear();
    m_jobs.reserve(rows.size());
    for (int row : rows) {
        Job j;
        j.row = row;
        j.status = BatchStatus::Pending;
        m_jobs.append(j);
    }
    m_currentAction = static_cast<BatchAction>(action);
    m_currentParams = params;
    m_inflight = 0;
    m_running = true;
    emit runningChanged();
    emit jobsChanged();

    pumpQueue();
    return true;
}

bool BatchRunner::retryFailed()
{
    if (m_running) return false;
    if (!m_dispatcher) return false;

    bool anyFailed = false;
    for (auto &j : m_jobs) {
        if (j.status == BatchStatus::DoneFailed) {
            j.status = BatchStatus::Pending;
            j.detail.clear();
            anyFailed = true;
        }
    }
    if (!anyFailed) return false;

    m_inflight = 0;
    m_running = true;
    emit runningChanged();
    emit jobsChanged();
    pumpQueue();
    return true;
}

void BatchRunner::pumpQueue()
{
    // Walk the job list and dispatch every Pending one we can fit
    // under the concurrency cap. The dispatcher is async — `reply`
    // may fire from this same call frame (cheap dispatchers in tests)
    // or much later (real WSMAN). The reply callback owns reentry
    // through `completeOne` and back into `pumpQueue`.
    for (int i = 0; i < m_jobs.size() && m_inflight < m_maxConcurrency; ++i) {
        if (m_jobs[i].status != BatchStatus::Pending) continue;
        m_jobs[i].status = BatchStatus::Running;
        ++m_inflight;
        emitJobChanged(i);
        // Capture by value: `this` may outlive the batch, but the
        // dispatcher reply must still find a valid job index. We
        // capture the row, not the index, because retries can shift
        // job ordering — match by row when the reply lands.
        const int row = m_jobs[i].row;
        m_dispatcher(row, m_currentAction, m_currentParams,
            [this, row](Reply r) {
                // Find the matching job slot. Linear scan over the
                // job list is fine — batch sizes are interactive
                // (10s of hosts, occasionally 100s).
                for (int k = 0; k < m_jobs.size(); ++k) {
                    if (m_jobs[k].row == row
                        && m_jobs[k].status == BatchStatus::Running) {
                        completeOne(k, std::move(r));
                        return;
                    }
                }
                // No matching Running slot. Either the batch was reset
                // or the dispatcher delivered a stale reply. Drop it.
            });
    }

    // Edge case: a batch consisting entirely of Skipped jobs (e.g.
    // every selected host failed the trust-policy gate). Detect the
    // all-terminal state here.
    bool anyActive = false;
    for (const auto &j : m_jobs) {
        if (j.status == BatchStatus::Pending
            || j.status == BatchStatus::Running) {
            anyActive = true;
            break;
        }
    }
    if (!anyActive && m_running) {
        m_running = false;
        emit runningChanged();
        emit batchFinished();
    }
}

void BatchRunner::completeOne(int jobIndex, Reply reply)
{
    if (jobIndex < 0 || jobIndex >= m_jobs.size()) return;
    auto &job = m_jobs[jobIndex];
    // Normalize: a reply of Pending/Running is the dispatcher
    // misbehaving — treat as failed with the supplied detail.
    if (reply.status == BatchStatus::Pending
        || reply.status == BatchStatus::Running) {
        reply.status = BatchStatus::DoneFailed;
        if (reply.detail.isEmpty()) reply.detail = tr("dispatcher returned non-terminal status");
    }
    job.status = reply.status;
    job.detail = std::move(reply.detail);
    --m_inflight;
    emitJobChanged(jobIndex);

    // Keep the queue moving — but only if the batch is still active
    // (a retry could have already concluded if this slot was the
    // last Pending one).
    if (m_running) {
        // Pump more work; pumpQueue() handles the all-done detection.
        pumpQueue();
    }
}

void BatchRunner::emitJobChanged(int jobIndex)
{
    if (jobIndex < 0 || jobIndex >= m_jobs.size()) return;
    const auto &j = m_jobs[jobIndex];
    emit jobChanged(j.row, static_cast<int>(j.status), j.detail);
    emit jobsChanged();
}

QVariantList BatchRunner::jobs() const
{
    QVariantList out;
    out.reserve(m_jobs.size());
    for (const auto &j : m_jobs) {
        QVariantMap m;
        m["row"] = j.row;
        m["status"] = static_cast<int>(j.status);
        m["detail"] = j.detail;
        out.append(m);
    }
    return out;
}

int BatchRunner::doneCount() const
{
    int n = 0;
    for (const auto &j : m_jobs) if (j.status == BatchStatus::DoneOk) ++n;
    return n;
}

int BatchRunner::failedCount() const
{
    int n = 0;
    for (const auto &j : m_jobs) if (j.status == BatchStatus::DoneFailed) ++n;
    return n;
}

int BatchRunner::skippedCount() const
{
    int n = 0;
    for (const auto &j : m_jobs) if (j.status == BatchStatus::Skipped) ++n;
    return n;
}

} // namespace qumesh::app
