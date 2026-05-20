// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "certs/cert_store.h"

#include "certs/cert_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace qumesh::certs {

CertStore::CertStore(QString path) : m_path(std::move(path)) {}

bool CertStore::load(QString *error)
{
    m_entries.clear();
    QFile f(m_path);
    if (!f.exists()) return true; // first run — no file yet, that's fine
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();
    if (bytes.isEmpty()) return true;

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
    if (perr.error != QJsonParseError::NoError) {
        if (error) *error = perr.errorString();
        return false;
    }
    if (!doc.isArray()) {
        if (error) *error = QStringLiteral("certificates.json is not a JSON array");
        return false;
    }
    const QJsonArray arr = doc.array();
    m_entries.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        CertEntry e = CertEntry::fromJson(v.toObject());
        // Backfill metadata + canonical id from the DER if missing.
        if (e.id.isEmpty() && !e.certDer.isEmpty()) {
            CertParser::populateMetadata(&e, nullptr);
        }
        if (e.certDer.isEmpty()) continue;
        m_entries.push_back(std::move(e));
    }
    return true;
}

bool CertStore::save(QString *error)
{
    QJsonArray arr;
    for (const CertEntry &e : m_entries) arr.append(e.toJson());

    QSaveFile sf(m_path);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = sf.errorString();
        return false;
    }
    const QByteArray bytes = QJsonDocument(arr).toJson(QJsonDocument::Indented);
    if (sf.write(bytes) != bytes.size()) {
        if (error) *error = sf.errorString();
        sf.cancelWriting();
        return false;
    }
    if (!sf.commit()) {
        if (error) *error = sf.errorString();
        return false;
    }
    // certificates.json embeds PEM private keys; restrict to 0600 so
    // they don't leak via a world-readable file. See #273.
    QFile::setPermissions(m_path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

int CertStore::addOrReplace(CertEntry entry)
{
    const int idx = indexOf(entry.id);
    if (idx >= 0) {
        m_entries[idx] = std::move(entry);
        return idx;
    }
    m_entries.push_back(std::move(entry));
    return m_entries.size() - 1;
}

bool CertStore::removeById(const QString &id)
{
    const int idx = indexOf(id);
    if (idx < 0) return false;
    m_entries.removeAt(idx);
    return true;
}

int CertStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) return i;
    }
    return -1;
}

} // namespace qumesh::certs
