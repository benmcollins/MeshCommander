// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "computermodel.h"

#include "configstore.h"
#include "secret_codec.h"

#include <QSet>
#include <QUuid>

namespace qumesh::model {

namespace {

constexpr quint16 kDefaultSshPort = 22;
constexpr int kMaxTcpPort = 65535;

quint16 clampSshPort(int port)
{
    return static_cast<quint16>(
        port > 0 && port <= kMaxTcpPort ? port : int(kDefaultSshPort));
}

/// Field-by-field deserializer shared by `Computer::fromJson` and
/// `Computer::fromPortableJson`. The only difference between the two
/// is whether secrets are run through `SecretCodec::decode` or trusted
/// as already-plaintext, so push that down to a callback and keep the
/// rest of the parsing in one place.
template <typename Unwrap>
Computer parseComputer(const QJsonObject &obj, Unwrap unwrap)
{
    Computer c;
    c.id = obj.value(QStringLiteral("id")).toString();
    if (c.id.isEmpty()) c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = obj.value(QStringLiteral("name")).toString();
    c.host = obj.value(QStringLiteral("host")).toString();
    c.user = obj.value(QStringLiteral("user")).toString();
    c.pass = unwrap(obj.value(QStringLiteral("pass")).toString());
    c.tls = obj.value(QStringLiteral("tls")).toBool();
    c.digestRealm = obj.value(QStringLiteral("digestrealm")).toString();
    const QJsonArray fps = obj.value(QStringLiteral("trustedFingerprints")).toArray();
    for (const QJsonValue &v : fps) {
        if (v.isString()) c.trustedFingerprints.push_back(v.toString());
    }
    c.sshTunnelEnabled = obj.value(QStringLiteral("sshTunnelEnabled")).toBool();
    c.sshHost = obj.value(QStringLiteral("sshHost")).toString();
    c.sshPort = clampSshPort(obj.value(QStringLiteral("sshPort")).toInt(kDefaultSshPort));
    c.sshUser = obj.value(QStringLiteral("sshUser")).toString();
    const int mode = obj.value(QStringLiteral("sshAuthMode")).toInt(0);
    c.sshAuthMode = (mode == int(Computer::SshAuthKey))
                       ? Computer::SshAuthKey
                       : Computer::SshAuthPassword;
    c.sshPassword = unwrap(obj.value(QStringLiteral("sshPassword")).toString());
    c.sshKeyPath = obj.value(QStringLiteral("sshKeyPath")).toString();
    c.sshKeyPassphrase = unwrap(obj.value(QStringLiteral("sshKeyPassphrase")).toString());
    const QJsonArray sshFps = obj.value(QStringLiteral("sshTrustedHostKeyFingerprints")).toArray();
    for (const QJsonValue &v : sshFps) {
        if (v.isString()) c.sshTrustedHostKeyFingerprints.push_back(v.toString());
    }
    c.sshCompression = obj.value(QStringLiteral("sshCompression")).toBool();
    return c;
}

template <typename Wrap>
QJsonObject buildComputerJson(const Computer &c, Wrap wrap)
{
    QJsonArray fps;
    for (const QString &fp : c.trustedFingerprints) fps.push_back(fp);
    QJsonArray sshFps;
    for (const QString &fp : c.sshTrustedHostKeyFingerprints) sshFps.push_back(fp);
    return QJsonObject{
        {QStringLiteral("id"), c.id},
        {QStringLiteral("name"), c.name},
        {QStringLiteral("host"), c.host},
        {QStringLiteral("user"), c.user},
        {QStringLiteral("pass"), wrap(c.pass)},
        {QStringLiteral("tls"), c.tls},
        {QStringLiteral("digestrealm"), c.digestRealm},
        {QStringLiteral("trustedFingerprints"), fps},
        {QStringLiteral("sshTunnelEnabled"), c.sshTunnelEnabled},
        {QStringLiteral("sshHost"), c.sshHost},
        {QStringLiteral("sshPort"), int(c.sshPort)},
        {QStringLiteral("sshUser"), c.sshUser},
        {QStringLiteral("sshAuthMode"), int(c.sshAuthMode)},
        {QStringLiteral("sshPassword"), wrap(c.sshPassword)},
        {QStringLiteral("sshKeyPath"), c.sshKeyPath},
        {QStringLiteral("sshKeyPassphrase"), wrap(c.sshKeyPassphrase)},
        {QStringLiteral("sshTrustedHostKeyFingerprints"), sshFps},
        {QStringLiteral("sshCompression"), c.sshCompression},
    };
}

} // namespace

QJsonObject Computer::toJson() const
{
    return buildComputerJson(*this, [](const QString &s) { return app::SecretCodec::encode(s); });
}

Computer Computer::fromJson(const QJsonObject &obj)
{
    return parseComputer(obj, [](const QString &s) { return app::SecretCodec::decode(s); });
}

QJsonObject Computer::toPortableJson() const
{
    return buildComputerJson(*this, [](const QString &s) { return s; });
}

Computer Computer::fromPortableJson(const QJsonObject &obj)
{
    return parseComputer(obj, [](const QString &s) { return s; });
}

ComputerModel::ComputerModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Re-emit row-count changes as a Q_PROPERTY NOTIFY so QML can bind
    // to `ComputerModel.count` and stay reactive. Without this the
    // TitleBar machine-count chip stayed stale until the model was
    // re-read (e.g. after a window reopen).
    connect(this, &QAbstractItemModel::rowsInserted,
            this, &ComputerModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved,
            this, &ComputerModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset,
            this, &ComputerModel::countChanged);
}

