// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
namespace qumesh::config {
class ConfigStore;
}

namespace qumesh::app {
class PowerStatePoller;
}

namespace qumesh::model {

/// In-memory representation of one row in `ComputerModel`. Mirrors the
/// subset of fields the legacy app stored under `computers[i]` that we
/// have UI for in Phase 2. New fields are added here, in `roleNames()`,
/// in `data()`, and in `setData()` — model contract has to stay in sync.
struct Computer
{
    QString id; // synthetic uuid for QML stable identity
    QString name;
    QString host;
    int port = 16992;
    QString user;
    QString pass;
    bool tls = false;
    QString digestRealm;
    QStringList trustedFingerprints; ///< Pinned cert fingerprints for TLS.

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Computer fromJson(const QJsonObject &obj);
};

/// QAbstractListModel that exposes the computer list to QML and persists
/// every mutation through a `ConfigStore`.
///
/// The model owns a backing `QList<Computer>` initialised from
/// `ConfigStore::loadComputers()`. Every mutating call emits the correct
/// `beginInsertRows`/`endInsertRows` (or `beginRemoveRows`/`endRemoveRows`
/// or `dataChanged`) wrap, then writes the new array through the store.
/// Persistence is best-effort: if a save fails, `lastError()` is set and
/// the model state is rolled back to the pre-mutation snapshot.
class ComputerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int countOn READ countOn NOTIFY fleetCountsChanged)
    Q_PROPERTY(int countOff READ countOff NOTIFY fleetCountsChanged)
    Q_PROPERTY(int countStandby READ countStandby NOTIFY fleetCountsChanged)
    Q_PROPERTY(int countUnreachable READ countUnreachable NOTIFY fleetCountsChanged)
    Q_PROPERTY(int countUnknown READ countUnknown NOTIFY fleetCountsChanged)
public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        PortRole,
        UserRole,
        PassRole,
        TlsRole,
        DigestRealmRole,
        TrustedFingerprintsRole,
        PowerStateRole,           ///< Live, set by per-row PowerStatePoller.
    };
    Q_ENUM(Role)

    explicit ComputerModel(QObject *parent = nullptr);

    /// Wire the model to a `ConfigStore` and reload from disk. Safe to call
    /// after the model has been constructed without a store (e.g. for tests
    /// that exercise the in-memory contract first).
    void setStore(config::ConfigStore *store);

    [[nodiscard]] config::ConfigStore *store() const { return m_store; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

    /// Fleet-aggregate accessors: count of rows currently in each
    /// `PowerStatePoller::State`. Updated together by the same poller
    /// signal that pushes per-row `PowerStateRole` updates, so the
    /// counts and per-row data stay coherent.
    [[nodiscard]] int countOn() const;
    [[nodiscard]] int countOff() const;
    [[nodiscard]] int countStandby() const;
    [[nodiscard]] int countUnreachable() const;
    [[nodiscard]] int countUnknown() const;

    // QAbstractListModel
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// Append a new computer and persist. Returns the new row index, or -1
    /// on save failure (the row is rolled back).
    Q_INVOKABLE int addComputer(const QString &name, const QString &host, int port,
                                const QString &user, const QString &pass, bool tls);

    /// Delete the computer at `row`. Returns false (and leaves state intact)
    /// if `row` is out of range or persistence fails.
    Q_INVOKABLE bool removeAt(int row);

    /// Append a trusted cert fingerprint for the computer at `row`.
    /// Idempotent; duplicates are dropped. Returns false on persistence
    /// failure or out-of-range row.
    Q_INVOKABLE bool addTrustedFingerprint(int row, const QString &fingerprint);

    /// Read-only accessor for tests / dialog state.
    [[nodiscard]] Computer at(int row) const;

signals:
    void fleetCountsChanged();

private:
    [[nodiscard]] bool persist();
    /// Recompute the fleet-aggregate counts from m_powerStates and
    /// emit `fleetCountsChanged` if anything moved.
    void recomputeFleetCounts();
    /// Build / refresh the per-row poller fleet to match the current
    /// computer list. Idempotent.
    void rebuildPollers();
    /// Disconnect + delete every poller.
    void tearDownPollers();

    config::ConfigStore *m_store = nullptr;
    QList<Computer> m_computers;
    QString m_lastError;
    QHash<QString, app::PowerStatePoller *> m_pollers;
    QHash<QString, int> m_powerStates; ///< id → poller State (cast to int)
    int m_countOn = 0;
    int m_countOff = 0;
    int m_countStandby = 0;
    int m_countUnreachable = 0;
    int m_countUnknown = 0;
};

} // namespace qumesh::model
