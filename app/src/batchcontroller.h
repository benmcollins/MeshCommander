// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "batchrunner.h"

namespace qumesh::model { class ComputerModel; }

namespace qumesh::app {

/// QML-facing front for batch actions. Holds a `BatchRunner`, plugs in
/// the production dispatcher that builds a per-host `WsmanClient` and
/// fires the matching `wsman::*` primitive, and forwards the runner's
/// `Q_PROPERTY` surface so QML doesn't need to know two objects exist.
///
/// Lives at app-singleton scope (one per `Main.qml`). Constructed in
/// `main.cpp` and handed to the QML side via `registerQumeshQmlTypes`.
class BatchController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int totalJobs READ totalJobs NOTIFY jobsChanged)
    Q_PROPERTY(int doneCount READ doneCount NOTIFY jobsChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY jobsChanged)
    Q_PROPERTY(int skippedCount READ skippedCount NOTIFY jobsChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)

public:
    explicit BatchController(QObject *parent = nullptr);

    void setComputerModel(qumesh::model::ComputerModel *model);

    [[nodiscard]] bool isRunning() const { return m_runner.isRunning(); }
    [[nodiscard]] int totalJobs() const { return m_runner.totalJobs(); }
    [[nodiscard]] int doneCount() const { return m_runner.doneCount(); }
    [[nodiscard]] int failedCount() const { return m_runner.failedCount(); }
    [[nodiscard]] int skippedCount() const { return m_runner.skippedCount(); }
    [[nodiscard]] QVariantList jobs() const { return m_runner.jobs(); }

    /// `action` matches `BatchAction`. The dispatcher honors the
    /// "skip TLS hosts without a pinned cert" and "skip SSH-tunneled
    /// hosts (v1 limitation)" rules before any WSMAN traffic.
    Q_INVOKABLE bool start(const QList<int> &rows, int action);

    Q_INVOKABLE bool retryFailed() { return m_runner.retryFailed(); }

    /// Human-readable label for a `BatchAction` enum value. Useful for
    /// the progress drawer header.
    Q_INVOKABLE QString actionLabel(int action) const;

    /// Human-readable label for a `BatchStatus` enum value.
    Q_INVOKABLE QString statusLabel(int status) const;

signals:
    void runningChanged();
    void jobsChanged();
    void batchFinished();

private:
    BatchRunner m_runner;
    qumesh::model::ComputerModel *m_model = nullptr;
};

} // namespace qumesh::app
