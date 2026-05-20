// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QString>

namespace qumesh::config {
class ConfigStore;
}

namespace qumesh::app {

/// Bridges the standalone `migrate` library into the Qt app's startup flow.
///
/// On `checkAndMaybeMigrate()` the controller decides whether a one-shot
/// import from the legacy NW.js install is required (no native config yet
/// AND a legacy directory exists), then runs the migrator synchronously
/// and reports the outcome through the `state` property. The result is
/// observable from QML so the UI can show a brief notice on first launch.
class MigrationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    // Distinct NOTIFY so a future caller that updates the count
    // without touching the message still re-binds. Pre-#287 both
    // properties shared `messageChanged` — fine today because the
    // count is always set immediately before the message, but a
    // contract violation against Qt's "NOTIFY fires iff value
    // changed" expectation.
    Q_PROPERTY(int computersImported READ computersImported
                   NOTIFY computersImportedChanged)
public:
    // `enum class` matches SolController::State / KvmController::State
    // / IderController::State — was a plain `enum` here pre-#295.
    enum class State : int {
        Idle,         ///< Nothing to do (native config already exists, or no legacy data found)
        Migrating,    ///< Currently running the importer
        Imported,     ///< Migration succeeded
        Failed,       ///< Migration ran and reported an error
        NoLegacyData, ///< No native config but also no legacy install found
    };
    Q_ENUM(State)

    explicit MigrationController(QObject *parent = nullptr);

    /// Tell the controller which `ConfigStore` to consult for "is there
    /// already a native config?" and to which directory migrated JSON
    /// should be written.
    void setStore(config::ConfigStore *store);

    /// Override the platform-default legacy data dir. Used by tests to
    /// point at a synthetic location; empty selects the default.
    void setLegacyDataDir(QString path) { m_legacyDataDirOverride = std::move(path); }

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] int computersImported() const { return m_computersImported; }

    /// Decide whether to run a migration and run it synchronously. Safe
    /// to call once at startup; no-op when a native config already exists.
    Q_INVOKABLE void checkAndMaybeMigrate();

signals:
    void stateChanged();
    void messageChanged();
    void computersImportedChanged();

private:
    void setState(State s);
    void setMessage(QString m);

    config::ConfigStore *m_store = nullptr;
    QString m_legacyDataDirOverride;
    State m_state = State::Idle;
    QString m_message;
    int m_computersImported = 0;
};

} // namespace qumesh::app
