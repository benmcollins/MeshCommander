// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "scannercontroller.h"

#include "computermodel.h"
#include "rmcp/scanner.h"
#include "rmcp/target_range.h"

#include <QHostAddress>

namespace qumesh::app {

namespace {

QString amtVersionString(const ScanResult &r)
{
    return QStringLiteral("%1.%2").arg(r.versionMajor).arg(r.versionMinor);
}

QString provisioningLabel(int prov)
{
    switch (prov) {
        case 0: return QStringLiteral("pre");
        case 1: return QStringLiteral("in-process");
        case 2: return QStringLiteral("post");
        default: return QStringLiteral("?");
    }
}

} // namespace

ScannerController::ScannerController(QObject *parent)
    : QAbstractListModel(parent)
{
    m_scanner = new qumesh::rmcp::Scanner(this);
    connect(m_scanner, &qumesh::rmcp::Scanner::pongReceived, this,
        [this](const qumesh::rmcp::PongMessage &pong, const QHostAddress &src,
               const QString &reverseDns) {
            ScanResult r;
            r.ip = src.toString();
            r.reverseDns = reverseDns;
            r.versionMajor = pong.versionMajor;
            r.versionMinor = pong.versionMinor;
            r.provisioning = static_cast<int>(pong.provisioning);
            r.dualPorts = pong.dualPorts;
            r.selected = true;
            beginInsertRows({}, m_results.size(), m_results.size());
            m_results.append(r);
            endInsertRows();
            emit countChanged();
            emit selectedCountChanged();
            setStatus(tr("Found %1 device(s)…").arg(m_results.size()));
        });
    connect(m_scanner, &qumesh::rmcp::Scanner::finished, this,
        [this]() {
            setScanning(false);
            if (m_results.isEmpty())
                setStatus(tr("Scan finished — no AMT devices responded."));
            else
                setStatus(tr("Scan finished — %1 device(s).").arg(m_results.size()));
        });
}

ScannerController::~ScannerController() = default;

void ScannerController::setComputerModel(qumesh::model::ComputerModel *model)
{
    m_computers = model;
}

int ScannerController::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_results.size();
}

QVariant ScannerController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size())
        return {};
    const auto &r = m_results[index.row()];
    switch (role) {
        case IpRole:           return r.ip;
        case ReverseDnsRole:   return r.reverseDns;
        case AmtVersionRole:   return amtVersionString(r);
        case ProvisioningRole: return provisioningLabel(r.provisioning);
        case TlsCapableRole:   return r.dualPorts;
        case SelectedRole:     return r.selected;
        default:               return {};
    }
}

bool ScannerController::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size())
        return false;
    if (role != SelectedRole)
        return false;
    const bool sel = value.toBool();
    if (m_results[index.row()].selected == sel)
        return true;
    m_results[index.row()].selected = sel;
    emit dataChanged(index, index, { SelectedRole });
    emit selectedCountChanged();
    return true;
}

QHash<int, QByteArray> ScannerController::roleNames() const
{
    return {
        { IpRole,           "ip"           },
        { ReverseDnsRole,   "reverseDns"   },
        { AmtVersionRole,   "amtVersion"   },
        { ProvisioningRole, "provisioning" },
        { TlsCapableRole,   "tlsCapable"   },
        { SelectedRole,     "selected"     },
    };
}

int ScannerController::selectedCount() const
{
    int n = 0;
    for (const auto &r : m_results)
        if (r.selected)
            ++n;
    return n;
}

void ScannerController::start(const QString &spec)
{
    if (m_scanning)
        return;
    QString err;
    const auto targets = qumesh::rmcp::parseTargets(spec, &err);
    if (targets.isEmpty()) {
        setStatus(err.isEmpty() ? tr("Invalid target.") : err);
        return;
    }

    if (!m_results.isEmpty()) {
        beginResetModel();
        m_results.clear();
        endResetModel();
        emit countChanged();
        emit selectedCountChanged();
    }

    if (!m_scanner->start(targets)) {
        setStatus(tr("Failed to start scan: %1").arg(m_scanner->errorString()));
        return;
    }
    setScanning(true);
    setStatus(tr("Scanning %1 address(es)…").arg(targets.size()));
}

void ScannerController::stop()
{
    if (!m_scanning)
        return;
    m_scanner->stop();
    // scanner::stop synchronously emits finished; setScanning(false) and
    // status were already updated by the lambda above. Override with a
    // user-facing "cancelled" so the result line doesn't read as a
    // natural scan completion.
    setStatus(tr("Scan cancelled."));
}

void ScannerController::setSelected(int row, bool sel)
{
    setData(index(row, 0), sel, SelectedRole);
}

int ScannerController::addSelected()
{
    if (!m_computers)
        return 0;
    int added = 0;
    for (const auto &r : m_results) {
        if (!r.selected)
            continue;
        const QString name = r.reverseDns.isEmpty() ? r.ip : r.reverseDns;
        const QString host = r.reverseDns.isEmpty() ? r.ip : r.reverseDns;
        if (m_computers->addComputer(name, host, /*user*/ {}, /*pass*/ {},
                                     /*tls*/ r.dualPorts) >= 0) {
            ++added;
        }
    }
    return added;
}

void ScannerController::setScanning(bool s)
{
    if (m_scanning == s)
        return;
    m_scanning = s;
    emit scanningChanged();
}

void ScannerController::setStatus(QString s)
{
    if (m_status == s)
        return;
    m_status = std::move(s);
    emit statusChanged();
}

} // namespace qumesh::app
