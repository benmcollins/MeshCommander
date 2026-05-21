// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include "computermodel.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

namespace qumesh::app {

/// QML-facing controller for the password-protected backup of the
/// machine list. Two flows:
///
/// - Export: `exportTo(fileUrl, password)` serializes the entire
///   model via `Computer::toPortableJson`, seals it with
///   `BackupCodec`, and writes the file.
///
/// - Import is two-stage:
///     1. `prepareImport(fileUrl, password)` reads + decrypts the
///        file, computes the diff against the current model, and
///        moves to `State::NeedsConfirm` with `previewAddCount` /
///        `previewUpdateCount` populated.
///     2. `applyImport()` commits the staged plan via
///        `ComputerModel::applyImport`.
///        `cancelImport()` discards the staged plan instead.
///
/// `State` follows the same `enum class` convention as `SolController`
/// / `KvmController` / `IderController` / `MigrationController`.
class BackupController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BackupController)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(int previewAddCount READ previewAddCount NOTIFY previewChanged)
    Q_PROPERTY(int previewUpdateCount READ previewUpdateCount NOTIFY previewChanged)
    Q_PROPERTY(int previewTotal READ previewTotal NOTIFY previewChanged)
    /// Surfaced to QML so dialog `Text` bindings re-evaluate on each
    /// new error. The typed `Error lastError()` accessor stays for
    /// C++ callers; QML uses this `int` view via the enum's underlying
    /// integer values (`BackupController.Error.BadPassword` etc).
    Q_PROPERTY(int lastErrorCode READ lastErrorCode NOTIFY lastErrorCodeChanged)
public:
    enum class State : int {
        Idle,
        Working,
        NeedsConfirm, ///< prepareImport succeeded; waiting on user to apply or cancel.
        Done,
        Failed,
    };
    Q_ENUM(State)

    /// Distinct failure reasons so QML can render the right error
    /// copy. Mirrors `BackupCodec::FailReason` with the file-IO
    /// cases that the codec doesn't see, and a generic "save failed"
    /// for export-side I/O.
    enum class Error : int {
        None = 0,
        BadFile,        ///< Couldn't read the import file at all.
        BadFormat,      ///< Not a qumesh-backup file (or unknown version).
        BadPassword,    ///< Password didn't decrypt the envelope.
        Corrupt,        ///< Backup file is internally inconsistent.
        WriteFailed,    ///< Couldn't write the export file.
        SaveFailed,     ///< applyImport's persist call failed.
        Internal,       ///< OpenSSL or other unexpected failure.
    };
    Q_ENUM(Error)

    explicit BackupController(QObject *parent = nullptr);

    /// `m_model` is wrapped in `QPointer` so a destruction-before-the-
    /// controller scenario (would only matter under a future async
    /// continuation reaching back here) auto-nulls instead of dangling.
    void setModel(model::ComputerModel *m) { m_model = m; }

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] int previewAddCount() const { return m_addCount; }
    [[nodiscard]] int previewUpdateCount() const { return m_updateCount; }
    [[nodiscard]] int previewTotal() const { return m_addCount + m_updateCount; }
    [[nodiscard]] Error lastError() const { return m_lastError; }

    Q_INVOKABLE bool exportTo(const QUrl &fileUrl, const QString &password);
    Q_INVOKABLE bool prepareImport(const QUrl &fileUrl, const QString &password);
    Q_INVOKABLE bool applyImport();
    Q_INVOKABLE void cancelImport();

    /// Q_INVOKABLE so QML can switch on the Error enum without
    /// hardcoding strings. Stable wording, translated via qsTr in the
    /// dialog itself.
    [[nodiscard]] Q_INVOKABLE int lastErrorCode() const { return int(m_lastError); }

signals:
    void stateChanged();
    void messageChanged();
    void previewChanged();
    void lastErrorCodeChanged();

private:
    void setState(State s);
    void setMessage(QString m);
    void setLastError(Error e);
    void setError(Error e, const QString &message);
    void clearPreview();

    QPointer<model::ComputerModel> m_model;
    State m_state = State::Idle;
    QString m_message;
    Error m_lastError = Error::None;
    QList<model::ComputerModel::ImportPlanEntry> m_pendingPlan;
    int m_addCount = 0;
    int m_updateCount = 0;
};

} // namespace qumesh::app
