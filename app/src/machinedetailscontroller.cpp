// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#include "machinedetailscontroller.h"

#include <QDateTime>

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
    if (p & PendingHardware)     refreshHardware();
    if (p & PendingAuditLog)     refreshAuditLog();
    if (p & PendingDeviceCerts)  refreshDeviceCerts();
    if (p & PendingRemoteAccess) refreshRemoteAccess();
    if (p & PendingWireless)     refreshWireless();
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

                // Build the fingerprint snapshot (#174). The Sku field
                // is a numeric bitmask string when present; decode the
                // single bit that distinguishes "Full AMT" from "ISM"
                // so the UI can render a friendly label without needing
                // to know the encoding.
                QVariantMap fp;
                fp.insert(QStringLiteral("amtVersion"),      r.versionString);
                fp.insert(QStringLiteral("buildNumber"),     r.buildNumber);
                fp.insert(QStringLiteral("recoveryVersion"), r.recoveryVersion);
                fp.insert(QStringLiteral("sku"),             r.sku);
                fp.insert(QStringLiteral("vendorId"),        r.vendorId);
                fp.insert(QStringLiteral("flash"),           r.flash);

                // SKU bitmask: bit 14 = ISM ("Intel Standard Manageability"),
                // bit 15 = AMT (full vPro). When both are clear we have
                // pre-provisioning / unprovisioned firmware. Older
                // firmware reports the field as a plain string we can't
                // decode — fall through to "(SKU code: X)" then.
                bool numeric = false;
                const quint64 skuMask = r.sku.toULongLong(&numeric);
                QString skuLabel;
                if (numeric) {
                    constexpr quint64 kAmtBit = 1ULL << 15;
                    constexpr quint64 kIsmBit = 1ULL << 14;
                    if (skuMask & kAmtBit)      skuLabel = tr("Full AMT (vPro)");
                    else if (skuMask & kIsmBit) skuLabel = tr("Intel Standard Manageability (ISM)");
                    else                        skuLabel = tr("Pre-provisioning / unknown SKU");
                } else if (!r.sku.isEmpty()) {
                    skuLabel = tr("SKU code: %1").arg(r.sku);
                }
                fp.insert(QStringLiteral("skuLabel"), skuLabel);

                m_amtFingerprint = fp;
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

                // Keyed snapshot for the read-only Boot Capabilities
                // pane (#172). The QML iterates the map directly so the
                // ordering is preserved by QVariantMap's sort. Labels
                // are derived in QML — keep keys as the raw AMT names.
                QVariantMap bc;
                bc.insert(QStringLiteral("IDER"),                   r.ider);
                bc.insert(QStringLiteral("SOL"),                    r.sol);
                bc.insert(QStringLiteral("BIOSReflash"),            r.biosReflash);
                bc.insert(QStringLiteral("BIOSSetup"),              r.biosSetup);
                bc.insert(QStringLiteral("BIOSPause"),              r.biosPause);
                bc.insert(QStringLiteral("ForcePXEBoot"),           r.forcePxeBoot);
                bc.insert(QStringLiteral("ForceHDDBoot"),           r.forceHddBoot);
                bc.insert(QStringLiteral("ForceCDorDVDBoot"),       r.forceCdOrDvdBoot);
                bc.insert(QStringLiteral("VerbosityScreenBlank"),   r.verbosityScreenBlank);
                bc.insert(QStringLiteral("PowerButtonLock"),        r.powerButtonLock);
                bc.insert(QStringLiteral("ResetButtonLock"),        r.resetButtonLock);
                bc.insert(QStringLiteral("KeyboardLock"),           r.keyboardLock);
                bc.insert(QStringLiteral("SleepButtonLock"),        r.sleepButtonLock);
                bc.insert(QStringLiteral("UserPasswordBypass"),     r.userPasswordBypass);
                bc.insert(QStringLiteral("ForcedProgressEvents"),   r.forcedProgressEvents);
                bc.insert(QStringLiteral("VerbosityVerbose"),       r.verbosityVerbose);
                bc.insert(QStringLiteral("VerbosityQuiet"),         r.verbosityQuiet);
                bc.insert(QStringLiteral("ConfigurationDataReset"), r.configurationDataReset);
                bc.insert(QStringLiteral("BIOSSecureBoot"),         r.biosSecureBoot);
                bc.insert(QStringLiteral("SecureErase"),            r.secureErase);
                bc.insert(QStringLiteral("ForceWinREBoot"),         r.forceWinReBoot);
                bc.insert(QStringLiteral("ForceUEFILocalPBABoot"),  r.forceUefiLocalPbaBoot);
                bc.insert(QStringLiteral("ForceUEFIHTTPSBoot"),     r.forceUefiHttpsBoot);
                bc.insert(QStringLiteral("AMTSecureBootControl"),   r.amtSecureBootControl);
                bc.insert(QStringLiteral("PlatformErase"),          r.platformErase);
                bc.insert(QStringLiteral("PlatformEraseMask"),      static_cast<int>(r.platformEraseMask));
                m_bootCapabilities = bc;

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

                QVariantList ifaces;
                ifaces.reserve(r.interfaces.size());
                for (const auto &i : r.interfaces) {
                    QVariantMap m;
                    m.insert(QStringLiteral("instanceId"),     i.instanceId);
                    m.insert(QStringLiteral("macAddress"),     i.macAddress);
                    m.insert(QStringLiteral("dhcpEnabled"),    i.dhcpEnabled);
                    m.insert(QStringLiteral("ipSyncEnabled"),  i.ipSyncEnabled);
                    m.insert(QStringLiteral("ipAddress"),      i.ipAddress);
                    m.insert(QStringLiteral("subnetMask"),     i.subnetMask);
                    m.insert(QStringLiteral("defaultGateway"), i.defaultGateway);
                    m.insert(QStringLiteral("primaryDns"),     i.primaryDns);
                    m.insert(QStringLiteral("secondaryDns"),   i.secondaryDns);

                    QVariantList lp;
                    QStringList lpLabels;
                    for (int c : i.linkPolicy) {
                        lp.append(c);
                        lpLabels.append(qumesh::wsman::linkPolicyLabel(c));
                    }
                    m.insert(QStringLiteral("linkPolicy"),      lp);
                    m.insert(QStringLiteral("linkPolicyLabel"), lpLabels.join(", "));

                    QVariantMap v6;
                    v6.insert(QStringLiteral("present"), i.ipv6.present);
                    QVariantList addrs;
                    for (const QString &a : i.ipv6.addresses) addrs.append(a);
                    v6.insert(QStringLiteral("addresses"),     addrs);
                    v6.insert(QStringLiteral("addressesLabel"), i.ipv6.addresses.join(", "));
                    v6.insert(QStringLiteral("defaultRouter"), i.ipv6.defaultRouter);
                    v6.insert(QStringLiteral("primaryDns"),    i.ipv6.primaryDns);
                    v6.insert(QStringLiteral("secondaryDns"),  i.ipv6.secondaryDns);
                    m.insert(QStringLiteral("ipv6"), v6);

                    ifaces.append(m);
                }
                m_networkInterfaces = std::move(ifaces);

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