void ComputerModel::setStore(config::ConfigStore *store)
{
    beginResetModel();
    m_store = store;
    m_computers.clear();
    if (m_store != nullptr) {
        const QJsonArray arr = m_store->loadComputers();
        m_computers.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            if (v.isObject()) m_computers.push_back(Computer::fromJson(v.toObject()));
        }
    }
    endResetModel();
}

int ComputerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_computers.size());
}

QVariant ComputerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_computers.size())
        return {};
    const Computer &c = m_computers.at(index.row());
    switch (role) {
    case IdRole:
        return c.id;
    case Qt::DisplayRole:
    case NameRole:
        return c.name;
    case HostRole:
        return c.host;
    case UserRole:
        return c.user;
    case PassRole:
        return c.pass;
    case TlsRole:
        return c.tls;
    case DigestRealmRole:
        return c.digestRealm;
    case TrustedFingerprintsRole:
        return c.trustedFingerprints;
    }
    return {};
}

bool ComputerModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_computers.size())
        return false;

    Computer &c = m_computers[index.row()];
    Computer snapshot = c;

    switch (role) {
    case NameRole:
        c.name = value.toString();
        break;
    case HostRole:
        c.host = value.toString();
        break;
    case UserRole:
        c.user = value.toString();
        break;
    case PassRole:
        c.pass = value.toString();
        break;
    case TlsRole:
        c.tls = value.toBool();
        break;
    case DigestRealmRole:
        c.digestRealm = value.toString();
        break;
    default:
        return false;
    }

    if (!persist()) {
        c = snapshot;
        return false;
    }

    const QList<int> roles{role};
    emit dataChanged(index, index, roles);
    if (role == NameRole) emit dataChanged(index, index, {Qt::DisplayRole});
    return true;
}

Qt::ItemFlags ComputerModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    // ItemIsEditable applies to the row as a whole — Qt's flags() API
    // doesn't take a role, so we can't narrow by it here. setData()
    // is the gate that actually rejects unsupported roles (Id,
    // TrustedFingerprints, the SSH fields) by returning false. QML
    // doesn't have an item-view that probes flags() on a per-role
    // basis (it goes through Q_INVOKABLEs instead), so the apparent
    // mismatch isn't observable in practice. See #295.
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("id")},
        {NameRole, QByteArrayLiteral("name")},
        {HostRole, QByteArrayLiteral("host")},
        {UserRole, QByteArrayLiteral("user")},
        {PassRole, QByteArrayLiteral("pass")},
        {TlsRole, QByteArrayLiteral("tls")},
        {DigestRealmRole, QByteArrayLiteral("digestrealm")},
        {TrustedFingerprintsRole, QByteArrayLiteral("trustedFingerprints")},
    };
}

