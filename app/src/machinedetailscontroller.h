// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include "wsman/wsman_client.h" // for PeerCertSummary

#include <QVariantMap>

namespace qumesh::ssh { class SshSession; }

namespace qumesh::app {

/// QML-creatable controller for one Machine Details window. Owns a
/// WSMAN client, exposes per-category fetch methods, and surfaces the
/// results as Q_PROPERTYs the QML side binds to. No background polling
/// — every fetch is user-triggered (or kicked off when a page becomes
/// visible).
class MachineDetailsController : public QObject
{
    Q_OBJECT

    // Connection inputs (set by the QML window from the saved machine).
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool tls READ tls WRITE setTls NOTIFY tlsChanged)
    Q_PROPERTY(QStringList trustedFingerprints READ trustedFingerprints
                   WRITE setTrustedFingerprints NOTIFY trustedFingerprintsChanged)

    // SSH tunnel — `true` when the saved machine config asks us to route
    // every byte through an SSH session. The actual SSH parameters are
    // pushed via `setSshConfig`; this bool is the cheap "is tunneling
    // active?" indicator the QML side binds to for the title-bar badge.
    Q_PROPERTY(bool sshTunnelActive READ sshTunnelActive NOTIFY sshTunnelStateChanged)
    Q_PROPERTY(QString sshTunnelStatus READ sshTunnelStatus NOTIFY sshTunnelStateChanged)

    // Trust-on-first-use surface for CertTrustDialog. Mirrors the shape
    // SolController / KvmController already expose so the same dialog
    // can render us.
    Q_PROPERTY(bool awaitingTrust READ awaitingTrust NOTIFY awaitingTrustChanged)
    Q_PROPERTY(QString pendingCertSubject READ pendingCertSubject NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertIssuer READ pendingCertIssuer NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertFingerprint READ pendingCertFingerprint NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotBefore READ pendingCertNotBefore NOTIFY pendingCertChanged)
    Q_PROPERTY(QString pendingCertNotAfter READ pendingCertNotAfter NOTIFY pendingCertChanged)