void MachineDetailsController::syncDeviceTime()
{
    if (deferIfSshConnecting(PendingTime)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot sync."));
        return;
    }
    incInflight();
    // The 3-point exchange: read Ta0 from the device, record the host
    // clock at receive (Tm1) and send (Tm2 — same instant in this
    // simple flow), then push them back. The firmware uses the gap to
    // compute drift and corrects. After success, re-read to update the
    // skew display.
    qumesh::wsman::getTimeSettings(m_client,
        [this](qumesh::wsman::TimeSettingsResult r) {
            if (!r.ok) {
                if (!r.error.isEmpty())
                    setLastError(QStringLiteral("Time sync (read): %1").arg(r.error));
                decInflight();
                return;
            }
            const qint64 ta0 = r.secondsSinceEpoch;
            const qint64 tmHost = QDateTime::currentSecsSinceEpoch();
            qumesh::wsman::setHighAccuracyTimeSync(m_client, ta0, tmHost, tmHost,
                [this](qumesh::wsman::InvokeResult inv) {
                    if (!inv.ok && !inv.error.isEmpty()) {
                        setLastError(QStringLiteral("Time sync (set): %1").arg(inv.error));
                        decInflight();
                        return;
                    }
                    // Re-read after sync so the QML skew row falls to ~0.
                    qumesh::wsman::getTimeSettings(m_client,
                        [this](qumesh::wsman::TimeSettingsResult rr) {
                            if (rr.ok) {
                                m_amtEpoch = rr.secondsSinceEpoch;
                                emit timeChanged();
                            }
                            decInflight();
                        });
                });
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

void MachineDetailsController::refreshHardware()
{
    if (deferIfSshConnecting(PendingHardware)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getHardwareInventory(m_client,
        [this](qumesh::wsman::HardwareInventoryResult r) {
            decInflight();
            if (!r.ok) {
                if (!r.error.isEmpty())
                    setLastError(QStringLiteral("Hardware: %1").arg(r.error));
                return;
            }
            QVariantMap inv;
            inv.insert(QStringLiteral("platformModel"),        r.platformModel);
            inv.insert(QStringLiteral("platformManufacturer"), r.platformManufacturer);
            inv.insert(QStringLiteral("platformVersion"),      r.platformVersion);
            inv.insert(QStringLiteral("platformSerialNumber"), r.platformSerialNumber);
            inv.insert(QStringLiteral("platformSystemId"),     r.platformSystemId);

            inv.insert(QStringLiteral("baseboardManufacturer"), r.baseboardManufacturer);
            inv.insert(QStringLiteral("baseboardModel"),        r.baseboardModel);
            inv.insert(QStringLiteral("baseboardVersion"),      r.baseboardVersion);
            inv.insert(QStringLiteral("baseboardSerialNumber"), r.baseboardSerialNumber);
            inv.insert(QStringLiteral("baseboardAssetTag"),     r.baseboardAssetTag);
            inv.insert(QStringLiteral("baseboardReplaceable"),  r.baseboardReplaceable);
            inv.insert(QStringLiteral("baseboardCanBeFRUedKnown"),
                                                      r.baseboardCanBeFRUedKnown);

            inv.insert(QStringLiteral("biosVendor"),      r.biosVendor);
            inv.insert(QStringLiteral("biosVersion"),     r.biosVersion);
            inv.insert(QStringLiteral("biosReleaseDate"), r.biosReleaseDate);

            QVariantList cpus;
            for (const auto &c : r.processors) {
                QVariantMap m;
                m.insert(QStringLiteral("manufacturer"), c.manufacturer);
                m.insert(QStringLiteral("version"),      c.version);
                m.insert(QStringLiteral("family"),       c.family);
                m.insert(QStringLiteral("familyLabel"),  c.familyLabel);
                m.insert(QStringLiteral("maxClockSpeedMhz"), c.maxClockSpeedMhz);
                m.insert(QStringLiteral("cpuStatus"),      c.cpuStatus);
                m.insert(QStringLiteral("cpuStatusLabel"), c.cpuStatusLabel);
                cpus.append(m);
            }
            inv.insert(QStringLiteral("processors"), cpus);

            QVariantList dimms;
            for (const auto &d : r.memoryModules) {
                QVariantMap m;
                m.insert(QStringLiteral("bankLabel"),     d.bankLabel);
                m.insert(QStringLiteral("manufacturer"),  d.manufacturer);
                m.insert(QStringLiteral("serialNumber"),  d.serialNumber);
                m.insert(QStringLiteral("capacityBytes"), qlonglong(d.capacityBytes));
                m.insert(QStringLiteral("formFactor"),    d.formFactor);
                m.insert(QStringLiteral("formFactorLabel"), d.formFactorLabel);
                m.insert(QStringLiteral("memoryType"),    d.memoryType);
                m.insert(QStringLiteral("memoryTypeLabel"), d.memoryTypeLabel);
                m.insert(QStringLiteral("assetTag"),      d.assetTag);
                m.insert(QStringLiteral("partNumber"),    d.partNumber);
                dimms.append(m);
            }
            inv.insert(QStringLiteral("memoryModules"), dimms);

            QVariantList storage;
            for (const auto &s : r.storageDevices) {
                QVariantMap m;
                m.insert(QStringLiteral("model"),          s.model);
                m.insert(QStringLiteral("serialNumber"),   s.serialNumber);
                m.insert(QStringLiteral("maxMediaSizeKb"), qlonglong(s.maxMediaSizeKb));
                storage.append(m);
            }
            inv.insert(QStringLiteral("storageDevices"), storage);

            QVariantMap batt;
            batt.insert(QStringLiteral("present"),           r.battery.present);
            batt.insert(QStringLiteral("deviceId"),          r.battery.deviceId);
            batt.insert(QStringLiteral("manufacturer"),      r.battery.manufacturer);
            batt.insert(QStringLiteral("manufactureDate"),   r.battery.manufactureDate);
            batt.insert(QStringLiteral("serialNumber"),      r.battery.serialNumber);
            batt.insert(QStringLiteral("chemistry"),         r.battery.chemistry);
            batt.insert(QStringLiteral("chemistryLabel"),    r.battery.chemistryLabel);
            batt.insert(QStringLiteral("designCapacityMwh"), qlonglong(r.battery.designCapacityMwh));
            batt.insert(QStringLiteral("designVoltageMv"),   qlonglong(r.battery.designVoltageMv));
            batt.insert(QStringLiteral("otherIdentifyingInfo"), r.battery.otherIdentifyingInfo);
            inv.insert(QStringLiteral("battery"), batt);

            m_hardwareInventory = std::move(inv);
            emit hardwareChanged();
        });
}

void MachineDetailsController::refreshWireless()
{
    if (deferIfSshConnecting(PendingWireless)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getWireless(m_client,
        [this](qumesh::wsman::WirelessResult r) {
            decInflight();
            if (!r.ok && !r.error.isEmpty())
                setLastError(QStringLiteral("Wireless: %1").arg(r.error));

            QVariantMap port;
            port.insert(QStringLiteral("present"), r.port.present);
            port.insert(QStringLiteral("portState"), r.port.portState);
            port.insert(QStringLiteral("portStateLabel"),
                        qumesh::wsman::wifiPortStateLabel(r.port.portState));
            port.insert(QStringLiteral("radioState"), r.port.radioState);
            port.insert(QStringLiteral("radioStateLabel"),
                        qumesh::wsman::wifiRadioStateLabel(r.port.radioState));
            port.insert(QStringLiteral("currentSsid"), r.port.currentSsid);
            port.insert(QStringLiteral("localProfileSyncEnabled"),
                        r.port.localProfileSyncEnabled);
            port.insert(QStringLiteral("uefiProfileShareEnabled"),
                        r.port.uefiProfileShareEnabled);

            QVariantList profiles;
            for (const auto &p : r.profiles) {
                QVariantMap m;
                m.insert(QStringLiteral("elementName"), p.elementName);
                m.insert(QStringLiteral("ssid"),        p.ssid);
                m.insert(QStringLiteral("priority"),    p.priority);
                m.insert(QStringLiteral("authMethod"),  p.authenticationMethod);
                m.insert(QStringLiteral("authMethodLabel"),
                         qumesh::wsman::wifiAuthMethodLabel(p.authenticationMethod));
                m.insert(QStringLiteral("encryption"),  p.encryptionMethod);
                m.insert(QStringLiteral("encryptionLabel"),
                         qumesh::wsman::wifiEncryptionLabel(p.encryptionMethod));
                m.insert(QStringLiteral("eap8021xProtocol"), p.eap8021xProtocol);
                m.insert(QStringLiteral("eap8021xProtocolLabel"),
                         p.eap8021xProtocol >= 0
                             ? qumesh::wsman::eap8021xProtocolLabel(p.eap8021xProtocol)
                             : QString());
                profiles.append(m);
            }

            QVariantMap wired;
            wired.insert(QStringLiteral("present"), r.wired.present);
            wired.insert(QStringLiteral("enabled"), r.wired.enabled);
            wired.insert(QStringLiteral("authenticationProtocol"),
                         r.wired.authenticationProtocol);
            wired.insert(QStringLiteral("authProtocolLabel"),
                         r.wired.authenticationProtocol >= 0
                             ? qumesh::wsman::eap8021xProtocolLabel(
                                   r.wired.authenticationProtocol)
                             : QString());

            QVariantMap wl;
            wl.insert(QStringLiteral("ok"),       r.ok);
            wl.insert(QStringLiteral("port"),     port);
            wl.insert(QStringLiteral("profiles"), profiles);
            wl.insert(QStringLiteral("wired"),    wired);
            m_wireless = std::move(wl);
            emit wirelessChanged();
        });
}

void MachineDetailsController::refreshRemoteAccess()
{
    if (deferIfSshConnecting(PendingRemoteAccess)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getRemoteAccess(m_client,
        [this](qumesh::wsman::RemoteAccessResult r) {
            decInflight();
            if (!r.ok && !r.error.isEmpty())
                setLastError(QStringLiteral("Remote access: %1").arg(r.error));

            QVariantMap envDet;
            envDet.insert(QStringLiteral("enabled"),
                          !r.envDetection.domains.isEmpty());
            envDet.insert(QStringLiteral("domains"),
                          QVariant(r.envDetection.domains));
            envDet.insert(QStringLiteral("domainsLabel"),
                          r.envDetection.domains.join(", "));

            QVariantMap user;
            user.insert(QStringLiteral("enabledState"),
                        r.userInitiated.enabledState);
            user.insert(QStringLiteral("label"),
                        qumesh::wsman::userInitiatedCiraLabel(
                            r.userInitiated.enabledState));

            QVariantList policies;
            for (const auto &p : r.policies) {
                QVariantMap m;
                m.insert(QStringLiteral("name"),           p.name);
                m.insert(QStringLiteral("trigger"),        p.trigger);
                m.insert(QStringLiteral("tunnelLifeTime"), p.tunnelLifeTime);
                m.insert(QStringLiteral("mpsNames"),       p.mpsNames);
                m.insert(QStringLiteral("mpsNamesLabel"),  p.mpsNames.join(", "));
                if (p.periodicInterval) {
                    m.insert(QStringLiteral("scheduleLabel"),
                             QStringLiteral("Every %1 s").arg(p.periodicSeconds));
                } else if (p.periodicTimeOfDay) {
                    m.insert(QStringLiteral("scheduleLabel"),
                             QStringLiteral("%1:%2 daily")
                                 .arg(p.periodicHour, 2, 10, QLatin1Char('0'))
                                 .arg(p.periodicMinute, 2, 10, QLatin1Char('0')));
                } else {
                    m.insert(QStringLiteral("scheduleLabel"), QString());
                }
                policies.append(m);
            }

            QVariantList servers;
            for (const auto &s : r.servers) {
                QVariantMap m;
                m.insert(QStringLiteral("name"),       s.name);
                m.insert(QStringLiteral("accessInfo"), s.accessInfo);
                m.insert(QStringLiteral("port"),       s.port);
                m.insert(QStringLiteral("cn"),         s.cn);
                m.insert(QStringLiteral("mpsType"),    s.mpsType);
                m.insert(QStringLiteral("cila"),       s.mpsType == 1);
                servers.append(m);
            }

            QVariantList proxies;
            for (const auto &p : r.httpProxies) {
                QVariantMap m;
                m.insert(QStringLiteral("accessInfo"), p.accessInfo);
                m.insert(QStringLiteral("port"),       p.port);
                m.insert(QStringLiteral("networkDnsSuffix"), p.networkDnsSuffix);
                proxies.append(m);
            }

            QVariantMap ra;
            ra.insert(QStringLiteral("ok"),            r.ok);
            ra.insert(QStringLiteral("envDetection"),  envDet);
            ra.insert(QStringLiteral("userInitiated"), user);
            ra.insert(QStringLiteral("policies"),      policies);
            ra.insert(QStringLiteral("servers"),       servers);
            ra.insert(QStringLiteral("httpProxies"),   proxies);
            ra.insert(QStringLiteral("httpProxySupported"), r.httpProxySupported);
            m_remoteAccess = std::move(ra);
            emit remoteAccessChanged();
        });
}

void MachineDetailsController::refreshDeviceCerts()
{
    if (deferIfSshConnecting(PendingDeviceCerts)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    incInflight();
    qumesh::wsman::getDeviceCertStore(m_client,
        [this](qumesh::wsman::DeviceCertResult r) {
            decInflight();
            if (!r.ok && !r.error.isEmpty())
                setLastError(QStringLiteral("Device certs: %1").arg(r.error));

            QVariantList certs;
            QHash<QString, bool> hasKey;
            for (const auto &k : r.keyPairs)
                hasKey.insert(k.instanceId, true);
            const QSet<QString> activeIds(r.activeCertInstanceIds.begin(),
                                          r.activeCertInstanceIds.end());

            QSet<QString> keysClaimedByCerts;
            for (const auto &c : r.certificates) {
                QVariantMap m;
                m.insert(QStringLiteral("instanceId"),    c.instanceId);
                m.insert(QStringLiteral("subjectCn"),     c.subjectCn);
                m.insert(QStringLiteral("issuerCn"),      c.issuerCn);
                m.insert(QStringLiteral("subjectRaw"),    c.subjectRaw);
                m.insert(QStringLiteral("issuerRaw"),     c.issuerRaw);
                m.insert(QStringLiteral("trustedRoot"),   c.trustedRoot);
                m.insert(QStringLiteral("derSizeBytes"),  c.derSizeBytes);
                // A cert "has a private key" when a key pair shares
                // an InstanceID-suffix with the cert. AMT's pairing
                // convention is "Intel(r) AMT Certificate: <n>" vs
                // "Intel(r) AMT Key: <n>" — match by the trailing
                // suffix.
                bool hasPrivateKey = false;
                QString matchedKey;
                const int colon = c.instanceId.lastIndexOf(QLatin1Char(':'));
                const QString suffix = colon < 0 ? QString()
                                                 : c.instanceId.mid(colon + 1).trimmed();
                if (!suffix.isEmpty()) {
                    for (const auto &k : r.keyPairs) {
                        if (k.instanceId.endsWith(suffix)) {
                            hasPrivateKey = true;
                            matchedKey = k.instanceId;
                            break;
                        }
                    }
                }
                if (!matchedKey.isEmpty())
                    keysClaimedByCerts.insert(matchedKey);
                m.insert(QStringLiteral("hasPrivateKey"), hasPrivateKey);
                m.insert(QStringLiteral("active"),
                         activeIds.contains(c.instanceId));
                certs.append(m);
            }

            QVariantList orphans;
            for (const auto &k : r.keyPairs) {
                if (keysClaimedByCerts.contains(k.instanceId)) continue;
                QVariantMap m;
                m.insert(QStringLiteral("instanceId"),   k.instanceId);
                m.insert(QStringLiteral("derSizeBytes"), k.derSizeBytes);
                orphans.append(m);
            }

            QVariantList tls;
            for (const auto &t : r.tlsSettings) {
                QVariantMap m;
                m.insert(QStringLiteral("instanceId"), t.instanceId);
                m.insert(QStringLiteral("isLocal"),
                         t.instanceId.contains(QStringLiteral("LMS")));
                m.insert(QStringLiteral("enabled"),      t.enabled);
                m.insert(QStringLiteral("mutualAuthentication"),
                         t.mutualAuthentication);
                m.insert(QStringLiteral("acceptNonSecureConnections"),
                         t.acceptNonSecureConnections);
                m.insert(QStringLiteral("trustedCn"),    t.trustedCn);
                m.insert(QStringLiteral("trustedCnLabel"), t.trustedCn.join(", "));
                QString label;
                if (!t.enabled) {
                    label = QStringLiteral("Disabled");
                } else {
                    label = t.mutualAuthentication
                        ? QStringLiteral("Mutual-auth TLS")
                        : QStringLiteral("Server-auth TLS");
                    if (t.acceptNonSecureConnections)
                        label += QStringLiteral(" + non-TLS");
                }
                m.insert(QStringLiteral("label"), label);
                tls.append(m);
            }

            QVariantMap store;
            store.insert(QStringLiteral("certificates"), certs);
            store.insert(QStringLiteral("orphanKeys"),   orphans);
            store.insert(QStringLiteral("tlsSettings"),  tls);
            store.insert(QStringLiteral("ok"),           r.ok);
            m_deviceCertStore = std::move(store);
            emit deviceCertStoreChanged();
        });
}

void MachineDetailsController::refreshAuditLog()
{
    if (deferIfSshConnecting(PendingAuditLog)) return;
    rebuildEndpoint();
    if (m_host.isEmpty()) {
        setLastError(QStringLiteral("Host is empty — cannot refresh."));
        return;
    }
    // Kick the state read and the records walk in parallel; QML only
    // re-renders once both have completed by binding to the single
    // `auditLogChanged` signal that each emits.
    incInflight();
    qumesh::wsman::getAuditLogState(m_client,
        [this](qumesh::wsman::AuditLogState s) {
            decInflight();
            QVariantMap st;
            st.insert(QStringLiteral("ok"), s.ok);
            st.insert(QStringLiteral("auditState"),             s.auditState);
            st.insert(QStringLiteral("overwritePolicy"),        s.overwritePolicy);
            st.insert(QStringLiteral("currentNumberOfRecords"), s.currentNumberOfRecords);
            st.insert(QStringLiteral("percentageFree"),         s.percentageFree);
            st.insert(QStringLiteral("maxAllowedAuditors"),     s.maxAllowedAuditors);
            st.insert(QStringLiteral("enabledState"),           s.enabledState);
            // Decoded bits — bit 0 enabled, bit 1 locked, bit 2 almost
            // full, bit 3 full, bit 4 no signing key.
            st.insert(QStringLiteral("enabled"),     (s.auditState & 0x01) != 0);
            st.insert(QStringLiteral("locked"),      (s.auditState & 0x02) != 0);
            st.insert(QStringLiteral("almostFull"),  (s.auditState & 0x04) != 0);
            st.insert(QStringLiteral("full"),        (s.auditState & 0x08) != 0);
            st.insert(QStringLiteral("noSigningKey"), (s.auditState & 0x10) != 0);
            m_auditLogState = std::move(st);
            emit auditLogChanged();
        });

    incInflight();
    qumesh::wsman::enumerateAuditLog(m_client,
        [this](qumesh::wsman::AuditLogResult r) {
            decInflight();
            if (!r.ok && !r.error.isEmpty())
                setLastError(QStringLiteral("Audit log: %1").arg(r.error));
            QVariantList list;
            list.reserve(r.entries.size());
            for (const auto &e : r.entries) {
                QVariantMap m;
                m.insert(QStringLiteral("auditAppId"),    e.auditAppId);
                m.insert(QStringLiteral("eventId"),       e.eventId);
                m.insert(QStringLiteral("auditAppLabel"), e.auditAppLabel);
                m.insert(QStringLiteral("eventLabel"),    e.eventLabel);
                m.insert(QStringLiteral("initiatorType"), e.initiatorType);
                m.insert(QStringLiteral("initiator"),     e.initiator);
                m.insert(QStringLiteral("unixSeconds"),   qlonglong(e.unixSeconds));
                m.insert(QStringLiteral("netAddress"),    e.netAddress);
                list.append(m);
            }
            m_auditLogEntries = std::move(list);
            emit auditLogChanged();
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
                m.insert(QStringLiteral("handle"),           u.handle);
                m.insert(QStringLiteral("name"),             u.name);
                m.insert(QStringLiteral("digestUsername"),   u.digestUsername);
                m.insert(QStringLiteral("kerberosUserSidB64"), u.kerberosUserSidB64);
                m.insert(QStringLiteral("isKerberos"),
                         !u.kerberosUserSidB64.isEmpty());
                m.insert(QStringLiteral("accessPermission"),     u.accessPermission);
                m.insert(QStringLiteral("accessPermissionLabel"),
                         qumesh::wsman::accessPermissionLabel(u.accessPermission));
                // Human-readable realm list: filter out the unnamed
                // reserved slots, label realm 3 specially as
                // "Administrator" since it's the all-access bit.
                QVariantList realmList;
                QStringList realmLabels;
                bool isAdmin = (u.accessPermission == 999);
                for (int rr : u.realms) {
                    realmList.append(rr);
                    if (rr == 3) {
                        isAdmin = true;
                        realmLabels.append(QStringLiteral("Administrator"));
                    } else {
                        const QString rn = qumesh::wsman::realmName(rr);
                        if (!rn.isEmpty()) realmLabels.append(rn);
                    }
                }
                m.insert(QStringLiteral("realms"),       realmList);
                m.insert(QStringLiteral("realmCount"),   realmList.size());
                m.insert(QStringLiteral("realmsLabel"),  realmLabels.join(", "));
                m.insert(QStringLiteral("isAdmin"),      isAdmin);
                m.insert(QStringLiteral("enabled"),      u.enabled);
                m.insert(QStringLiteral("hidden"),       u.hidden);
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
