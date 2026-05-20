// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "certmodel.h"

#include "certs/cert_parser.h"

#include <QFile>
#include <QFileInfo>

namespace qumesh::app {

using certs::CertEntry;
using certs::CertParser;
using certs::CertStore;

CertModel::CertModel(QObject *parent)
    : QAbstractListModel(parent), m_store(QString{})
{
}

void CertModel::setStorePath(const QString &path)
{
    beginResetModel();
    m_store = CertStore(path);
    QString err;
    bool loadOk = m_store.load(&err);
    endResetModel();
    if (!loadOk) setLastError(err);
}

int CertModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_store.entries().size();
}

QVariant CertModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_store.entries().size()) {
        return {};
    }
    const CertEntry &e = m_store.entries().at(index.row());
    switch (role) {
    case IdRole:            return e.id;
    case SubjectRole:       return e.subjectCommonName;
    case IssuerRole:        return e.issuerCommonName;
    case SerialRole:        return e.serial;
    case NotBeforeRole:     return e.notBefore;
    case NotAfterRole:      return e.notAfter;
    case FingerprintRole:   return e.fingerprintSha256;
    case HasPrivateKeyRole: return e.hasPrivateKey;
    default:                return {};
    }
}

QHash<int, QByteArray> CertModel::roleNames() const
{
    return {
        {IdRole,            "id"},
        {SubjectRole,       "subject"},
        {IssuerRole,        "issuer"},
        {SerialRole,        "serial"},
        {NotBeforeRole,     "notBefore"},
        {NotAfterRole,      "notAfter"},
        {FingerprintRole,   "fingerprint"},
        {HasPrivateKeyRole, "hasPrivateKey"},
    };
}

int CertModel::importFromFile(const QString &path, const QString &password)
{
    setLastError({});
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setLastError(f.errorString());
        return -1;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    CertEntry e;
    QString parseErr;
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix == QLatin1String("p12") || suffix == QLatin1String("pfx")) {
        e = CertParser::fromPkcs12(bytes, password, &parseErr);
    } else if (suffix == QLatin1String("pem") || suffix == QLatin1String("key")
               || bytes.contains("BEGIN CERTIFICATE")) {
        e = CertParser::fromPem(bytes, &parseErr);
    } else {
        e = CertParser::fromDer(bytes, &parseErr);
    }
    if (e.certDer.isEmpty()) {
        setLastError(parseErr.isEmpty()
            ? QStringLiteral("could not parse certificate")
            : parseErr);
        return -1;
    }

    // Save-first / mutate-model-after: we build a candidate store,
    // try to persist it, and only update the in-memory model + emit
    // begin/end notifications if the save succeeds. This avoids any
    // chance of telling views about phantom rows on rollback.
    CertStore next = m_store;
    const int existing = next.addOrReplace(e);

    {
        CertStore toSave = next;
        QString saveErr;
        if (!toSave.save(&saveErr)) {
            setLastError(saveErr);
            return -1;
        }
    }

    if (existing < m_store.entries().size()
        && m_store.entries().at(existing).id == e.id) {
        m_store = std::move(next);
        emit dataChanged(index(existing, 0), index(existing, 0));
        return existing;
    }
    beginInsertRows({}, existing, existing);
    m_store = std::move(next);
    endInsertRows();
    return existing;
}

bool CertModel::removeAt(int row)
{
    if (row < 0 || row >= m_store.entries().size()) return false;
    CertStore next = m_store;
    const QString id = m_store.entries().at(row).id;
    next.removeById(id);

    QString saveErr;
    if (!next.save(&saveErr)) {
        setLastError(saveErr);
        return false;
    }
    beginRemoveRows({}, row, row);
    m_store = std::move(next);
    endRemoveRows();
    return true;
}

bool CertModel::exportAsDer(int row, const QString &path)
{
    setLastError({});
    if (row < 0 || row >= m_store.entries().size()) {
        setLastError(QStringLiteral("row out of range"));
        return false;
    }
    const CertEntry &e = m_store.entries().at(row);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(f.errorString());
        return false;
    }
    return f.write(e.certDer) == e.certDer.size();
}

bool CertModel::exportAsPem(int row, const QString &path)
{
    setLastError({});
    if (row < 0 || row >= m_store.entries().size()) {
        setLastError(QStringLiteral("row out of range"));
        return false;
    }
    const QByteArray pem = CertParser::toPem(m_store.entries().at(row));
    if (pem.isEmpty()) {
        setLastError(QStringLiteral("could not encode PEM"));
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(f.errorString());
        return false;
    }
    return f.write(pem) == pem.size();
}

void CertModel::reload()
{
    QString err;
    beginResetModel();
    m_store.load(&err);
    endResetModel();
    setLastError(err);
}

void CertModel::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void CertModel::clearLastError() { setLastError({}); }

} // namespace qumesh::app
