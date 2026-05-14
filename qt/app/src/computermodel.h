// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace qumesh::config {
class ConfigStore;
}

namespace qumesh::model {

/// In-memory representation of one row in `ComputerModel`. Mirrors the
/// subset of fields the legacy app stored under `computers[i]`. New
/// fields are added here, in `roleNames()`, in `data()`, and in
/// `setData()` — model contract has to stay in sync.
struct Computer
{
    QString id; // synthetic uuid for QML stable identity
    QString name;
    QString host;
    QString user;
    QString pass;
    bool tls = false;
    QString digestRealm;
    QStringList trustedFingerprints; ///< Pinned cert fingerprints for TLS.

    /// SSH tunnel configuration. When `sshTunnelEnabled` is true the
    /// `MachineDetailsController` opens an `SshSession` to
    /// `sshHost:sshPort` and forwards WSMAN / redir traffic to the AMT
    /// host through that session — no local listening ports involved.
    bool sshTunnelEnabled = false;
    QString sshHost;
    quint16 sshPort = 22;
    QString sshUser;
    enum SshAuthMode {
        SshAuthPassword = 0,
        SshAuthKey = 1,
    };
    SshAuthMode sshAuthMode = SshAuthPassword;
    QString sshPassword;
    QString sshKeyPath;
    QString sshKeyPassphrase;
    QStringList sshTrustedHostKeyFingerprints;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Computer fromJson(const QJsonObject &obj);
};

/// QAbstractListModel that exposes the computer list to QML and persists
/// every mutation through a `ConfigStore`. Pure storage — no live
/// polling. The detail window opens an explicit, per-machine WSMAN
/// session on user request; the model itself never connects.
class ComputerModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        UserRole,
        PassRole,
        TlsRole,
        DigestRealmRole,
        TrustedFingerprintsRole,
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
    Q_INVOKABLE int addComputer(const QString &name, const QString &host,
                                const QString &user, const QString &pass, bool tls);

    /// Delete the computer at `row`. Returns false (and leaves state intact)
    /// if `row` is out of range or persistence fails.
    Q_INVOKABLE bool removeAt(int row);

    /// Append a trusted cert fingerprint for the computer at `row`.
    /// Idempotent; duplicates are dropped. Returns false on persistence
    /// failure or out-of-range row.
    Q_INVOKABLE bool addTrustedFingerprint(int row, const QString &fingerprint);

    /// Return the SSH tunnel configuration for the computer at `row`
    /// as a `QVariantMap` (keys: `enabled`, `host`, `port`, `user`,
    /// `authMode`, `password`, `keyPath`, `keyPassphrase`,
    /// `trustedHostKeyFingerprints`). Empty map for out-of-range row.
    Q_INVOKABLE QVariantMap sshConfigFor(int row) const;

    /// Replace the SSH tunnel configuration for the computer at `row`
    /// with the values in `cfg`. Persists immediately. Returns false on
    /// out-of-range row or persistence failure.
    Q_INVOKABLE bool setSshConfig(int row, const QVariantMap &cfg);

    /// Append a trusted SSH host-key fingerprint at `row`. Idempotent.
    Q_INVOKABLE bool addTrustedSshHostKey(int row, const QString &fingerprint);

    /// Read-only accessor for tests / dialog state.
    [[nodiscard]] Computer at(int row) const;

private:
    [[nodiscard]] bool persist();

    config::ConfigStore *m_store = nullptr;
    QList<Computer> m_computers;
    QString m_lastError;
};

} // namespace qumesh::model
