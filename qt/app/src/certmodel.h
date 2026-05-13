// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QAbstractListModel>
#include <QString>

#include "certs/cert_store.h"

namespace qumesh::app {

/// QAbstractListModel exposing the local certificate store to QML.
/// Backed by a `qumesh::certs::CertStore` loaded from
/// `<configDir>/certificates.json`.
class CertModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role : int {
        IdRole = Qt::UserRole + 1,
        SubjectRole,
        IssuerRole,
        SerialRole,
        NotBeforeRole,
        NotAfterRole,
        FingerprintRole,
        HasPrivateKeyRole,
    };
    Q_ENUM(Role)

    explicit CertModel(QObject *parent = nullptr);

    /// Point the model at a store file (absolute path) and reload.
    void setStorePath(const QString &path);

    [[nodiscard]] QString lastError() const { return m_lastError; }

    // QAbstractListModel
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// Import any of: .cer (DER), .pem, .p12 (PKCS#12). `password` is
    /// only used for .p12; ignored otherwise. Returns the new row index
    /// or -1 on failure (see lastError()).
    Q_INVOKABLE int importFromFile(const QString &path, const QString &password = {});

    /// Remove the row at `row` and persist. Returns false on failure.
    Q_INVOKABLE bool removeAt(int row);

    /// Export the row at `row` as either DER (.cer) or PEM. Returns
    /// false (and sets lastError) if the row is out of range or the
    /// file can't be written.
    Q_INVOKABLE bool exportAsDer(int row, const QString &path);
    Q_INVOKABLE bool exportAsPem(int row, const QString &path);

private:
    void reload();

    certs::CertStore m_store;
    QString m_lastError;
};

} // namespace qumesh::app