bool ComputerModel::addTrustedFingerprint(int row, const QString &fingerprint)
{
    if (row < 0 || row >= m_computers.size()) return false;
    Computer &c = m_computers[row];
    if (c.trustedFingerprints.contains(fingerprint)) return true;

    Computer snapshot = c;
    c.trustedFingerprints.append(fingerprint);
    if (!persist()) {
        c = snapshot;
        return false;
    }
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {TrustedFingerprintsRole});
    return true;
}

QVariantMap ComputerModel::sshConfigFor(int row) const
{
    if (row < 0 || row >= m_computers.size()) return {};
    const Computer &c = m_computers[row];
    QVariantList fps;
    for (const QString &fp : c.sshTrustedHostKeyFingerprints) fps.push_back(fp);
    return {
        {QStringLiteral("enabled"), c.sshTunnelEnabled},
        {QStringLiteral("host"), c.sshHost},
        {QStringLiteral("port"), int(c.sshPort)},
        {QStringLiteral("user"), c.sshUser},
        {QStringLiteral("authMode"), int(c.sshAuthMode)},
        {QStringLiteral("password"), c.sshPassword},
        {QStringLiteral("keyPath"), c.sshKeyPath},
        {QStringLiteral("keyPassphrase"), c.sshKeyPassphrase},
        {QStringLiteral("compression"), c.sshCompression},
        {QStringLiteral("trustedHostKeyFingerprints"), fps},
    };
}

bool ComputerModel::setSshConfig(int row, const QVariantMap &cfg)
{
    if (row < 0 || row >= m_computers.size()) return false;
    Computer &c = m_computers[row];
    Computer snapshot = c;
    c.sshTunnelEnabled = cfg.value(QStringLiteral("enabled")).toBool();
    c.sshHost = cfg.value(QStringLiteral("host")).toString();
    c.sshPort = clampSshPort(
        cfg.value(QStringLiteral("port"), kDefaultSshPort).toInt());
    c.sshUser = cfg.value(QStringLiteral("user")).toString();
    const int mode = cfg.value(QStringLiteral("authMode")).toInt();
    c.sshAuthMode = (mode == int(Computer::SshAuthKey))
                       ? Computer::SshAuthKey
                       : Computer::SshAuthPassword;
    c.sshPassword = cfg.value(QStringLiteral("password")).toString();
    c.sshKeyPath = cfg.value(QStringLiteral("keyPath")).toString();
    c.sshKeyPassphrase = cfg.value(QStringLiteral("keyPassphrase")).toString();
    c.sshCompression = cfg.value(QStringLiteral("compression")).toBool();
    // trustedHostKeyFingerprints is intentionally not overwritten by
    // the edit pane — it grows only via addTrustedSshHostKey (TOFU).
    if (!persist()) {
        c = snapshot;
        return false;
    }
    // QAbstractItemModel contract: emit dataChanged after any row
    // mutation, even for fields without explicit roles. Today QML
    // reads SSH config imperatively via `sshConfigFor()`, so missing
    // this signal was invisible — but the moment a binding starts
    // depending on these fields it would stale-read. See #284.
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
    return true;
}

bool ComputerModel::addTrustedSshHostKey(int row, const QString &fingerprint)
{
    if (row < 0 || row >= m_computers.size()) return false;
    Computer &c = m_computers[row];
    if (c.sshTrustedHostKeyFingerprints.contains(fingerprint)) return true;
    Computer snapshot = c;
    c.sshTrustedHostKeyFingerprints.append(fingerprint);
    if (!persist()) {
        c = snapshot;
        return false;
    }
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
    return true;
}

int ComputerModel::addComputer(const QString &name, const QString &host,
                               const QString &user, const QString &pass, bool tls)
{
    Computer c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = name;
    c.host = host;
    c.user = user;
    c.pass = pass;
    c.tls = tls;

    const int row = static_cast<int>(m_computers.size());
    beginInsertRows(QModelIndex(), row, row);
    m_computers.push_back(std::move(c));
    endInsertRows();

    if (!persist()) {
        beginRemoveRows(QModelIndex(), row, row);
        m_computers.pop_back();
        endRemoveRows();
        return -1;
    }
    return row;
}

