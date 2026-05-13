// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
namespace meshcommander::config {
class ConfigStore;
}

namespace meshcommander::model {

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
    };
    Q_ENUM(Role)

    explicit ComputerModel(QObject *parent = nullptr);

    /// Wire the model to a `ConfigStore` and reload from disk. Safe to call
    /// after the model has been constructed without a store (e.g. for tests
    /// that exercise the in-memory contract first).
    void setStore(config::ConfigStore *store);

    [[nodiscard]] config::ConfigStore *store() const { return m_store; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

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

    /// Read-only accessor for tests / dialog state.
    [[nodiscard]] Computer at(int row) const;

private:
    [[nodiscard]] bool persist();

    config::ConfigStore *m_store = nullptr;
    QList<Computer> m_computers;
    QString m_lastError;
};

} // namespace meshcommander::model
