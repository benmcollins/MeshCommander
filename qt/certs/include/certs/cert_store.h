// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include "certs/cert_entry.h"

namespace meshcommander::certs {

/// On-disk certificate store. Wraps reading and writing
/// `<configDir>/certificates.json` — the same file the migration tool
/// in #14 produces. Entries are persisted atomically via QSaveFile.
class CertStore
{
public:
    /// `path` is the absolute path of `certificates.json`. The file
    /// does not need to exist; it'll be created on first save.
    explicit CertStore(QString path);

    [[nodiscard]] QString path() const { return m_path; }

    /// Re-read the store from disk. Returns false on parse error.
    /// Empty / nonexistent files are treated as "no certs" and return
    /// true with an empty list.
    bool load(QString *error = nullptr);

    /// Persist the current in-memory list.
    bool save(QString *error = nullptr);

    [[nodiscard]] const QVector<CertEntry> &entries() const { return m_entries; }

    /// Add a new entry. If an entry with the same id already exists,
    /// it is replaced. Returns the index of the (new or replaced) entry.
    int addOrReplace(CertEntry entry);

    /// Remove the entry with the given id. Returns true if anything
    /// was removed.
    bool removeById(const QString &id);

    /// Find by id. Returns -1 if not present.
    [[nodiscard]] int indexOf(const QString &id) const;

private:
    QString m_path;
    QVector<CertEntry> m_entries;
};

} // namespace meshcommander::certs