bool ComputerModel::removeAt(int row)
{
    if (row < 0 || row >= m_computers.size()) return false;

    Computer snapshot = m_computers.at(row);

    beginRemoveRows(QModelIndex(), row, row);
    m_computers.removeAt(row);
    endRemoveRows();

    if (!persist()) {
        beginInsertRows(QModelIndex(), row, row);
        m_computers.insert(row, snapshot);
        endInsertRows();
        return false;
    }
    return true;
}

Computer ComputerModel::at(int row) const
{
    if (row < 0 || row >= m_computers.size()) return {};
    return m_computers.at(row);
}

QList<ComputerModel::ImportPlanEntry> ComputerModel::planImport(
    const QList<Computer> &incoming) const
{
    QList<ImportPlanEntry> plan;
    plan.reserve(incoming.size());

    // Used to enforce one-target-per-incoming so a backup with two
    // entries that both match the same existing row doesn't update
    // it twice (the second match would become an append in that
    // case, but the existing row would be silently overwritten).
    QSet<int> claimed;

    for (const Computer &c : incoming) {
        ImportPlanEntry e;
        e.incoming = c;

        // id match first.
        int hit = -1;
        if (!c.id.isEmpty()) {
            for (int i = 0; i < m_computers.size(); ++i) {
                if (claimed.contains(i)) continue;
                if (m_computers.at(i).id == c.id) { hit = i; break; }
            }
        }

        // (host, name) fallback. Host exact (case matters for IPs);
        // name case-insensitive because users rename machines casually.
        if (hit < 0) {
            for (int i = 0; i < m_computers.size(); ++i) {
                if (claimed.contains(i)) continue;
                const Computer &x = m_computers.at(i);
                if (x.host == c.host
                    && x.name.compare(c.name, Qt::CaseInsensitive) == 0) {
                    hit = i;
                    break;
                }
            }
        }

        if (hit >= 0) {
            claimed.insert(hit);
            e.existingId = m_computers.at(hit).id;
            e.match = ImportUpdate;
        } else {
            e.match = ImportNew;
        }
        plan.push_back(e);
    }
    return plan;
}

bool ComputerModel::applyImport(const QList<ImportPlanEntry> &plan)
{
    QList<Computer> snapshot = m_computers;

    beginResetModel();

    for (const ImportPlanEntry &e : plan) {
        Computer c = e.incoming;
        int target = -1;
        if (e.match == ImportUpdate && !e.existingId.isEmpty()) {
            // Re-resolve by id at apply time, not by the row index
            // captured during planImport. The two operations are
            // separated by an arbitrary UI delay (the confirm dialog)
            // during which the model may have been edited from
            // elsewhere — a stale absolute index could silently
            // overwrite an unrelated row.
            for (int i = 0; i < m_computers.size(); ++i) {
                if (m_computers.at(i).id == e.existingId) { target = i; break; }
            }
        }

        if (target >= 0) {
            // Reuse the existing row's id so QML's stable identity
            // (Loader keys, selection state) survives the import.
            c.id = m_computers.at(target).id;
            m_computers[target] = c;
        } else {
            // Either an ImportNew, or the matched row was deleted
            // while the confirm dialog sat open. Either way, treat
            // as an append — losing a never-actually-overwrote-
            // anything entry beats overwriting the wrong row.
            if (c.id.isEmpty())
                c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_computers.push_back(c);
        }
    }

    if (!persist()) {
        m_computers = snapshot;
        endResetModel();
        return false;
    }

    endResetModel();
    return true;
}

bool ComputerModel::persist()
{
    if (m_store == nullptr) return true; // in-memory only mode is allowed in tests
    QJsonArray arr;
    for (const Computer &c : m_computers) arr.push_back(c.toJson());
    if (!m_store->saveComputers(arr)) {
        m_lastError = m_store->lastError();
        return false;
    }
    m_lastError.clear();
    return true;
}

} // namespace qumesh::model
