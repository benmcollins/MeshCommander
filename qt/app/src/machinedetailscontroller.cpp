// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "machinedetailscontroller.h"

#include "wsman/operations.h"
#include "wsman/wsman_client.h"

namespace qumesh::app {

MachineDetailsController::MachineDetailsController(QObject *parent)
    : QObject(parent), m_client(new qumesh::wsman::WsmanClient(this))
{
}

MachineDetailsController::~MachineDetailsController() = default;

void MachineDetailsController::setHost(const QString &v)
{
    if (v == m_host) return;
    m_host = v;
    emit hostChanged();
    rebuildEndpoint();
}

void MachineDetailsController::setUser(const QString &v)
{
    if (v == m_user) return;
    m_user = v;
    emit userChanged();
    m_client->setCredentials(m_user, m_password);
}

void MachineDetailsController::setPassword(const QString &v)
{
    if (v == m_password) return;
    m_password = v;
    emit passwordChanged();
    m_client->setCredentials(m_user, m_password);
}

void MachineDetailsController::setTls(bool v)
{
    if (v == m_tls) return;
    m_tls = v;
    emit tlsChanged();
    rebuildEndpoint();
}

QString MachineDetailsController::powerStateLabel() const
{
    switch (m_powerState) {
    case 0:  return QStringLiteral("Unknown");
    case 2:  return QStringLiteral("On");
    case 3:  return QStringLiteral("Standby (light)");
    case 4:  return QStringLiteral("Standby (deep)");
    case 6:  return QStringLiteral("Off (soft)");
    case 7:  return QStringLiteral("Hibernate");
    case 8:  return QStringLiteral("Off (hard)");
    case 9:  return QStringLiteral("Power cycle");
    case 13: return QStringLiteral("Standby");
    case 14: return QStringLiteral("Standby");
    default: return QStringLiteral("Power state %1").arg(m_powerState);
    }
}

void MachineDetailsController::rebuildEndpoint()
{
    if (m_host.isEmpty()) return;
    QUrl url;
    url.setScheme(m_tls ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_tls ? 16993 : 16992);
    url.setPath(QStringLiteral("/wsman"));
    m_client->setEndpoint(url);
    m_client->setCredentials(m_user, m_password);
}

void MachineDetailsController::incInflight()
{
    const bool was = busy();
    ++m_inflight;
    if (busy() != was) emit busyChanged();
}

void MachineDetailsController::decInflight()
{
    const bool was = busy();
    if (m_inflight > 0) --m_inflight;
    if (busy() != was) emit busyChanged();
}

void MachineDetailsController::setLastError(const QString &e)
{
    if (e == m_lastError) return;
    m_lastError = e;
    emit lastErrorChanged();
}

void MachineDetailsController::refreshOverview()
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    setLastError({});

    // identify — no credentials required.
    incInflight();
    qumesh::wsman::identify(m_client, [this](qumesh::wsman::IdentifyResult r) {
        if (r.ok) {
            m_amtProtocolVersion = r.protocolVersion;
            m_amtVendor          = r.productVendor;
            m_amtVersion         = r.productVersion;
            emit identifyChanged();
        } else if (!r.error.isEmpty()) {
            setLastError(QStringLiteral("Identify: %1").arg(r.error));
        }
        decInflight();
    });

    incInflight();
    qumesh::wsman::getPowerState(m_client, [this](qumesh::wsman::PowerStateResult r) {
        if (r.ok) {
            if (m_powerState != r.powerState) {
                m_powerState = r.powerState;
                emit powerStateChanged();
            }
        } else if (!r.error.isEmpty()) {
            setLastError(QStringLiteral("Power: %1").arg(r.error));
        }
        decInflight();
    });

    incInflight();
    qumesh::wsman::getGeneralSettings(m_client,
        [this](qumesh::wsman::GeneralSettingsResult r) {
            if (r.ok) {
                m_hostName     = r.hostName;
                m_domainName   = r.domainName;
                m_digestRealm  = r.digestRealm;
                m_networkInterfaceEnabled = r.networkInterfaceEnabled;
                m_rmcpPingResponseEnabled = r.rmcpPingResponseEnabled;
                emit generalSettingsChanged();
            } else if (!r.error.isEmpty()) {
                setLastError(QStringLiteral("GeneralSettings: %1").arg(r.error));
            }
            decInflight();
        });

    incInflight();
    qumesh::wsman::getComputerSystem(m_client,
        [this](qumesh::wsman::ComputerSystemResult r) {
            if (r.ok) {
                m_systemName        = r.name;
                m_systemElementName = r.elementName;
                m_systemUuid        = r.systemUuid;
                emit computerSystemChanged();
            } else if (!r.error.isEmpty()) {
                setLastError(QStringLiteral("ComputerSystem: %1").arg(r.error));
            }
            decInflight();
        });
}

void MachineDetailsController::refreshNetwork()
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getEthernetSettings(m_client,
        [this](qumesh::wsman::EthernetSettingsResult r) {
            if (r.ok) {
                m_macAddress     = r.macAddress;
                m_dhcpEnabled    = r.dhcpEnabled;
                m_ipAddress      = r.ipAddress;
                m_subnetMask     = r.subnetMask;
                m_defaultGateway = r.defaultGateway;
                m_primaryDns     = r.primaryDns;
                m_secondaryDns   = r.secondaryDns;
                emit ethernetChanged();
            } else if (!r.error.isEmpty()) {
                setLastError(QStringLiteral("Ethernet: %1").arg(r.error));
            }
            decInflight();
        });
}

void MachineDetailsController::refreshTime()
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getTimeSettings(m_client,
        [this](qumesh::wsman::TimeSettingsResult r) {
            if (r.ok) {
                m_amtEpoch = r.secondsSinceEpoch;
                emit timeChanged();
            } else if (!r.error.isEmpty()) {
                setLastError(QStringLiteral("Time: %1").arg(r.error));
            }
            decInflight();
        });
}

void MachineDetailsController::refreshPower()
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getPowerState(m_client, [this](qumesh::wsman::PowerStateResult r) {
        if (r.ok) {
            if (m_powerState != r.powerState) {
                m_powerState = r.powerState;
                emit powerStateChanged();
            }
        } else if (!r.error.isEmpty()) {
            setLastError(QStringLiteral("Power: %1").arg(r.error));
        }
        decInflight();
    });
}

void MachineDetailsController::changePowerState(int code)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(code, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(code);
    incInflight();
    qumesh::wsman::requestPowerStateChange(m_client, code,
        [this, code](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (!r.ok) {
                setLastError(QStringLiteral("Power change %1: %2")
                                  .arg(code).arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            // Give the firmware ~1 s to settle before re-reading the
            // current state. Keeping it synchronous (no QTimer) — the
            // user can hit Refresh if they want sooner.
            refreshPower();
        });
}

} // namespace qumesh::app
