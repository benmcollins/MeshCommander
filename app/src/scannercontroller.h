// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QVector>

namespace qumesh::model {
class ComputerModel;
}

namespace qumesh::rmcp {
class Scanner;
}

namespace qumesh::app {

/// One discovered host in the scan results table. Mutable `selected`
/// flag drives the per-row Add-to-fleet checkbox.
struct ScanResult
{
    QString ip;
    QString reverseDns;
    quint8 versionMajor = 0;
    quint8 versionMinor = 0;
    int provisioning = 0; ///< 0=pre, 1=in-process, 2=post.
    bool dualPorts = false;
    bool selected = true; ///< Pre-checked so "Add selected" works on first scan.
};

/// Backs the QML `ScanDialog`. Owns one `rmcp::Scanner` per scan and a
/// model of discovered hosts. The dialog instantiates a fresh
/// `ScannerController` each time it opens — there's no need to keep
/// scanner state across opens.
class ScannerController : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)
public:
    enum Role : int {
        IpRole = Qt::UserRole + 1,
        ReverseDnsRole,
        AmtVersionRole,
        ProvisioningRole,
        TlsCapableRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    explicit ScannerController(QObject *parent = nullptr);
    ~ScannerController() override;

    /// QML hands us the same `ComputerModel` singleton — call this once
    /// before `addSelected()`.
    Q_INVOKABLE void setComputerModel(qumesh::model::ComputerModel *model);

    [[nodiscard]] bool scanning() const { return m_scanning; }
    [[nodiscard]] QString status() const { return m_status; }

    /// Number of rows currently flagged `selected`. Drives the
    /// dialog's "Add N machines" button enable state.
    [[nodiscard]] int selectedCount() const;

    /// Parse `spec` (IP / range / CIDR), start the scan, clear prior
    /// results. Updates `status` with the live host count.
    Q_INVOKABLE void start(const QString &spec);

    /// Cancel an in-flight scan. `status` reflects the cancel.
    Q_INVOKABLE void stop();

    /// Toggle the per-row checkbox.
    Q_INVOKABLE void setSelected(int row, bool selected);

    /// Append one `Computer` per selected row to the bound
    /// `ComputerModel`. Names default to the reverse-DNS or the IP;
    /// TLS is preset from the dual-ports advertisement. Returns the
    /// number of computers actually added.
    Q_INVOKABLE int addSelected();

    // QAbstractListModel
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void scanningChanged();
    void statusChanged();
    void countChanged();
    void selectedCountChanged();

private:
    void setScanning(bool s);
    void setStatus(QString s);

    qumesh::rmcp::Scanner *m_scanner = nullptr;
    qumesh::model::ComputerModel *m_computers = nullptr;
    QVector<ScanResult> m_results;
    bool m_scanning = false;
    QString m_status;
};

} // namespace qumesh::app
