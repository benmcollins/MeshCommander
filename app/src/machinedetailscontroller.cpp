// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "machinedetailscontroller.h"

#include "wsman/operations.h"
#include "wsman/wsman_client.h"

#include "ssh/ssh_session.h"
#include "ssh/ssh_tunnel.h"
#include "ssh_tunnel_opener.h"

namespace qumesh::app {

MachineDetailsController::MachineDetailsController(QObject *parent)
    : QObject(parent), m_client(new qumesh::wsman::WsmanClient(this))
{
    connect(m_client, &qumesh::wsman::WsmanClient::trustPromptRequired,
            this, &MachineDetailsController::onTrustPromptRequired);
    connect(m_client, &qumesh::wsman::WsmanClient::peerCertVerifiedByPin,
            this, &MachineDetailsController::peerCertVerifiedByPin);
}

MachineDetailsController::~MachineDetailsController() = default;

bool MachineDetailsController::sshTunnelActive() const
{
    return m_sshEnabled
        && m_sshSession != nullptr
        && m_sshSession->state() == qumesh::ssh::SshSession::Connected;
}

void MachineDetailsController::setSshConfig(const QVariantMap &cfg)
{
    const bool enabled = cfg.value(QStringLiteral("enabled")).toBool();
    if (!enabled) {
        m_sshEnabled = false;
        if (m_sshSession != nullptr) {
            m_sshSession->close();
            m_sshSession->deleteLater();
            m_sshSession = nullptr;
        }
        m_client->setSocketFactory({});
        m_client->setSerializeRequests(false);
        m_sshTunnelStatus.clear();
        emit sshTunnelStateChanged();
        return;
    }

    m_sshEnabled = true;
    m_sshConnecting = true;
    // AMT typically only accepts ~2 concurrent HTTPS WSMAN sessions;
    // over an SSH tunnel the extras silently stall instead of failing
    // fast, so let the WsmanClient single-flight requests while the
    // tunnel is active.
    m_client->setSerializeRequests(true);
    if (m_sshSession == nullptr) {
        m_sshSession = new qumesh::ssh::SshSession(this);
        connect(m_sshSession, &qumesh::ssh::SshSession::stateChanged, this,
                [this](qumesh::ssh::SshSession::State s) {
                    switch (s) {
                    case qumesh::ssh::SshSession::Connecting:
                        m_sshTunnelStatus = QStringLiteral("Connecting…"); break;
                    case qumesh::ssh::SshSession::Authenticating:
                        m_sshTunnelStatus = QStringLiteral("Authenticating…"); break;
                    case qumesh::ssh::SshSession::NeedsHostKeyTrust:
                        m_sshTunnelStatus = QStringLiteral("Awaiting host-key trust"); break;
                    case qumesh::ssh::SshSession::Connected:
                        m_sshTunnelStatus = QStringLiteral("via SSH");
                        m_sshConnecting = false;
                        runPendingRefreshes();
                        break;
                    case qumesh::ssh::SshSession::Failed:
                        m_sshTunnelStatus = QStringLiteral("SSH failed: %1")
                                                .arg(m_sshSession->lastError());
                        m_sshConnecting = false;
                        m_pendingRefreshes = 0;
                        break;
                    case qumesh::ssh::SshSession::Disconnected:
                        m_sshTunnelStatus.clear();
                        m_sshConnecting = false;
                        m_pendingRefreshes = 0;
                        break;
                    }
                    emit sshTunnelStateChanged();
                });
        connect(m_sshSession, &qumesh::ssh::SshSession::hostKeyPromptRequired, this,
                [this](const QString &fp, const QString &keyType) {
                    m_pendingSshHostKey = fp;
                    m_pendingSshHostKeyType = keyType;
                    m_awaitingSshHostKeyTrust = true;
                    // Auto-pin on first connect (TOFU). The
                    // fingerprint is then persisted into ComputerModel
                    // via `trustedSshHostKeyAdded`, so subsequent
                    // connects must match — protecting against
                    // post-first-connect MITM / key rotation.
                    emit sshHostKeyPromptRequired(fp, keyType);
                    trustPendingSshHostKey(true);
                });
    }

    qumesh::ssh::SshSession::Params p;
    p.host = cfg.value(QStringLiteral("host")).toString();
    p.port = static_cast<quint16>(cfg.value(QStringLiteral("port"), 22).toInt());
    p.user = cfg.value(QStringLiteral("user")).toString();
    p.authMode = cfg.value(QStringLiteral("authMode")).toInt()
                     == int(qumesh::ssh::SshSession::AuthKey)
                     ? qumesh::ssh::SshSession::AuthKey
                     : qumesh::ssh::SshSession::AuthPassword;
    p.password = cfg.value(QStringLiteral("password")).toString();
    p.privateKeyPath = cfg.value(QStringLiteral("keyPath")).toString();
    p.privateKeyPassphrase = cfg.value(QStringLiteral("keyPassphrase")).toString();
    const QVariantList fps = cfg.value(QStringLiteral("trustedHostKeyFingerprints"))
                                  .toList();
    for (const QVariant &v : fps) p.trustedHostKeyFingerprints.append(v.toString());

    // Each WSMAN request opens a fresh SshTunnel; the channel is short-
    // lived (the WsmanClient closes the socket once the response body
    // is consumed). Channel-create cost over an already-authenticated
    // SSH session is ~1 ms, well below user-perceptible.
    m_client->setSocketFactory(makeSshSocketFactory(
        m_sshSession, m_host,
        static_cast<quint16>(m_tls ? 16993 : 16992)));

    m_sshSession->open(p);
    emit sshTunnelStateChanged();
}

void MachineDetailsController::trustPendingSshHostKey(bool persist)
{
    if (!m_awaitingSshHostKeyTrust) return;
    const QString fp = m_pendingSshHostKey;
    m_pendingSshHostKey.clear();
    m_pendingSshHostKeyType.clear();
    m_awaitingSshHostKeyTrust = false;
    if (m_sshSession != nullptr) m_sshSession->trustPendingHostKey();
    if (persist) emit trustedSshHostKeyAdded(fp);
}

bool MachineDetailsController::deferIfSshConnecting(PendingRefresh kind)
{
    if (!m_sshConnecting) return false;
    m_pendingRefreshes |= kind;
    return true;
}

void MachineDetailsController::runPendingRefreshes()
{
    const int p = m_pendingRefreshes;
    m_pendingRefreshes = 0;
    if (p & PendingOverview)     refreshOverview();
    if (p & PendingPower)        refreshPower();
    if (p & PendingNetwork)      refreshNetwork();
    if (p & PendingTime)         refreshTime();
    if (p & PendingEventLog)     refreshEventLog();
    if (p & PendingUserAccounts) refreshUserAccounts();
}

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

void MachineDetailsController::setTrustedFingerprints(QStringList v)
{
    if (v == m_trustedFingerprints) return;
    m_trustedFingerprints = std::move(v);
    m_client->setTrustedFingerprints(m_trustedFingerprints);
    emit trustedFingerprintsChanged();
}

QString MachineDetailsController::powerSourceLabel() const
{
    switch (m_powerSource) {
    case 0:  return QStringLiteral("Plugged-in");
    case 1:  return QStringLiteral("On battery");
    default: return QStringLiteral("(unknown)");
    }
}

QString MachineDetailsController::provisioningModeLabel() const
{
    // Mirrors `legacy/source/Commander.htm` line ~45371. Mode 4 = CCM,
    // anything else (typically 1/2/3) at state 2 = ACM.
    if (m_provisioningState != 2) return QStringLiteral("Pre-provisioning");
    return m_provisioningMode == 4
        ? QStringLiteral("Activated in CCM")
        : QStringLiteral("Activated in ACM");
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
    m_client->setTrustedFingerprints(m_trustedFingerprints);
}

void MachineDetailsController::onTrustPromptRequired(
        const qumesh::wsman::PeerCertSummary &s)
{
    m_pendingCert = s;
    if (!m_awaitingTrust) {
        m_awaitingTrust = true;
        emit awaitingTrustChanged();
    }
    emit pendingCertChanged();
    // Suppress the SSL-handshake error message from the failed reply
    // — the trust dialog is more informative than "SSL handshake
    // failed: hostname mismatch".
    if (!m_lastError.isEmpty()) {
        m_lastError.clear();
        emit lastErrorChanged();
    }
}

void MachineDetailsController::close()
{
    if (m_awaitingTrust) {
        m_pendingCert = {};
        m_awaitingTrust = false;
        emit awaitingTrustChanged();
        emit pendingCertChanged();
    }
    emit closeRequested();
}

void MachineDetailsController::trustPendingCert(bool persist)
{
    if (!m_awaitingTrust) return;
    const QString fp = m_pendingCert.fingerprintSha256;
    m_client->trustPendingPeerCert();
    if (!m_trustedFingerprints.contains(fp)) {
        m_trustedFingerprints.append(fp);
        emit trustedFingerprintsChanged();
    }
    m_pendingCert = {};
    m_awaitingTrust = false;
    emit awaitingTrustChanged();
    emit pendingCertChanged();
    if (persist && !fp.isEmpty()) emit trustedFingerprintAdded(fp);

    // Resume whatever fetch was in flight when the prompt fired. If we
    // weren't tracking one specifically, restart the overview — that's
    // the section we open on first connect.
    switch (m_pendingOp) {
    case PendingOp::Overview: m_pendingOp = PendingOp::None; refreshOverview(); break;
    case PendingOp::Power:    m_pendingOp = PendingOp::None; refreshPower();    break;
    case PendingOp::Network:  m_pendingOp = PendingOp::None; refreshNetwork();  break;
    case PendingOp::Time:     m_pendingOp = PendingOp::None; refreshTime();     break;
    case PendingOp::None:     refreshOverview(); break;
    }
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
    // While we're prompting for trust the failed reply's "SSL handshake
    // failed" text is noisier than the dialog itself; drop it.
    QString filtered = e;
    if (m_awaitingTrust && filtered.contains(QStringLiteral("SSL")))
        filtered.clear();
    if (filtered == m_lastError) return;
    m_lastError = filtered;
    emit lastErrorChanged();
}

void MachineDetailsController::refreshOverview()
{
    if (deferIfSshConnecting(PendingOverview)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    setLastError({});
    m_pendingOp = PendingOp::Overview;

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
                m_powerSource  = r.powerSource;
                emit generalSettingsChanged();
            } else if (!r.error.isEmpty()) {
                setLastError(QStringLiteral("GeneralSettings: %1").arg(r.error));
            }
            decInflight();
        });

    incInflight();
    qumesh::wsman::getMeVersion(m_client,
        [this](qumesh::wsman::MeVersionResult r) {
            if (r.ok) {
                m_meVersionString = r.versionString;
                emit meVersionChanged();
            }
            // Soft failure: older firmware may not enumerate
            // CIM_SoftwareIdentity. Leave the previous value in place.
            decInflight();
        });

    incInflight();
    qumesh::wsman::getSetupAndConfiguration(m_client,
        [this](qumesh::wsman::SetupAndConfigResult r) {
            if (r.ok) {
                m_provisioningState = r.provisioningState;
                m_provisioningMode  = r.provisioningMode;
                emit setupConfigChanged();
            }
            // Soft failure: pre-AMT-5 firmware doesn't expose the
            // service. Leave provisioning labels at "Pre-provisioning".
            decInflight();
        });

    incInflight();
    qumesh::wsman::getRedirectionStatus(m_client,
        [this](qumesh::wsman::RedirectionStatusResult r) {
            if (r.ok) {
                m_redirectionListenerEnabled = r.redirectionListenerEnabled;
                m_solEnabled    = r.solEnabled;
                m_iderEnabled   = r.iderEnabled;
                m_kvmEnabled    = r.kvmEnabled;
                m_kvmAvailable  = r.kvmAvailable;
                emit redirectionStatusChanged();
            } else if (!r.error.isEmpty()) {
                setLastError(QStringLiteral("Redirection: %1").arg(r.error));
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

    incInflight();
    qumesh::wsman::getBootCapabilities(m_client,
        [this](qumesh::wsman::BootCapabilitiesResult r) {
            if (r.ok) {
                m_capBiosSetup          = r.biosSetup;
                m_capBiosPause          = r.biosPause;
                m_capSecureErase        = r.secureErase;
                m_capPlatformErase      = r.platformErase;
                m_capPlatformEraseMask  = r.platformEraseMask;
                m_capForceUefiHttpsBoot = r.forceUefiHttpsBoot;
                emit bootCapabilitiesChanged();
            } else if (!r.error.isEmpty()) {
                // Capabilities are advisory — log but don't blame the
                // user. Older firmware may not expose this resource.
                setLastError(QStringLiteral("BootCapabilities: %1").arg(r.error));
            }
            decInflight();
        });

    refreshOptInStatus();
}

void MachineDetailsController::refreshOptInStatus()
{
    if (deferIfSshConnecting(PendingOverview)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) return;
    incInflight();
    qumesh::wsman::getOptInStatus(m_client,
        [this](qumesh::wsman::OptInServiceResult r) {
            if (r.ok) {
                m_optInRequired         = r.optInRequired;
                m_optInState            = r.optInState;
                m_canModifyOptInPolicy  = r.canModifyOptInPolicy;
                m_kvmOptInPolicy        = r.kvmOptInPolicy;
                emit optInStatusChanged();
            }
            // Soft failure: older AMT firmware doesn't expose these
            // classes. Leave the previous values in place and let the
            // UI default to "OptIn: not detected" or similar.
            decInflight();
        });
}

void MachineDetailsController::setKvmOptInPolicyEnabled(bool enabled)
{
    setLastError({});
    incInflight();
    qumesh::wsman::setKvmOptInPolicy(m_client, enabled,
        [this, enabled](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (r.ok) {
                m_kvmOptInPolicy = enabled;
                emit optInStatusChanged();
                // Re-read the runtime fields too — disabling the
                // policy flips `OptInRequired` back to false.
                refreshOptInStatus();
            } else {
                emit optInPolicyChangeFailed(r.error);
            }
        });
}

void MachineDetailsController::startOptIn()
{
    incInflight();
    qumesh::wsman::startOptIn(m_client,
        [this](qumesh::wsman::InvokeResult r) {
            decInflight();
            emit optInStarted(r.ok, r.error);
            refreshOptInStatus();
        });
}

void MachineDetailsController::sendOptInCode(int code)
{
    incInflight();
    qumesh::wsman::sendOptInCode(m_client, static_cast<quint32>(code),
        [this](qumesh::wsman::InvokeResult r) {
            decInflight();
            emit optInCodeResult(r.ok, r.error);
            refreshOptInStatus();
        });
}

void MachineDetailsController::cancelOptIn()
{
    incInflight();
    qumesh::wsman::cancelOptIn(m_client,
        [this](qumesh::wsman::InvokeResult) {
            decInflight();
            refreshOptInStatus();
        });
}


void MachineDetailsController::refreshNetwork()
{
    if (deferIfSshConnecting(PendingNetwork)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    m_pendingOp = PendingOp::Network;
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
    if (deferIfSshConnecting(PendingTime)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    m_pendingOp = PendingOp::Time;
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

void MachineDetailsController::refreshEventLog()
{
    if (deferIfSshConnecting(PendingEventLog)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::enumerateEventLog(m_client,
        [this](qumesh::wsman::EventLogResult r) {
            decInflight();
            if (!r.ok) {
                if (!r.error.isEmpty())
                    setLastError(QStringLiteral("Event log: %1").arg(r.error));
                return;
            }
            QVariantList list;
            list.reserve(r.entries.size());
            for (const auto &e : r.entries) {
                QVariantMap m;
                m.insert(QStringLiteral("recordId"),  e.recordId);
                m.insert(QStringLiteral("timestamp"), e.timestamp);
                m.insert(QStringLiteral("severity"),  e.severity);
                m.insert(QStringLiteral("message"),   e.message);
                list.append(m);
            }
            m_eventLog = std::move(list);

            emit eventLogChanged();
        });
}

void MachineDetailsController::refreshUserAccounts()
{
    if (deferIfSshConnecting(PendingUserAccounts)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::enumerateUserAccounts(m_client,
        [this](qumesh::wsman::UserAccountsResult r) {
            decInflight();
            if (!r.ok) {
                if (!r.error.isEmpty())
                    setLastError(QStringLiteral("User accounts: %1").arg(r.error));
                return;
            }
            QVariantList list;
            list.reserve(r.accounts.size());
            for (const auto &u : r.accounts) {
                QVariantMap m;
                m.insert(QStringLiteral("instanceID"),  u.instanceID);
                m.insert(QStringLiteral("name"),        u.name);
                m.insert(QStringLiteral("elementName"), u.elementName);
                m.insert(QStringLiteral("enabled"),     u.enabled);
                list.append(m);
            }

            m_userAccounts = std::move(list);
            emit userAccountsChanged();
        });
}

void MachineDetailsController::refreshPower()
{
    if (deferIfSshConnecting(PendingPower)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    m_pendingOp = PendingOp::Power;
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

void MachineDetailsController::bootToBios(bool reset)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(reset ? 10 : 2);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = reset ? 10 : 2;
    p.biosSetup = true;
    qumesh::wsman::performBootAction(m_client, p,
        [this, reset](qumesh::wsman::InvokeResult r) {
            decInflight();
            const int code = reset ? 10 : 2;
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to BIOS: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::bootToPxe(bool reset)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(reset ? 10 : 2);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = reset ? 10 : 2;
    p.amtBootSource = QStringLiteral("Force PXE Boot");
    qumesh::wsman::performBootAction(m_client, p,
        [this, reset](qumesh::wsman::InvokeResult r) {
            decInflight();
            const int code = reset ? 10 : 2;
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to PXE: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::bootToIderCdrom(bool reset)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(reset ? 10 : 2);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = reset ? 10 : 2;
    p.amtBootSource = QStringLiteral("Force CD/DVD Boot");
    p.useIder = true;
    p.iderBootDevice = 1; // CD-ROM
    qumesh::wsman::performBootAction(m_client, p,
        [this, reset](qumesh::wsman::InvokeResult r) {
            decInflight();
            const int code = reset ? 10 : 2;
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to IDE-R CDROM: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::bootToIderFloppy(bool reset)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(reset ? 10 : 2);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = reset ? 10 : 2;
    p.amtBootSource = QStringLiteral("Force CD/DVD Boot");
    p.useIder = true;
    p.iderBootDevice = 0; // Floppy
    qumesh::wsman::performBootAction(m_client, p,
        [this, reset](qumesh::wsman::InvokeResult r) {
            decInflight();
            const int code = reset ? 10 : 2;
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to IDE-R Floppy: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
        });
}

int MachineDetailsController::amtVersionMajor() const
{
    // amtVersion looks like "11.8.50" / "16.1.25.0" / "5.2.0". Parse
    // the leading integer; return -1 when we haven't fetched it yet.
    if (m_amtVersion.isEmpty()) return -1;
    const int dot = m_amtVersion.indexOf(QLatin1Char('.'));
    const QString head = dot < 0 ? m_amtVersion : m_amtVersion.left(dot);
    bool ok = false;
    const int n = head.toInt(&ok);
    return ok ? n : -1;
}

void MachineDetailsController::osWakeFromSleep()
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(2);
    incInflight();
    qumesh::wsman::requestOsPowerStateChange(m_client, 2,
        [this](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (!r.ok) {
                setLastError(QStringLiteral("OS wake: %1").arg(r.error));
                emit powerChangeCompleted(2, false, r.error);
                return;
            }
            emit powerChangeCompleted(2, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::osPutToSleep()
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    emit powerChangeRequested(3);
    incInflight();
    qumesh::wsman::requestOsPowerStateChange(m_client, 3,
        [this](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (!r.ok) {
                setLastError(QStringLiteral("OS sleep: %1").arg(r.error));
                emit powerChangeCompleted(3, false, r.error);
                return;
            }
            emit powerChangeCompleted(3, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::bootToSecureErase(bool reset, const QString &password)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    setLastError({});
    const int code = reset ? 10 : 2;
    emit powerChangeRequested(code);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = code;
    p.biosSetup = true;
    p.secureErase = true;
    p.rsePassword = password;
    qumesh::wsman::performBootAction(m_client, p,
        [this, code](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to Secure Erase: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::bootToPlatformErase(bool reset, int flagsIn,
                                                    const QString &psid,
                                                    const QString &ssdPassword)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    if (flagsIn == 0) {
        setLastError(QStringLiteral("Platform Erase: no sub-actions selected"));
        return;
    }
    setLastError({});
    const int code = reset ? 10 : 2;
    emit powerChangeRequested(code);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = code;
    p.platformErase = true;
    int tlvCount = 0;
    const QByteArray tlv = qumesh::wsman::buildPlatformEraseTlv(
        static_cast<quint32>(flagsIn), psid, ssdPassword, &tlvCount);
    p.platformEraseTlvBase64 = QString::fromLatin1(tlv.toBase64());
    p.platformEraseTlvCount = tlvCount;
    qumesh::wsman::performBootAction(m_client, p,
        [this, code](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to Platform Erase: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
        });
}

void MachineDetailsController::bootToHttpsBootUrl(bool reset, const QString &url)
{
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        emit powerChangeCompleted(0, false, QStringLiteral("Host is empty"));
        return;
    }
    if (!url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        setLastError(QStringLiteral("HTTPS Boot: URL must start with https://"));
        return;
    }
    setLastError({});
    const int code = reset ? 10 : 2;
    emit powerChangeRequested(code);
    incInflight();
    qumesh::wsman::BootActionParams p;
    p.targetPowerState = code;
    p.amtBootSource = QStringLiteral("Force OCR UEFI HTTPS Boot");
    p.httpsBootUrl = true;
    p.httpsBootUrlStr = url;
    qumesh::wsman::performBootAction(m_client, p,
        [this, code](qumesh::wsman::InvokeResult r) {
            decInflight();
            if (!r.ok) {
                setLastError(QStringLiteral("Boot to HTTPS URL: %1").arg(r.error));
                emit powerChangeCompleted(code, false, r.error);
                return;
            }
            emit powerChangeCompleted(code, true, QString());
            refreshPower();
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
