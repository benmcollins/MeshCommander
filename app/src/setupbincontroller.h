// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration>

#include "wsman/setupbin.h"

namespace qumesh::app {

/// QML-facing controller that owns the in-memory `SetupBinFile` the
/// editor window is mutating. Decode / encode / validate are delegated
/// to `qumesh::wsman::setupbin` so the UI never touches the byte format
/// directly; everything QML sees is a typed `QVariantMap`.
///
/// The whole file is exposed as a single `QVariantMap` so QML can drive
/// repeaters off a single binding. Every mutation flips `dirty` to true
/// (so the window can prompt before discarding) and emits `fileChanged`.
class SetupBinController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int version READ version WRITE setVersion NOTIFY fileChanged)
    Q_PROPERTY(bool doNotConsumeRecords READ doNotConsumeRecords
               WRITE setDoNotConsumeRecords NOTIFY fileChanged)
    Q_PROPERTY(int recordCount READ recordCount NOTIFY fileChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)

public:
    explicit SetupBinController(QObject *parent = nullptr);

    [[nodiscard]] int version() const { return m_file.version; }
    void setVersion(int v);
    [[nodiscard]] bool doNotConsumeRecords() const { return (m_file.flags & 0x1) != 0; }
    void setDoNotConsumeRecords(bool on);
    [[nodiscard]] int recordCount() const { return m_file.records.size(); }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] QString currentPath() const { return m_currentPath; }
    [[nodiscard]] bool isDirty() const { return m_dirty; }

    /// Reset to an empty file with the given version. v must be 1..4.
    Q_INVOKABLE void newFile(int version = 2);

    /// Read `path` and replace the current file. Returns false on failure
    /// (see `lastError`). The path is remembered for "Save".
    Q_INVOKABLE bool openFromFile(const QString &path);

    /// Validate the current file (without writing); returns an empty
    /// string when valid, the failing-rule message otherwise.
    Q_INVOKABLE QString validate() const;

    /// Validate, encode, write to `path`. Returns false on failure
    /// (validation or I/O); see `lastError`. The path becomes the new
    /// "current path".
    Q_INVOKABLE bool saveToFile(const QString &path);

    /// Append an empty record (with `valid` flag set) and return its
    /// index.
    Q_INVOKABLE int addRecord();
    Q_INVOKABLE bool removeRecord(int recordIndex);
    Q_INVOKABLE bool setRecordScrambled(int recordIndex, bool scrambled);

    /// Append a variable inside `recordIndex` for the given catalog
    /// tuple. The value is initialized to the type's empty default
    /// (zero-byte for binary, 0 for numeric, 16 NUL bytes for GUID).
    /// Returns the new variable index inside the record, or -1 if the
    /// tuple isn't in the catalog or the indices are out of range.
    Q_INVOKABLE int addVariable(int recordIndex, int moduleId, int varId);
    Q_INVOKABLE bool removeVariable(int recordIndex, int varIndex);

    /// Set the variable value. `value` is interpreted per the variable's
    /// catalog type:
    ///   - Binary  : QString → UTF-8 bytes, or QByteArray pass-through.
    ///   - U8/U16/U32 : numeric (any QVariant convertible to qulonglong).
    ///   - GUID    : QByteArray of exactly 16 bytes, or hex/UUID string.
    /// Returns false (and sets lastError) on type mismatch.
    Q_INVOKABLE bool setVariableValue(int recordIndex, int varIndex,
                                       const QVariant &value);

    /// Load raw bytes from `path` and assign them to the variable as a
    /// Binary value. Useful for the cert-add field which carries a
    /// binary blob the user typically has on disk.
    Q_INVOKABLE bool setVariableValueFromFile(int recordIndex, int varIndex,
                                              const QString &path);

    /// Snapshot of the full file as a QVariantMap, easy to bind to QML
    /// (Repeater / ListView / Text). Keys:
    ///   - "version" : int
    ///   - "doNotConsumeRecords" : bool
    ///   - "records" : QVariantList of records, each:
    ///       - "scrambled" : bool
    ///       - "variables" : QVariantList of variables, each:
    ///           - "moduleId" : int
    ///           - "varId"    : int
    ///           - "name"     : QString  (catalog name)
    ///           - "typeCode" : int      (SetupBinVarType)
    ///           - "valueDisplay" : QString (decoded for human display)
    ///           - "rawValue" : QByteArray (the bytes as-stored)
    Q_INVOKABLE QVariantMap fileSnapshot() const;

    /// Full catalog as a QVariantList — used to populate the "add
    /// variable" dropdown. Each entry has `moduleId`, `varId`, `name`,
    /// `typeCode`, and (when present) `enumValues` (QVariantList of
    /// `{value, label}` maps).
    Q_INVOKABLE QVariantList catalog() const;

signals:
    void fileChanged();
    void lastErrorChanged();
    void currentPathChanged();
    void dirtyChanged();

private:
    void markDirty();
    void setLastError(const QString &err);
    void setCurrentPath(const QString &path);

    [[nodiscard]] QVariantMap variableSnapshot(const qumesh::wsman::SetupBinVariable &var) const;
    [[nodiscard]] QString decodeForDisplay(const qumesh::wsman::SetupBinVariable &var) const;

    qumesh::wsman::SetupBinFile m_file;
    QString m_lastError;
    QString m_currentPath;
    bool m_dirty = false;
};

} // namespace qumesh::app
