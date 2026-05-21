// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "backupcontroller.h"

#include "backup_codec.h"
#include "computermodel.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace qumesh::app {

using model::Computer;
using model::ComputerModel;

BackupController::BackupController(QObject *parent)
    : QObject(parent)
{
}

void BackupController::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void BackupController::setMessage(QString m)
{
    if (m_message == m) return;
    m_message = std::move(m);
    emit messageChanged();
}

void BackupController::setLastError(Error e)
{
    if (m_lastError == e) return;
    m_lastError = e;
    emit lastErrorCodeChanged();
}

void BackupController::setError(Error e, const QString &message)
{
    setLastError(e);
    setMessage(message);
    setState(State::Failed);
}

void BackupController::clearPreview()
{
    if (m_pendingPlan.isEmpty() && m_addCount == 0 && m_updateCount == 0) return;
    m_pendingPlan.clear();
    m_addCount = 0;
    m_updateCount = 0;
    emit previewChanged();
}

bool BackupController::exportTo(const QUrl &fileUrl, const QString &password)
{
    if (m_state == State::Working) return false; // single-shot guard
    if (m_model.isNull()) {
        setError(Error::Internal, tr("No machine model attached"));
        return false;
    }
    if (password.isEmpty()) {
        setError(Error::BadPassword, tr("A password is required to export"));
        return false;
    }

    // Wipe any in-flight import preview — an export operation
    // replaces whatever was staged. Otherwise QML observers reading
    // previewAddCount/previewUpdateCount after a successful export
    // would still see the prior import counts.
    clearPreview();

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    if (path.isEmpty()) {
        setError(Error::WriteFailed, tr("No destination file selected"));
        return false;
    }

    setState(State::Working);
    setLastError(Error::None);

    QJsonArray portable;
    for (int i = 0, n = m_model->rowCount(); i < n; ++i)
        portable.push_back(m_model->at(i).toPortableJson());

    const auto enc = BackupCodec::encode(portable, password);
    if (!enc.ok()) {
        setError(Error::Internal, tr("Encryption failed: %1").arg(enc.error));
        return false;
    }

    // QSaveFile so a half-written file never replaces an existing one
    // (covers both crash and disk-full). On every failure path we
    // call cancelWriting() explicitly — the destructor would do it
    // implicitly, but the explicit call pairs cleanly with the
    // explicit commit() on the success branch and is easier to audit.
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        setError(Error::WriteFailed,
                 tr("Couldn't open %1 for writing: %2").arg(path, out.errorString()));
        return false;
    }
    if (out.write(enc.bytes) != enc.bytes.size() || !out.commit()) {
        out.cancelWriting();
        setError(Error::WriteFailed,
                 tr("Couldn't write backup to %1: %2").arg(path, out.errorString()));
        return false;
    }

    setMessage(tr("Exported %1 machine(s) to %2")
                   .arg(m_model->rowCount())
                   .arg(QFileInfo(path).fileName()));
    setState(State::Done);
    return true;
}

bool BackupController::prepareImport(const QUrl &fileUrl, const QString &password)
{
    if (m_state == State::Working) return false; // single-shot guard
    // Any prior preview is invalidated regardless of where this
    // invocation fails — move clearPreview ABOVE the early returns
    // so QML observers can't read stale `previewAddCount` /
    // `previewUpdateCount` after a Failed transition. Empty-password
    // / null-model failures had been leaking the prior preview
    // through.
    clearPreview();

    if (m_model.isNull()) {
        setError(Error::Internal, tr("No machine model attached"));
        return false;
    }
    if (password.isEmpty()) {
        setError(Error::BadPassword, tr("A password is required to unlock the backup"));
        return false;
    }

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        setError(Error::BadFile, tr("Couldn't find %1").arg(path));
        return false;
    }
    // Reject obviously oversized files before mapping them into
    // memory. `BackupCodec::decode` enforces the same ceiling as a
    // defense-in-depth check; we duplicate it here so the disk read
    // itself can't be turned into a DoS by an attacker who chose the
    // file the user is about to open.
    if (info.size() > BackupCodec::kMaxFileBytes) {
        setError(Error::BadFormat,
                 tr("Backup file is too large to be a QuMesh backup"));
        return false;
    }

    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        setError(Error::BadFile,
                 tr("Couldn't open %1: %2").arg(path, in.errorString()));
        return false;
    }
    const QByteArray bytes = in.readAll();
    in.close();

    setState(State::Working);
    setLastError(Error::None);

    const auto dec = BackupCodec::decode(bytes, password);
    if (!dec.ok()) {
        switch (dec.reason) {
        case BackupCodec::FailReason::BadFormat:
            setError(Error::BadFormat, dec.error);
            return false;
        case BackupCodec::FailReason::BadPassword:
            setError(Error::BadPassword, dec.error);
            return false;
        case BackupCodec::FailReason::Corrupt:
            setError(Error::Corrupt, dec.error);
            return false;
        case BackupCodec::FailReason::Internal:
            setError(Error::Internal, dec.error);
            return false;
        case BackupCodec::FailReason::Ok:
            // Unreachable: dec.ok() returns true iff reason == Ok,
            // and we just checked !dec.ok(). Keep the case explicit
            // so the -Wswitch warning still fires on a future enum
            // addition.
            Q_UNREACHABLE();
        }
    }

    QList<Computer> incoming;
    incoming.reserve(dec.computers.size());
    for (const QJsonValue &v : std::as_const(dec.computers)) {
        if (v.isObject())
            incoming.push_back(Computer::fromPortableJson(v.toObject()));
    }

    m_pendingPlan = m_model->planImport(incoming);
    m_addCount = 0;
    m_updateCount = 0;
    for (const auto &e : std::as_const(m_pendingPlan)) {
        if (e.match == ComputerModel::ImportUpdate) ++m_updateCount;
        else                                        ++m_addCount;
    }
    emit previewChanged();

    setMessage(tr("Ready to import: %1 new, %2 updated")
                   .arg(m_addCount).arg(m_updateCount));
    setState(State::NeedsConfirm);
    return true;
}

bool BackupController::applyImport()
{
    if (m_state != State::NeedsConfirm) {
        // Surface the wrong-state attempt instead of returning
        // silently. Mirrors how exportTo / prepareImport route
        // failure through setError, so QML can render a message.
        setError(Error::Internal, tr("No import is awaiting confirmation"));
        return false;
    }
    if (m_model.isNull()) {
        setError(Error::Internal, tr("No machine model attached"));
        return false;
    }

    setState(State::Working);
    if (!m_model->applyImport(m_pendingPlan)) {
        setError(Error::SaveFailed,
                 tr("Saving the imported list failed: %1").arg(m_model->lastError()));
        clearPreview();
        return false;
    }

    const int added = m_addCount;
    const int updated = m_updateCount;
    clearPreview();
    setMessage(tr("Imported %1 new and updated %2 existing machine(s)")
                   .arg(added).arg(updated));
    setState(State::Done);
    return true;
}

void BackupController::cancelImport()
{
    // Always return to Idle. Pre-fix the reset was gated on
    // NeedsConfirm/Failed, which left a stale "Exported …" or
    // "Imported …" message in place after a Done state. QML uses
    // cancelImport as a generic "dismiss + reset" hook for both
    // dialogs, so an unconditional reset matches user expectation.
    clearPreview();
    setMessage(QString());
    setLastError(Error::None);
    setState(State::Idle);
}

} // namespace qumesh::app
