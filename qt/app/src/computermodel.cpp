// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "computermodel.h"

#include "configstore.h"

#include <QUuid>

namespace qumesh::model {

QJsonObject Computer::toJson() const
{
    QJsonArray fps;
    for (const QString &fp : trustedFingerprints) fps.push_back(fp);
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("host"), host},
        {QStringLiteral("user"), user},
        {QStringLiteral("pass"), pass},
        {QStringLiteral("tls"), tls},
        {QStringLiteral("digestrealm"), digestRealm},
        {QStringLiteral("trustedFingerprints"), fps},
    };
}

Computer Computer::fromJson(const QJsonObject &obj)
{
    Computer c;
    c.id = obj.value(QStringLiteral("id")).toString();
    if (c.id.isEmpty()) c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.name = obj.value(QStringLiteral("name")).toString();
    c.host = obj.value(QStringLiteral("host")).toString();
    c.user = obj.value(QStringLiteral("user")).toString();
    c.pass = obj.value(QStringLiteral("pass")).toString();
    c.tls = obj.value(QStringLiteral("tls")).toBool();
    c.digestRealm = obj.value(QStringLiteral("digestrealm")).toString();
    const QJsonArray fps = obj.value(QStringLiteral("trustedFingerprints")).toArray();
    for (const QJsonValue &v : fps) {
        if (v.isString()) c.trustedFingerprints.push_back(v.toString());
    }
    return c;
}

ComputerModel::ComputerModel(QObject *parent)
    : QAbstractListModel(parent)
{
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