    // Overview / power.
    Q_PROPERTY(int powerState READ powerState NOTIFY powerStateChanged)
    Q_PROPERTY(QString powerStateLabel READ powerStateLabel NOTIFY powerStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // Identify.
    Q_PROPERTY(QString amtProtocolVersion READ amtProtocolVersion NOTIFY identifyChanged)
    Q_PROPERTY(QString amtVendor READ amtVendor NOTIFY identifyChanged)
    Q_PROPERTY(QString amtVersion READ amtVersion NOTIFY identifyChanged)
    /// Parsed `amtVersion.major` (the first integer of the dotted
    /// version) — used by the QML side to gate menu entries on
    /// firmware that needs a minimum AMT level (e.g. OS Wake-on-Sleep
    /// requires AMT 10+).
    Q_PROPERTY(int amtVersionMajor READ amtVersionMajor NOTIFY identifyChanged)

    // General settings.
    Q_PROPERTY(QString hostName READ hostName NOTIFY generalSettingsChanged)
    Q_PROPERTY(QString domainName READ domainName NOTIFY generalSettingsChanged)
    Q_PROPERTY(QString digestRealm READ digestRealm NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool networkInterfaceEnabled READ networkInterfaceEnabled NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool rmcpPingResponseEnabled READ rmcpPingResponseEnabled NOTIFY generalSettingsChanged)
    /// -1 = unknown, 0 = plugged in / AC, 1 = on battery.
    Q_PROPERTY(int powerSource READ powerSource NOTIFY generalSettingsChanged)
    /// Localized label matching `powerSource` (so QML doesn't need its
    /// own switch).
    Q_PROPERTY(QString powerSourceLabel READ powerSourceLabel NOTIFY generalSettingsChanged)

    // Intel ME firmware version (CIM_SoftwareIdentity[InstanceID='AMT']).
    Q_PROPERTY(QString meVersionString READ meVersionString NOTIFY meVersionChanged)

    // Provisioning state (AMT_SetupAndConfigurationService).
    Q_PROPERTY(int provisioningState READ provisioningState NOTIFY setupConfigChanged)
    Q_PROPERTY(int provisioningMode  READ provisioningMode  NOTIFY setupConfigChanged)
    Q_PROPERTY(QString provisioningModeLabel READ provisioningModeLabel
                   NOTIFY setupConfigChanged)

    // Active features — derived from AMT_RedirectionService + CIM_KVMRedirectionSAP.
    Q_PROPERTY(bool redirectionListenerEnabled READ redirectionListenerEnabled
                   NOTIFY redirectionStatusChanged)
    Q_PROPERTY(bool solEnabled  READ solEnabled  NOTIFY redirectionStatusChanged)
    Q_PROPERTY(bool iderEnabled READ iderEnabled NOTIFY redirectionStatusChanged)
    Q_PROPERTY(bool kvmEnabled  READ kvmEnabled  NOTIFY redirectionStatusChanged)
    Q_PROPERTY(bool kvmAvailable READ kvmAvailable NOTIFY redirectionStatusChanged)

    // Computer system.
    Q_PROPERTY(QString systemName READ systemName NOTIFY computerSystemChanged)
    Q_PROPERTY(QString systemElementName READ systemElementName NOTIFY computerSystemChanged)
    Q_PROPERTY(QString systemUuid READ systemUuid NOTIFY computerSystemChanged)

    // Ethernet settings.
    Q_PROPERTY(QString macAddress READ macAddress NOTIFY ethernetChanged)
    Q_PROPERTY(bool dhcpEnabled READ dhcpEnabled NOTIFY ethernetChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ethernetChanged)
    Q_PROPERTY(QString subnetMask READ subnetMask NOTIFY ethernetChanged)
    Q_PROPERTY(QString defaultGateway READ defaultGateway NOTIFY ethernetChanged)
    Q_PROPERTY(QString primaryDns READ primaryDns NOTIFY ethernetChanged)
    Q_PROPERTY(QString secondaryDns READ secondaryDns NOTIFY ethernetChanged)

    // Time.
    Q_PROPERTY(qint64 amtEpoch READ amtEpoch NOTIFY timeChanged)

    // Event log — list of QVariantMaps for the QML ListView.
    Q_PROPERTY(QVariantList eventLog READ eventLog NOTIFY eventLogChanged)

    // User accounts — list of QVariantMaps.
    Q_PROPERTY(QVariantList userAccounts READ userAccounts NOTIFY userAccountsChanged)

    // Boot capabilities — which power-to-X menu entries we show.
    Q_PROPERTY(bool capBiosSetup READ capBiosSetup NOTIFY bootCapabilitiesChanged)
    Q_PROPERTY(bool capBiosPause READ capBiosPause NOTIFY bootCapabilitiesChanged)
    Q_PROPERTY(bool capSecureErase READ capSecureErase NOTIFY bootCapabilitiesChanged)
    Q_PROPERTY(bool capPlatformErase READ capPlatformErase NOTIFY bootCapabilitiesChanged)
    /// Raw `AMT_BootCapabilities.PlatformErase` bitmask the QML uses to
    /// build the per-sub-action checkbox list.
    Q_PROPERTY(int capPlatformEraseMask READ capPlatformEraseMask NOTIFY bootCapabilitiesChanged)
    Q_PROPERTY(bool capForceUefiHttpsBoot READ capForceUefiHttpsBoot NOTIFY bootCapabilitiesChanged)

    // User-consent (OptIn) policy + runtime state. Populated after
    // `refreshOverview` (and refreshed on demand via `refreshOptInStatus`).
    Q_PROPERTY(bool optInRequired READ optInRequired NOTIFY optInStatusChanged)
    Q_PROPERTY(int optInState READ optInState NOTIFY optInStatusChanged)
    Q_PROPERTY(bool canModifyOptInPolicy READ canModifyOptInPolicy NOTIFY optInStatusChanged)
    Q_PROPERTY(bool kvmOptInPolicy READ kvmOptInPolicy NOTIFY optInStatusChanged)

public:
    explicit MachineDetailsController(QObject *parent = nullptr);
    ~MachineDetailsController() override;

    [[nodiscard]] QString host() const { return m_host; }
    [[nodiscard]] QString user() const { return m_user; }
    [[nodiscard]] QString password() const { return m_password; }
    [[nodiscard]] bool    tls()  const { return m_tls; }
    [[nodiscard]] QStringList trustedFingerprints() const { return m_trustedFingerprints; }
    void setHost(const QString &v);
    void setUser(const QString &v);
    void setPassword(const QString &v);
    void setTls(bool v);
    void setTrustedFingerprints(QStringList v);

    [[nodiscard]] bool sshTunnelActive() const;
    [[nodiscard]] QString sshTunnelStatus() const { return m_sshTunnelStatus; }

    /// Apply the per-machine SSH config (as produced by
    /// `ComputerModel::sshConfigFor`). When `cfg["enabled"]` is true the
    /// controller routes every WSMAN request through the SSH tunnel by
    /// installing a socket factory on the underlying `WsmanClient`.
    /// Passing an empty / disabled map tears any existing tunnel down.
    Q_INVOKABLE void setSshConfig(const QVariantMap &cfg);

    [[nodiscard]] bool awaitingTrust() const { return m_awaitingTrust; }
    [[nodiscard]] QString pendingCertSubject() const { return m_pendingCert.subject; }
    [[nodiscard]] QString pendingCertIssuer() const { return m_pendingCert.issuer; }
    [[nodiscard]] QString pendingCertFingerprint() const { return m_pendingCert.fingerprintSha256; }
    [[nodiscard]] QString pendingCertNotBefore() const { return m_pendingCert.notBefore; }
    [[nodiscard]] QString pendingCertNotAfter() const { return m_pendingCert.notAfter; }

    [[nodiscard]] int powerState() const { return m_powerState; }
    [[nodiscard]] QString powerStateLabel() const;
    [[nodiscard]] bool busy() const { return m_inflight > 0; }
    [[nodiscard]] QString lastError() const { return m_lastError; }

    [[nodiscard]] QString amtProtocolVersion() const { return m_amtProtocolVersion; }
    [[nodiscard]] QString amtVendor() const { return m_amtVendor; }
    [[nodiscard]] QString amtVersion() const { return m_amtVersion; }
    [[nodiscard]] int amtVersionMajor() const;

    [[nodiscard]] QString hostName() const { return m_hostName; }
    [[nodiscard]] QString domainName() const { return m_domainName; }
    [[nodiscard]] QString digestRealm() const { return m_digestRealm; }
    [[nodiscard]] bool networkInterfaceEnabled() const { return m_networkInterfaceEnabled; }
    [[nodiscard]] bool rmcpPingResponseEnabled() const { return m_rmcpPingResponseEnabled; }
    [[nodiscard]] int     powerSource() const { return m_powerSource; }
    [[nodiscard]] QString powerSourceLabel() const;

    [[nodiscard]] QString meVersionString() const { return m_meVersionString; }

    [[nodiscard]] int provisioningState() const { return m_provisioningState; }
    [[nodiscard]] int provisioningMode()  const { return m_provisioningMode; }
    [[nodiscard]] QString provisioningModeLabel() const;

    [[nodiscard]] bool redirectionListenerEnabled() const { return m_redirectionListenerEnabled; }
    [[nodiscard]] bool solEnabled()   const { return m_solEnabled; }
    [[nodiscard]] bool iderEnabled()  const { return m_iderEnabled; }
    [[nodiscard]] bool kvmEnabled()   const { return m_kvmEnabled; }
    [[nodiscard]] bool kvmAvailable() const { return m_kvmAvailable; }

    [[nodiscard]] QString systemName() const { return m_systemName; }
    [[nodiscard]] QString systemElementName() const { return m_systemElementName; }
    [[nodiscard]] QString systemUuid() const { return m_systemUuid; }

    [[nodiscard]] QString macAddress() const { return m_macAddress; }
    [[nodiscard]] bool dhcpEnabled() const { return m_dhcpEnabled; }
    [[nodiscard]] QString ipAddress() const { return m_ipAddress; }
    [[nodiscard]] QString subnetMask() const { return m_subnetMask; }
    [[nodiscard]] QString defaultGateway() const { return m_defaultGateway; }
    [[nodiscard]] QString primaryDns() const { return m_primaryDns; }
    [[nodiscard]] QString secondaryDns() const { return m_secondaryDns; }

    [[nodiscard]] qint64 amtEpoch() const { return m_amtEpoch; }

    [[nodiscard]] bool capBiosSetup() const { return m_capBiosSetup; }
    [[nodiscard]] bool capBiosPause() const { return m_capBiosPause; }
    [[nodiscard]] bool capSecureErase() const { return m_capSecureErase; }
    [[nodiscard]] bool capPlatformErase() const { return m_capPlatformErase; }
    [[nodiscard]] int  capPlatformEraseMask() const {
        return static_cast<int>(m_capPlatformEraseMask);
    }
    [[nodiscard]] bool capForceUefiHttpsBoot() const { return m_capForceUefiHttpsBoot; }

    [[nodiscard]] bool optInRequired() const { return m_optInRequired; }
    [[nodiscard]] int  optInState() const { return m_optInState; }
    [[nodiscard]] bool canModifyOptInPolicy() const { return m_canModifyOptInPolicy; }
    [[nodiscard]] bool kvmOptInPolicy() const { return m_kvmOptInPolicy; }

    [[nodiscard]] QVariantList eventLog() const { return m_eventLog; }
    [[nodiscard]] QVariantList userAccounts() const { return m_userAccounts; }

    /// Fetch the overview bundle (identify + general settings + system +
    /// power state). Each completes independently; the QML side just
    /// reacts to whichever property changes.
    Q_INVOKABLE void refreshOverview();

    /// Refresh individual categories.
    Q_INVOKABLE void refreshNetwork();
    Q_INVOKABLE void refreshTime();
    Q_INVOKABLE void refreshPower();
    Q_INVOKABLE void refreshEventLog();
    Q_INVOKABLE void refreshUserAccounts();
    /// Read `IPS_OptInService` + `IPS_KVMRedirectionSettingData` and
    /// update the four exposed properties. Also called by
    /// `refreshOverview`.
    Q_INVOKABLE void refreshOptInStatus();
    /// Flip the persisted KVM consent policy. Fires
    /// `optInPolicyChangeFailed` if AMT rejects the Put (most often
    /// because the current login lacks the realm).
    Q_INVOKABLE void setKvmOptInPolicyEnabled(bool enabled);
    /// Start the AMT-side consent prompt — the firmware shows a
    /// 6-digit code on the target's local screen.
    Q_INVOKABLE void startOptIn();
    /// Submit the operator-entered consent code.
    Q_INVOKABLE void sendOptInCode(int code);
    /// Abort a pending opt-in.
    Q_INVOKABLE void cancelOptIn();

    /// CIM power-state codes:
    ///   2  = Power On
    ///   5  = Master Bus Reset
    ///   8  = Power Off (Hard)
    ///   10 = Master Bus Reset Graceful
    ///   12 = Power Off (Soft Graceful)
    Q_INVOKABLE void powerOn()           { changePowerState(2); }
    Q_INVOKABLE void powerOffHard()      { changePowerState(8); }
    Q_INVOKABLE void powerReset()        { changePowerState(5); }
    Q_INVOKABLE void powerResetGraceful(){ changePowerState(10); }
    Q_INVOKABLE void powerOffSoft()      { changePowerState(12); }

    /// Boot-source-override actions. Each chains
    /// ChangeBootOrder(clear) → Put AMT_BootSettingData →
    /// SetBootConfigRole(1) → (optionally) ChangeBootOrder(source) →
    /// RequestPowerStateChange. `reset` selects between power-on
    /// (false → CIM state 2) and graceful-reset (true → CIM state 10).
    Q_INVOKABLE void bootToBios(bool reset);
    Q_INVOKABLE void bootToPxe(bool reset);
    Q_INVOKABLE void bootToIderCdrom(bool reset);
    Q_INVOKABLE void bootToIderFloppy(bool reset);

    /// `IPS_PowerManagementService.RequestOSPowerSavingStateChange`
    /// — wake the running OS from sleep / put it back to sleep. AMT
    /// 10+ only; the QML side hides the menu entries below that.
    Q_INVOKABLE void osWakeFromSleep();
    Q_INVOKABLE void osPutToSleep();

    /// Boot to Secure Erase. `password` is the AMT RSE (Remote Secure
    /// Erase) password — required by firmware versions that enforce
    /// it. Pass empty when not configured. Gated in the UI on
    /// `capSecureErase`.
    Q_INVOKABLE void bootToSecureErase(bool reset, const QString &password);

    /// Boot to Platform Erase. `flags` is a subset of
    /// `capPlatformEraseMask` — the bits the user ticked in the
    /// modal. `psid` only applies when bit 1 (Pyrite Revert) is set;
    /// `ssdPassword` only when bit 2 (Secure Erase All SSDs) is set.
    Q_INVOKABLE void bootToPlatformErase(bool reset, int flags,
                                          const QString &psid,
                                          const QString &ssdPassword);

    /// Boot to an arbitrary HTTPS-hosted ISO. AMT does the download
    /// itself, so `url` must be a reachable HTTPS endpoint. Gated in
    /// the UI on `capForceUefiHttpsBoot` + the connection being TLS
    /// (otherwise the firmware refuses).
    Q_INVOKABLE void bootToHttpsBootUrl(bool reset, const QString &url);

    /// Called by the QML trust prompt. On accept, the pending cert's
    /// fingerprint is promoted into the trusted list, the trust state
    /// clears, and the operation that triggered the prompt is retried.
    /// When `persist` is true we also emit `trustedFingerprintAdded`
    /// so the QML layer can write the fingerprint back to ComputerModel.
    Q_INVOKABLE void trustPendingCert(bool persist);

    /// Called by the SSH host-key trust prompt. On accept, the
    /// pending SSH key fingerprint is promoted into the trusted list
    /// and the in-flight SSH session resumes its auth step.
    Q_INVOKABLE void trustPendingSshHostKey(bool persist);

    /// Called by CertTrustDialog when the user declines the prompt.
    /// Clears pending trust state and asks the QML window to close.
    Q_INVOKABLE void close();

signals:
    void hostChanged();
    void userChanged();
    void passwordChanged();
    void tlsChanged();
    void trustedFingerprintsChanged();
    void busyChanged();
    void lastErrorChanged();
    void powerStateChanged();
    void identifyChanged();
    void generalSettingsChanged();
    void computerSystemChanged();
    void ethernetChanged();
    void timeChanged();
    void meVersionChanged();
    void setupConfigChanged();
    void redirectionStatusChanged();
    void awaitingTrustChanged();
    void pendingCertChanged();
    void bootCapabilitiesChanged();
    void eventLogChanged();
    void userAccountsChanged();
    void optInStatusChanged();
    /// Result of a `setKvmOptInPolicyEnabled` Put. `ok=false` carries
    /// the firmware-reported reason (most commonly the AMT login lacks
    /// the realm to modify the policy).
    void optInPolicyChangeFailed(const QString &error);
    /// Result of `startOptIn` — when ok, AMT is now showing a code on
    /// the target's local screen and the operator should be prompted
    /// for it.
    void optInStarted(bool ok, const QString &error);
    /// Result of `sendOptInCode` — when ok, the redir framebuffer /
    /// serial / IDE-R should unblock on the active session.
    void optInCodeResult(bool ok, const QString &error);
    void powerChangeRequested(int state);
    void powerChangeCompleted(int state, bool ok, const QString &error);
    /// Emitted after `trustPendingCert(true)` — the QML layer persists
    /// the fingerprint into ComputerModel.
    void trustedFingerprintAdded(const QString &fingerprint);
    /// Forwarded from the underlying client whenever a TLS
    /// reconnect quietly matched a pinned fingerprint. The QML side
    /// uses it to flash a small "verified" badge.
    void peerCertVerifiedByPin(const QString &fingerprint);
    /// Emitted from `close()`; the QML window listens and dismisses
    /// itself.
    void closeRequested();
    /// SSH-tunnel state change. The QML title bar listens and lights
    /// the "via SSH" badge / error state.
    void sshTunnelStateChanged();
    /// Emitted when the operator must confirm an SSH host key on first
    /// connect (or after a key rotation). The QML side surfaces a
    /// confirm-and-pin dialog and either calls `trustPendingSshHostKey`
    /// or `close()`.
    void sshHostKeyPromptRequired(const QString &fingerprint,
                                   const QString &keyType);
    /// Emitted after `trustPendingSshHostKey()` — the QML layer
    /// persists the fingerprint into ComputerModel.
    void trustedSshHostKeyAdded(const QString &fingerprint);

private:
    void rebuildEndpoint();
    void changePowerState(int code);
    void incInflight();
    void decInflight();
    void setLastError(const QString &e);
    void onTrustPromptRequired(const qumesh::wsman::PeerCertSummary &s);

    /// Refresh methods that get called while the SSH tunnel is still
    /// negotiating are buffered here as bits and replayed once the
    /// `SshSession` reaches `Connected`. Without this the first wave
    /// of refresh calls (overview, power, etc.) hits the WSMAN socket
    /// factory before the session is up and every one fails with
    /// "SSH tunnel socket could not be opened".
    enum PendingRefresh : int {
        PendingOverview     = 1 << 0,
        PendingNetwork      = 1 << 1,
        PendingTime         = 1 << 2,
        PendingPower        = 1 << 3,
        PendingEventLog     = 1 << 4,
        PendingUserAccounts = 1 << 5,
    };
    int m_pendingRefreshes = 0;
    /// `true` between `setSshConfig(enabled=true)` and the session
    /// reaching `Connected` (or `Failed`). While true, refreshes are
    /// buffered into `m_pendingRefreshes` instead of being issued.
    bool m_sshConnecting = false;
    [[nodiscard]] bool deferIfSshConnecting(PendingRefresh kind);
    void runPendingRefreshes();
    /// Remember which top-level fetch was in flight when the trust
    /// prompt fired, so we can resume it once the user accepts. -1
    /// means "the section the user is currently looking at".
    enum class PendingOp { None, Overview, Power, Network, Time };
    PendingOp m_pendingOp = PendingOp::None;

    qumesh::wsman::WsmanClient *m_client = nullptr;
    QString m_host;
    QString m_user;
    QString m_password;
    bool m_tls = false;
    QStringList m_trustedFingerprints;
    qumesh::wsman::PeerCertSummary m_pendingCert;
    bool m_awaitingTrust = false;

    // SSH tunnel state. The session is lazily created when
    // `setSshConfig(...enabled=true)` is called; per-request
    // SshTunnels are then handed to the WsmanClient via its socket
    // factory. The status string is what the QML title-bar badge
    // displays ("connecting…", "via SSH", error text).
    qumesh::ssh::SshSession *m_sshSession = nullptr;
    bool m_sshEnabled = false;
    QString m_sshTunnelStatus;
    QString m_pendingSshHostKey;
    QString m_pendingSshHostKeyType;
    bool m_awaitingSshHostKeyTrust = false;

    int m_powerState = 0;
    int m_inflight = 0;
    QString m_lastError;

    QString m_amtProtocolVersion;
    QString m_amtVendor;
    QString m_amtVersion;

    QString m_hostName;
    QString m_domainName;
    QString m_digestRealm;
    bool m_networkInterfaceEnabled = false;
    bool m_rmcpPingResponseEnabled = false;
    int m_powerSource = -1;

    QString m_meVersionString;

    int m_provisioningState = -1;
    int m_provisioningMode  = -1;

    bool m_redirectionListenerEnabled = false;
    bool m_solEnabled  = false;
    bool m_iderEnabled = false;
    bool m_kvmEnabled  = false;
    bool m_kvmAvailable = false;

    QString m_systemName;
    QString m_systemElementName;
    QString m_systemUuid;

    QString m_macAddress;
    bool m_dhcpEnabled = false;
    QString m_ipAddress;
    QString m_subnetMask;
    QString m_defaultGateway;
    QString m_primaryDns;
    QString m_secondaryDns;

    qint64 m_amtEpoch = 0;

    bool m_capBiosSetup = false;
    bool m_capBiosPause = false;
    bool m_capSecureErase = false;
    bool m_capPlatformErase = false;
    quint32 m_capPlatformEraseMask = 0;
    bool m_capForceUefiHttpsBoot = false;

    QVariantList m_eventLog;
    QVariantList m_userAccounts;

    bool m_optInRequired = false;
    int m_optInState = 0;
    bool m_canModifyOptInPolicy = false;
    bool m_kvmOptInPolicy = false;
};

} // namespace qumesh::app
