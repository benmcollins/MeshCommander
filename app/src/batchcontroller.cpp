// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "batchcontroller.h"

#include "computermodel.h"

#include "wsman/operations.h"
#include "wsman/wsman_client.h"

#include <QDateTime>
#include <QPointer>
#include <QUrl>

namespace qumesh::app {

namespace {

/// Build the WSMAN endpoint URL for one of the saved-computer rows
/// using the same convention `MachineDetailsController::rebuildEndpoint`
/// uses (TLS → 16993, otherwise 16992). Lifted out here so a future
/// follow-up that pulls SSH-tunneled rows into batch scope doesn't have
/// to duplicate it.
QUrl endpointForRow(const qumesh::model::Computer &c)
{
    QUrl url;
    url.setScheme(c.tls ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(c.host);
    url.setPort(c.tls ? 16993 : 16992);
    url.setPath(QStringLiteral("/wsman"));
    return url;
}

int powerStateCodeFor(BatchAction a)
{
    switch (a) {
    case BatchAction::PowerOn:       return 2;
    case BatchAction::PowerOffSoft:  return 8;
    case BatchAction::PowerOffHard:  return 12;
    case BatchAction::PowerReset:    return 10;
    case BatchAction::PowerCycle:    return 5;
    default:                          return 0;
    }
}

bool isPowerAction(BatchAction a)
{
    return powerStateCodeFor(a) != 0;
}

} // namespace

BatchController::BatchController(QObject *parent)
    : QObject(parent)
{
    // Forward the runner's signals so QML can bind to `BatchController`
    // alone and not need to reach through to the embedded runner.
    connect(&m_runner, &BatchRunner::runningChanged,
            this, &BatchController::runningChanged);
    connect(&m_runner, &BatchRunner::jobsChanged,
            this, &BatchController::jobsChanged);
    connect(&m_runner, &BatchRunner::batchFinished,
            this, &BatchController::batchFinished);
}

void BatchController::setComputerModel(qumesh::model::ComputerModel *model)
{
    m_model = model;
}

bool BatchController::start(const QList<int> &rows, int action)
{
    if (m_model == nullptr) return false;

    const auto act = static_cast<BatchAction>(action);

    // Set up the dispatcher fresh per batch start — it captures the
    // ComputerModel snapshot of each row at dispatch time (so a mid-
    // batch edit of host/credentials doesn't redirect an in-flight
    // session). `this` is the only owner of the WsmanClients spun up.
    m_runner.setDispatcher(
        [this, act](int row, BatchAction action, QVariantMap /*params*/,
                    BatchRunner::ReplyCallback reply) {
            (void) act; // current per-job action also lives in `action`
            if (m_model == nullptr) {
                reply({BatchStatus::DoneFailed, tr("computer model is gone")});
                return;
            }
            if (row < 0 || row >= m_model->rowCount()) {
                reply({BatchStatus::DoneFailed, tr("row %1 out of range").arg(row)});
                return;
            }
            const auto host = m_model->at(row);

            // Trust policy: a TLS host with no pinned fingerprints
            // wasn't visited via the single-host flow, so we don't
            // trust its cert yet. Refuse to fire WSMAN — silently
            // auto-trusting in a batch would be a security regression.
            if (host.tls && host.trustedFingerprints.isEmpty()) {
                reply({BatchStatus::Skipped,
                       tr("no pinned certificate — open this host once first")});
                return;
            }
            // SSH-tunneled hosts: out of scope for v1. The per-host
            // SSH-session lifecycle isn't tuned for N-parallel use yet.
            if (host.sshTunnelEnabled) {
                reply({BatchStatus::Skipped,
                       tr("SSH-tunneled hosts are not supported in batch yet")});
                return;
            }

            // Build a fresh per-job WsmanClient. Reparented to `this`
            // for lifetime; deleted in the result callback below so the
            // socket / TLS handshake doesn't outlive the reply.
            auto *client = new qumesh::wsman::WsmanClient(this);
            client->setEndpoint(endpointForRow(host));
            client->setCredentials(host.user, host.pass);
            client->setTrustedFingerprints(host.trustedFingerprints);
            // Single-flight per box keeps the WSMAN session count under
            // AMT's ~2-concurrent cap (each box gets its own client,
            // but the same host could appear in two ops back-to-back).
            client->setSerializeRequests(true);

            // Use a QPointer to guard against the BatchController going
            // away mid-flight — finishing reply tears down the client
            // and then sends the result. If the controller is already
            // gone, we just drop the result on the floor.
            const QPointer<BatchController> self(this);
            auto finish = [self, client, reply](BatchRunner::Reply r) {
                if (client) client->deleteLater();
                if (self) reply(std::move(r));
            };

            switch (action) {
            case BatchAction::PowerOn:
            case BatchAction::PowerOffSoft:
            case BatchAction::PowerOffHard:
            case BatchAction::PowerReset:
            case BatchAction::PowerCycle: {
                const int code = powerStateCodeFor(action);
                qumesh::wsman::requestPowerStateChange(client, code,
                    [finish](qumesh::wsman::InvokeResult r) {
                        BatchRunner::Reply br;
                        br.status = r.ok ? BatchStatus::DoneOk : BatchStatus::DoneFailed;
                        br.detail = r.ok ? QString() : r.error;
                        finish(std::move(br));
                    });
                break;
            }
            case BatchAction::SyncClock: {
                // Three-point time exchange, same as the per-host flow:
                // first read AMT's clock (Ta0), then push our clock
                // (Tm1/Tm2 — single batch step, both timestamps collapse
                // to the same instant since we don't measure per-host
                // RTT here).
                qumesh::wsman::getTimeSettings(client,
                    [client, finish](qumesh::wsman::TimeSettingsResult tr) {
                        if (!tr.ok) {
                            finish({BatchStatus::DoneFailed, tr.error});
                            return;
                        }
                        const qint64 ta0 = tr.secondsSinceEpoch;
                        const qint64 tmHost = QDateTime::currentSecsSinceEpoch();
                        qumesh::wsman::setHighAccuracyTimeSync(client,
                            ta0, tmHost, tmHost,
                            [finish](qumesh::wsman::InvokeResult r) {
                                BatchRunner::Reply br;
                                br.status = r.ok ? BatchStatus::DoneOk : BatchStatus::DoneFailed;
                                br.detail = r.ok ? QString() : r.error;
                                finish(std::move(br));
                            });
                    });
                break;
            }
            case BatchAction::ClearAuditLog:
                qumesh::wsman::clearAuditLog(client,
                    [finish](qumesh::wsman::InvokeResult r) {
                        BatchRunner::Reply br;
                        br.status = r.ok ? BatchStatus::DoneOk : BatchStatus::DoneFailed;
                        br.detail = r.ok ? QString() : r.error;
                        finish(std::move(br));
                    });
                break;
            case BatchAction::ClearEventLog:
                qumesh::wsman::clearEventLog(client,
                    [finish](qumesh::wsman::InvokeResult r) {
                        BatchRunner::Reply br;
                        br.status = r.ok ? BatchStatus::DoneOk : BatchStatus::DoneFailed;
                        br.detail = r.ok ? QString() : r.error;
                        finish(std::move(br));
                    });
                break;
            }
        });

    return m_runner.start(rows, action);
}

QString BatchController::actionLabel(int action) const
{
    switch (static_cast<BatchAction>(action)) {
    case BatchAction::PowerOn:       return tr("Power on");
    case BatchAction::PowerOffSoft:  return tr("Power off (soft)");
    case BatchAction::PowerOffHard:  return tr("Power off (hard)");
    case BatchAction::PowerReset:    return tr("Reset");
    case BatchAction::PowerCycle:    return tr("Power cycle");
    case BatchAction::SyncClock:     return tr("Sync clock");
    case BatchAction::ClearAuditLog: return tr("Clear audit log");
    case BatchAction::ClearEventLog: return tr("Clear event log");
    }
    Q_UNUSED(isPowerAction); // referenced by future cert/disable-AMT actions
    return tr("Unknown action %1").arg(action);
}

QString BatchController::statusLabel(int status) const
{
    switch (static_cast<BatchStatus>(status)) {
    case BatchStatus::Pending:    return tr("Pending");
    case BatchStatus::Running:    return tr("Running");
    case BatchStatus::DoneOk:     return tr("Done");
    case BatchStatus::DoneFailed: return tr("Failed");
    case BatchStatus::Skipped:    return tr("Skipped");
    }
    return tr("?");
}

} // namespace qumesh::app
