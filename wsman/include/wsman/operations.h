// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <functional>
#include <utility>

namespace qumesh::wsman {

class WsmanClient;

struct IdentifyResult
{
    bool ok = false;
    QString error;
    QString protocolVersion;
    QString productVendor;
    QString productVersion;
};

struct PowerStateResult
{
    bool ok = false;
    QString error;
    int powerState = -1;
};

struct GeneralSettingsResult
{
    bool ok = false;
    QString error;
    QString hostName;           ///< `AMT_GeneralSettings.HostName`
    QString domainName;         ///< `AMT_GeneralSettings.DomainName`
    QString amtVersion;         ///< `AMT_GeneralSettings.AMTNetworkEnabled` etc. — best-effort.
    QString digestRealm;        ///< Optional, included for cert / auth display.
    bool networkInterfaceEnabled = false;
    bool rmcpPingResponseEnabled = false;
    /// `AMT_GeneralSettings.PowerSource` — 0 = plugged-in / AC, 1 = on
    /// battery. -1 means the firmware didn't report it (older releases,
    /// desktop SKUs).
    int powerSource = -1;
};

struct ComputerSystemResult
{
    bool ok = false;
    QString error;
    QString name;               ///< `CIM_ComputerSystem.Name`
    QString elementName;        ///< `CIM_ComputerSystem.ElementName`
    QString creationClassName;
    QString systemUuid;         ///< From `CIM_ComputerSystemPackage` or `Name`
};

/// One IPv6 endpoint, paired with an EthernetInterface by ordinal.
struct IPv6PortSettings
{
    bool present = false;
    QString instanceId;
    QStringList addresses;        ///< `CurrentAddressInfo`, address-only.
    QString defaultRouter;        ///< `CurrentDefaultRouter`.
    QString primaryDns;           ///< `CurrentPrimaryDNS`.
    QString secondaryDns;         ///< `CurrentSecondaryDNS`.
};

/// One row of `AMT_EthernetPortSettings`, plus the matching IPv6 block.
struct EthernetInterface
{
    QString instanceId;           ///< e.g. "Intel(r) AMT Ethernet Port Settings 0".
    QString macAddress;
    bool dhcpEnabled = false;
    bool ipSyncEnabled = false;
    QString ipAddress;
    QString subnetMask;
    QString defaultGateway;
    QString primaryDns;
    QString secondaryDns;
    /// `AMT_EthernetPortSettings.LinkPolicy` — raw integer codes.
    /// 1 = S0/AC, 14 = Sx/AC, 16 = S0/DC, 224 = Sx/DC.
    QList<int> linkPolicy;

    IPv6PortSettings ipv6;
};

struct EthernetSettingsResult
{
    bool ok = false;
    QString error;
    QList<EthernetInterface> interfaces;

    // Backward-compat scalars, populated from `interfaces[0]` so the
    // existing single-NIC bindings keep working.
    QString macAddress;
    bool dhcpEnabled = false;
    bool ipv4Enabled = false;
    QString ipAddress;
    QString subnetMask;
    QString defaultGateway;
    QString primaryDns;
    QString secondaryDns;
    QString linkPolicy;          ///< Reserved (kept for ABI; not populated).
};

/// Render a `LinkPolicy` bitmask code into a human-readable label.
[[nodiscard]] QString linkPolicyLabel(int code);

struct TimeSettingsResult
{
    bool ok = false;
    QString error;
    qint64 secondsSinceEpoch = 0; ///< 0 if not parsed.
};

/// Snapshot of `AMT_SetupAndConfigurationService` — the provisioning
/// state machine. `ProvisioningState == 2` means the device has finished
/// activation; `ProvisioningMode == 4` means it was activated in Client
/// Control Mode (otherwise Admin Control Mode).
struct SetupAndConfigResult
{
    bool ok = false;
    QString error;
    int provisioningState = -1;
    int provisioningMode  = -1;
};

/// `CIM_SoftwareIdentity` snapshot — the AMT firmware exposes several
/// rows in this collection, each keyed by an `InstanceID` string and
/// carrying a `VersionString`. The names of the well-known rows are
/// firmware-version-dependent; the parser normalises them by case-
/// insensitive substring match. `versionString` retains the AMT main
/// version for backwards compatibility with existing callers.
struct MeVersionResult
{
    bool ok = false;
    QString error;

    /// `InstanceID == "AMT"` — main AMT firmware version
    /// (e.g. "16.1.25.2122"). Distinct from the WSMAN protocol version
    /// returned by `Identify`.
    QString versionString;

    /// `InstanceID == "AMT FW Recovery Version"` (or similar) — the
    /// recovery image version, ships separately from the main FW.
    QString recoveryVersion;

    /// `InstanceID == "Build Number"`.
    QString buildNumber;

    /// `InstanceID == "Sku"` — typically a numeric bitmask string;
    /// the controller decodes it into "Full AMT" vs "ISM" for display.
    QString sku;

    /// `InstanceID == "VendorID"`.
    QString vendorId;

    /// `InstanceID == "Flash"` — the SPI flash version, when reported.
    QString flash;

    /// Every `InstanceID → VersionString` pair returned by the firmware,
    /// in the order received. Lets the UI surface unknown / vendor-
    /// specific rows without code changes.
    QList<std::pair<QString, QString>> identities;
};

/// Per-redirection-channel enabled state. Decoded from
/// `AMT_RedirectionService` (Redir port + SOL + IDE-R) and
/// `CIM_KVMRedirectionSAP` (KVM, AMT > 5 only).
struct RedirectionStatusResult
{
    bool ok = false;
    QString error;
    /// `AMT_RedirectionService.ListenerEnabled` — the umbrella
    /// redirection listener on port 16994/16995.
    bool redirectionListenerEnabled = false;
    /// `AMT_RedirectionService.EnabledState & 2`.
    bool solEnabled = false;
    /// `AMT_RedirectionService.EnabledState & 1`.
    bool iderEnabled = false;
    /// `CIM_KVMRedirectionSAP.EnabledState == 2 || == 6`. False when
    /// the firmware doesn't expose the SAP (AMT 5 and earlier).
    bool kvmEnabled = false;
    /// `true` when we managed to read `CIM_KVMRedirectionSAP`. Lets the
    /// UI distinguish "KVM disabled" from "no KVM capability".
    bool kvmAvailable = false;
};

/// Decoded `AMT_BootCapabilities` snapshot. Each scalar is `true` when
/// the firmware reports the capability. The first few fields drive
/// existing power-menu visibility; the rest are surfaced as a read-only
/// pane (see #172) so operators can tell SKU-vs-user-error apart at a
/// glance.
struct BootCapabilitiesResult
{
    bool ok = false;
    QString error;
    bool ider = false;
    bool sol = false;
    bool biosReflash = false;
    bool biosSetup = false;
    bool biosPause = false;
    bool forcePxeBoot = false;
    bool forceHddBoot = false;
    bool forceCdOrDvdBoot = false;
    bool verbosityScreenBlank = false;
    bool powerButtonLock = false;
    bool resetButtonLock = false;
    bool keyboardLock = false;
    bool sleepButtonLock = false;
    bool userPasswordBypass = false;
    bool forcedProgressEvents = false;
    bool verbosityVerbose = false;
    bool verbosityQuiet = false;
    bool configurationDataReset = false;
    bool biosSecureBoot = false;
    bool secureErase = false;
    bool forceWinReBoot = false;
    bool forceUefiLocalPbaBoot = false;
    bool forceUefiHttpsBoot = false;
    bool amtSecureBootControl = false;
    /// `true` when the firmware advertises any Platform Erase support.
    bool platformErase = false;
    /// Raw bitmask AMT returns under `PlatformErase`. Bits:
    ///   1  — Pyrite Revert
    ///   2  — Secure Erase All SSDs
    ///   6  — TPM Clear
    ///   16 — OEM Custom Action
    ///   25 — Clear BIOS NVM Variables
    ///   26 — BIOS Reload of Golden Configuration
    ///   31 — CSME Unconfigure
    quint32 platformEraseMask = 0;
};

struct InvokeResult
{
    bool ok = false;
    QString error;
    int returnValue = -1;        ///< Vendor-specific status code from the Invoke reply.
};

/// Snapshot of `IPS_OptInService` — drives the AMT user-consent flow.
struct OptInServiceResult
{
    bool ok = false;
    QString error;
    /// Policy: `true` when this machine requires user consent for
    /// redirection at all. Parsed from the firmware's `OptInRequired`,
    /// which derives from the per-redir policy flags.
    ///
    /// This is NOT a progress flag — it does not go false once the
    /// operator enters the code, and it stays true for the whole life
    /// of a consent-gated machine. Use `optInState` to decide whether
    /// consent has been *granted*; testing this field alone will
    /// re-request consent forever. See #433.
    bool optInRequired = false;
    /// `IPS_OptInService.OptInState`: 0=NotStarted, 1=Requested,
    /// 2=Displayed (code on screen, waiting), 3=Received,
    /// 4=InSession (consent already granted).
    ///
    /// Consent is satisfied at **3 or 4**. A successful `SendOptInCode`
    /// lands on 3 (Received); the firmware only advances to 4
    /// (InSession) once a redirection session is actually running, so
    /// code that waits for 4 before proceeding waits forever — and
    /// code that treats "not 4" as "ask again" revokes the consent the
    /// operator just gave. The legacy client gates on
    /// `state != 3 && state != 4` for exactly this reason.
    int optInState = 0;
    /// `true` when the current AMT login has the privilege to flip
    /// the policy. Determines whether the UI offers a Disable button.
    bool canModifyOptInPolicy = false;
    /// `IPS_KVMRedirectionSettingData.OptInPolicy` — `true` means
    /// KVM sessions require consent before the firmware unblocks the
    /// framebuffer.
    bool kvmOptInPolicy = false;
    /// `IPS_KVMRedirectionSettingData.OptInPolicyTimeout` — seconds the
    /// firmware will wait for the operator to enter the consent code
    /// before tearing the in-progress opt-in down. 0 when the firmware
    /// didn't expose it (older AMT). Drives the countdown in the
    /// PIN-entry dialog. See #171.
    int optInPolicyTimeoutSec = 0;

    /// `IPS_KVMRedirectionSettingData.Is5900PortEnabled` — VNC port
    /// listener toggle. Editable via `setKvmSettings` (#175).
    bool is5900PortEnabled = false;
    /// `IPS_KVMRedirectionSettingData.SessionTimeout` — minutes of idle
    /// before the firmware auto-closes a KVM session. 0 = no timeout.
    int sessionTimeoutMinutes = 0;
    /// `IPS_KVMRedirectionSettingData.GreyscalePixelFormatRequested` —
    /// hint to send 8-bit greyscale instead of RGB565 (bandwidth saver).
    /// QuMesh itself always negotiates RGB565, but legacy management
    /// tools may consume this.
    bool greyscalePixelFormatRequested = false;
};

/// Partial-update payload for `setKvmSettings` (#175). Each `set*` flag
/// gates whether the corresponding field is included in the Put.
/// Unset fields are echoed back verbatim from the previous Get so the
/// AMT validator stays happy.
struct KvmSettingsPatch
{
    bool setOptInPolicy = false;             bool optInPolicy = false;
    bool setIs5900PortEnabled = false;       bool is5900PortEnabled = false;
    bool setSessionTimeoutMinutes = false;   int  sessionTimeoutMinutes = 0;
    /// Pass an empty string to clear the password; non-empty to set it.
    /// The firmware never echoes the password back on Get; we only ever
    /// send it when `setRfbPassword` is true.
    bool setRfbPassword = false;             QString rfbPassword;
    bool setGreyscaleRequested = false;      bool greyscaleRequested = false;
};

/// Read `IPS_OptInService` + `IPS_KVMRedirectionSettingData` in one
/// helper. Two separate WSMAN Gets internally; results are merged into
/// the single struct above.
void getOptInStatus(WsmanClient *client,
                    std::function<void(OptInServiceResult)> callback);

/// Invoke `IPS_OptInService.StartOptIn` — AMT begins showing a 6-digit
/// consent code on the target's local screen. The operator reads it
/// and passes it back via `sendOptInCode`.
void startOptIn(WsmanClient *client,
                std::function<void(InvokeResult)> callback);

/// Invoke `IPS_OptInService.SendOptInCode(code)` — submits the
/// operator-entered code. On success the redir framebuffer / serial
/// unblocks.
void sendOptInCode(WsmanClient *client, quint32 code,
                   std::function<void(InvokeResult)> callback);

/// Invoke `IPS_OptInService.CancelOptIn` — abandons a pending consent
/// prompt. Safe to call from any state.
void cancelOptIn(WsmanClient *client,
                 std::function<void(InvokeResult)> callback);

/// Put `IPS_KVMRedirectionSettingData` with a new `OptInPolicy` value.
/// `policyRequired` true makes consent mandatory; false disables.
/// Fails (with a populated `error`) when the AMT login lacks the
/// realm needed to modify the policy.
void setKvmOptInPolicy(WsmanClient *client, bool policyRequired,
                       std::function<void(InvokeResult)> callback);

/// Apply a partial update to `IPS_KVMRedirectionSettingData`. The op
/// reads the current record, merges in whichever fields the patch
/// flags as set, and Puts it back — the firmware validator requires
/// the full record on every Put. See #175.
void setKvmSettings(WsmanClient *client, const KvmSettingsPatch &patch,
                    std::function<void(InvokeResult)> callback);

/// Invoke `CIM_KVMRedirectionSAP.RequestStateChange` to enable or
/// disable KVM at the device level. `RequestedState = 2` (Enabled) or
/// `3` (Disabled). Disabling KVM stops new sessions until re-enabled.
/// See #175.
void setKvmRedirectionEnabled(WsmanClient *client, bool enabled,
                              std::function<void(InvokeResult)> callback);

/// Send the DMTF `Identify` discovery message to the endpoint configured on
/// `client` and invoke `callback` exactly once with the result. Requires
/// no credentials; useful for connection sanity-checks.
void identify(WsmanClient *client, std::function<void(IdentifyResult)> callback);

/// Read `CIM_AssociatedPowerManagementService.PowerState` via WS-Transfer
/// Get. PowerState is a CIM enum: 2=On, 6=Off (Soft), 8=Off (Hard),
/// 13=Sleep, etc.
void getPowerState(WsmanClient *client, std::function<void(PowerStateResult)> callback);

/// Read `AMT_GeneralSettings` — hostname/domain/realm/feature toggles.
void getGeneralSettings(WsmanClient *client,
                        std::function<void(GeneralSettingsResult)> callback);

/// Read `CIM_ComputerSystem` — name, element name, UUID-like identifier.
void getComputerSystem(WsmanClient *client,
                       std::function<void(ComputerSystemResult)> callback);

/// Read `AMT_EthernetPortSettings` — MAC, DHCP/static IP, DNS, gateway.
/// AMT exposes two instances (LAN + Wireless); we request the wired one
/// by default (`InstanceID=Intel(r) AMT Ethernet Port Settings 0`).
void getEthernetSettings(WsmanClient *client,
                         std::function<void(EthernetSettingsResult)> callback);

/// Read `AMT_TimeSynchronizationService` — current AMT clock value.
void getTimeSettings(WsmanClient *client,
                     std::function<void(TimeSettingsResult)> callback);

/// Invoke `AMT_TimeSynchronizationService.SetHighAccuracyTimeSynch` to
/// correct the AMT clock. The protocol is a 3-point exchange: the
/// caller first reads `Ta0` from `GetLowAccuracyTimeSynch`, records
/// `Tm1` (the host's clock at the moment the response arrived) and
/// `Tm2` (the host's clock at the moment of sending this Set), and
/// passes all three back. The firmware uses the difference to compute
/// its drift and adjusts. `ReturnValue == 0` means accepted. All three
/// timestamps are seconds-since-1970-UTC.
void setHighAccuracyTimeSync(WsmanClient *client,
                             qint64 ta0, qint64 tm1, qint64 tm2,
                             std::function<void(InvokeResult)> callback);

/// One `AMT_SystemPowerScheme` row.
struct PowerScheme
{
    QString instanceId;     ///< Selector value for `SetPowerScheme`.
    QString schemeGuid;     ///< Matches Windows power-scheme GUIDs.
    QString description;    ///< Localised; typically `"<num>:<label>"`.
};

/// Enumerated `AMT_SystemPowerScheme` rows plus the InstanceID of the
/// currently-active scheme (determined by walking
/// `CIM_ElementSettingData[IsCurrent=1]` for entries whose `SettingData`
/// EPR points back at `AMT_SystemPowerScheme`). `currentInstanceId` is
/// empty when the firmware didn't expose the association — the UI
/// should treat that as "unknown" rather than "none active."
struct PowerSchemesResult
{
    bool ok = false;
    QString error;
    QList<PowerScheme> schemes;
    QString currentInstanceId;
};

/// Enumerate `AMT_SystemPowerScheme` and resolve which one is current
/// via `CIM_ElementSettingData`. See #162.
void getPowerSchemes(WsmanClient *client,
                     std::function<void(PowerSchemesResult)> callback);

/// Invoke `AMT_SystemPowerScheme.SetPowerScheme` to activate
/// `instanceId`. The InstanceID lands in the selector set with no
/// method-input fields.
void setPowerScheme(WsmanClient *client, const QString &instanceId,
                    std::function<void(InvokeResult)> callback);

/// One `AMT_AgentPresenceWatchdog` row.
struct AgentPresenceWatchdog
{
    /// `DeviceID` in the firmware arrives as a base-64-encoded 16-byte
    /// GUID; surfaced here as the formatted GUID string.
    QString deviceIdGuid;
    QString description;            ///< `MonitoredEntityDescription` — empty when unset.
    int monitoredEntityCode = -1;   ///< Raw `MonitoredEntity` enum.
    QString monitoredEntityLabel;   ///< Localised label for `monitoredEntityCode`.
    int currentStateCode = -1;      ///< Raw `CurrentState` enum.
    QString currentStateLabel;
    int enabledStateCode = -1;      ///< Raw `EnabledState` enum.
    QString enabledStateLabel;
    int startupIntervalSec = 0;
    int timeoutIntervalSec = 0;
};

/// One `AMT_AgentPresenceWatchdogAction` row. Tied to its parent
/// `AMT_AgentPresenceWatchdog` by `watchdogDeviceIdGuid`. The triple
/// `(watchdogDeviceIdGuid, oldState, newState)` is the CIM key set
/// the firmware uses to address the row on Delete. See #350.
struct AgentPresenceWatchdogAction
{
    /// Parent watchdog's DeviceID — the formatted 8-4-4-4-12 GUID
    /// (the read side decoded from base-64 on the way in).
    QString watchdogDeviceIdGuid;
    int oldState = -1;                ///< Raw watchdog state code (1/2/4/8/16).
    QString oldStateLabel;
    int newState = -1;                ///< Raw watchdog state code (1/2/4/8/16).
    QString newStateLabel;
    bool eventOnTransition = false;
    int actionSac = -1;               ///< System Action Code.
    QString actionSacLabel;
    int actionEac = -1;               ///< Extended Action Code (-1 = absent).
};

/// Snapshot of `AMT_AgentPresenceCapabilities` (the per-firmware
/// max-watchdog / max-action ceiling) plus every
/// `AMT_AgentPresenceWatchdog` row and the
/// `AMT_AgentPresenceWatchdogAction` rows currently attached to them.
/// See #164 (read) + #350 (actions).
struct AgentPresenceResult
{
    bool ok = false;
    QString error;
    int maxTotalAgents = 0;
    int maxTotalActions = 0;
    QList<AgentPresenceWatchdog> watchdogs;
    QList<AgentPresenceWatchdogAction> actions;
};

/// Enumerate `AMT_AgentPresenceWatchdog` and read
/// `AMT_AgentPresenceCapabilities`. See #164.
void getAgentPresence(WsmanClient *client,
                      std::function<void(AgentPresenceResult)> callback);

/// Invoke `AMT_AgentPresenceService.RegisterAgent` with an embedded
/// `AMT_AgentPresenceWatchdog`. `watchdog.deviceIdGuid` is the GUID
/// string the dialog generates (or the user types); the builder
/// packs it back into the base-64 raw-16-bytes wire format. The
/// read-only state fields on the struct are ignored. See #348.
void registerWatchdogAgent(WsmanClient *client,
                            const AgentPresenceWatchdog &watchdog,
                            std::function<void(InvokeResult)> callback);

/// Test-only seam: hand back the bytes the `RegisterAgent` envelope
/// builder would send for `watchdog`, without round-tripping through
/// a `WsmanClient`. Used by `wsman/tests/test_soap_envelope.cpp` to
/// lock in the on-wire shape.
[[nodiscard]] QByteArray buildRegisterAgentEnvelopeForTesting(
    const AgentPresenceWatchdog &watchdog,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:register-agent-test"));

/// WS-Transfer Delete on `AMT_AgentPresenceWatchdog` keyed by
/// `DeviceID` (base-64-encoded raw 16 GUID bytes). See #348.
void deleteAgentPresenceWatchdog(WsmanClient *client,
                                   const QString &deviceIdGuid,
                                   std::function<void(InvokeResult)> callback);

/// Invoke `AMT_AgentPresenceService.AddAction` to attach an action
/// to the watchdog identified by `watchdogDeviceIdGuid`. The
/// `Watchdog` parameter is an endpoint reference (EPR) into
/// `AMT_AgentPresenceWatchdog`, so the envelope is hand-built (the
/// flat-scalar `buildInvokeEnvelope` helper can't carry EPRs).
///
/// `oldState` / `newState` are the watchdog state-machine codes
/// (1=Not Started, 2=Stopped, 4=Running, 8=Expired, 16=Suspended);
/// the action fires when the watchdog transitions from `oldState`
/// to `newState`. `actionSac` is the AMT System Action Code (1=Alert,
/// 2=Event Log, 16=Power off, 17=Reset, 18=Power cycle). Pass
/// `actionEac < 0` to omit the optional Extended Action Code. See #350.
void addAgentPresenceWatchdogAction(WsmanClient *client,
                                     const QString &watchdogDeviceIdGuid,
                                     int oldState, int newState,
                                     bool eventOnTransition,
                                     int actionSac, int actionEac,
                                     std::function<void(InvokeResult)> callback);

/// Test-only seam: hand back the bytes the `AddAction` envelope
/// builder would send. See `buildRegisterAgentEnvelopeForTesting`.
[[nodiscard]] QByteArray buildAddActionEnvelopeForTesting(
    const QString &watchdogDeviceIdGuid,
    int oldState, int newState,
    bool eventOnTransition,
    int actionSac, int actionEac,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:add-action-test"));

/// WS-Transfer Delete on `AMT_AgentPresenceWatchdogAction`. The
/// firmware uses `(Watchdog, OldState, NewState)` as the CIM key
/// set for the class — Watchdog is the base-64-encoded DeviceID of
/// the parent watchdog. See #350.
void deleteAgentPresenceWatchdogAction(WsmanClient *client,
                                        const QString &watchdogDeviceIdGuid,
                                        int oldState, int newState,
                                        std::function<void(InvokeResult)> callback);

/// One row from `CIM_FilterCollection` — a named event-filter the
/// firmware exposes; subscriptions reference it by `instanceId`.
struct EventFilter
{
    QString instanceId;
    QString collectionName;
};

/// One row from `CIM_ListenerDestination(WSManagement)` — a sink the
/// firmware can push event notifications to.
struct EventListener
{
    QString name;
    QString destination;       ///< Listener URL.
    int deliveryMode = -1;     ///< 2=Push, 3=Push+ACK, 4=Events, 5=Pull.
    QString deliveryModeLabel; ///< Human-readable form of `deliveryMode`.
};

/// One row from `CIM_FilterCollectionSubscription` — the join between
/// a filter and a listener. `Filter` and `Handler` are EPRs; the
/// `InstanceID` / `Name` selectors are extracted into named fields.
struct EventSubscription
{
    QString filterInstanceId;
    QString listenerName;
};

/// Snapshot of event subscriptions and the catalogs they pull from.
/// Read-only Phase A (#163) — write side (Subscribe / UnSubscribe)
/// deferred to Phase B.
struct EventSubscriptionsResult
{
    bool ok = false;
    QString error;
    QList<EventFilter> filters;
    QList<EventListener> listeners;
    QList<EventSubscription> subscriptions;
};

/// Enumerate `CIM_FilterCollection`, `CIM_ListenerDestination`, and
/// `CIM_FilterCollectionSubscription` in parallel. See #163.
void getEventSubscriptions(WsmanClient *client,
                           std::function<void(EventSubscriptionsResult)> callback);

/// Wire-level delivery mode for `subscribeToEventFilter`. AMT supports
/// `Push` (fire-and-forget) and `PushWithAck` (firmware retries until
/// the listener acknowledges). See #345.
enum class EventDeliveryMode { Push, PushWithAck };

/// Invoke WS-Eventing Subscribe with ResourceURI `CIM_FilterCollection`
/// and the chosen filter as the `InstanceID` selector. `notifyUrl` is
/// the listener-side endpoint AMT pushes notifications to. If `user`
/// or `pass` is set, both are embedded as a WS-Trust UsernameToken so
/// the firmware can forward HTTP basic auth to the listener. See #345.
void subscribeToEventFilter(WsmanClient *client,
                              const QString &filterInstanceId,
                              EventDeliveryMode deliveryMode,
                              const QString &notifyUrl,
                              const QString &user,
                              const QString &pass,
                              std::function<void(InvokeResult)> callback);

/// Test-only seam for `subscribeToEventFilter`: hand back the bytes
/// the envelope builder would send, without round-tripping through a
/// `WsmanClient`. Used to lock in the on-wire shape.
[[nodiscard]] QByteArray buildSubscribeEnvelopeForTesting(
    const QString &filterInstanceId,
    EventDeliveryMode deliveryMode,
    const QString &notifyUrl,
    const QString &user,
    const QString &pass,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:subscribe-test"));

/// Invoke WS-Eventing Unsubscribe with ResourceURI
/// `CIM_FilterCollectionSubscription`. The two selectors are
/// EPR-valued (not scalars): a Filter EPR pointing at the
/// `CIM_FilterCollection` row by `InstanceID`, and a Handler EPR
/// pointing at the `CIM_ListenerDestinationWSManagement` row by the
/// standard 4-key tuple with `Name = listenerName`. See #345.
void unsubscribeFromEventFilter(WsmanClient *client,
                                  const QString &filterInstanceId,
                                  const QString &listenerName,
                                  std::function<void(InvokeResult)> callback);

/// Test-only seam for `unsubscribeFromEventFilter`.
[[nodiscard]] QByteArray buildUnsubscribeEnvelopeForTesting(
    const QString &filterInstanceId,
    const QString &listenerName,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:unsubscribe-test"));

/// Render a `CIM_ListenerDestination.DeliveryMode` enum into a label.
[[nodiscard]] QString listenerDeliveryModeLabel(int code);

/// One `IPS_AlarmClockOccurrence` row — a scheduled wake-on-alarm.
/// The `startTime` / `interval` are arrives wrapped in nested
/// `<Datetime>` / `<Interval>` elements and are surfaced here as raw
/// ISO-8601 strings (the controller pre-formats them for display).
struct WakeAlarm
{
    QString instanceId;
    QString elementName;
    QString startTimeIso;        ///< ISO-8601 timestamp.
    QString intervalIso;         ///< ISO-8601 duration (`PnDTnHnM`); empty if non-recurring.
    bool deleteOnCompletion = false;
};

/// Snapshot of all `IPS_AlarmClockOccurrence` rows. See #161 phase A.
struct WakeAlarmsResult
{
    bool ok = false;
    QString error;
    QList<WakeAlarm> alarms;
};

/// Enumerate `IPS_AlarmClockOccurrence`. Older firmware (pre-AMT-8)
/// doesn't expose the class and the result will be empty.
void getWakeAlarms(WsmanClient *client,
                   std::function<void(WakeAlarmsResult)> callback);

/// Invoke `AMT_AlarmClockService.AddAlarm` with an embedded
/// `IPS_AlarmClockOccurrence`. `alarm.instanceId` is ignored — the
/// firmware mints the new key from `elementName`. `intervalIso` may
/// be empty for a one-shot alarm. See #347.
void addWakeAlarm(WsmanClient *client, const WakeAlarm &alarm,
                  std::function<void(InvokeResult)> callback);

/// Test-only seam: hand back the bytes the `AddAlarm` envelope
/// builder would send for `alarm`, without round-tripping through a
/// `WsmanClient`. Used by `wsman/tests/test_soap_envelope.cpp` to
/// lock in the on-wire shape.
[[nodiscard]] QByteArray buildAddAlarmEnvelopeForTesting(
    const WakeAlarm &alarm,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:alarm-test"));

/// WS-Transfer Delete on `IPS_AlarmClockOccurrence` keyed by
/// `InstanceID`. See #347.
void deleteWakeAlarm(WsmanClient *client, const QString &instanceId,
                     std::function<void(InvokeResult)> callback);

/// Which WSMAN operation the browser tool (#167) should fire.
enum class BrowseKind { Get, Enumerate };

/// Output of `executeBrowse` — the raw response body. For `Enumerate`
/// the per-item XML from each `Pull` is concatenated under a synthetic
/// `<Items>` wrapper so the caller can show the whole walk at once.
struct WsmanBrowseResult
{
    bool ok = false;
    QString error;
    BrowseKind kind = BrowseKind::Get;
    QByteArray xml;          ///< Raw response body (or merged pull items).
    int itemCount = 0;       ///< For `Enumerate`; 0 otherwise.
};

/// Free-form WSMAN browser (#167). Auto-prefixes bare class names:
///   - `AMT_*` → `http://intel.com/wbem/wscim/1/amt-schema/1/<name>`
///   - `IPS_*` → `http://intel.com/wbem/wscim/1/ips-schema/1/<name>`
///   - `CIM_*` → `http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/<name>`
/// otherwise `classOrUri` is used as-is. `selectors` apply only when
/// `kind == Get` (Enumerate ignores them).
void executeBrowse(WsmanClient *client, const QString &classOrUri,
                   BrowseKind kind,
                   const QHash<QString, QString> &selectors,
                   std::function<void(WsmanBrowseResult)> callback);

/// One `AMT_SystemDefensePolicy` row — a named filter set with a
/// priority and direction flags. The actual filter contents live in
/// the AMT_Hdr8021Filter / AMT_IPHeadersFilter classes.
struct SystemDefensePolicy
{
    QString instanceId;
    QString policyName;
    int priority = 0;
    /// Bit 0 of the legacy `AntiSpoofingSupport`-style flag word —
    /// `true` when the policy transmits on the wire (the firmware uses
    /// the term `tx_enabled` in the schema docs).
    bool txEnabled = false;
    bool rxEnabled = false;
    /// Default policy that catches all otherwise-unmatched packets.
    bool defaultPolicy = false;
    /// Create-side only — per-direction default actions on a packet
    /// that doesn't match any contained filter. AMT splits this into
    /// six independent flags (legacy MeshCommander reference). See #357.
    bool txDefaultCount = false;
    bool txDefaultDrop = false;
    bool txDefaultMatchEvent = false;
    bool rxDefaultCount = false;
    bool rxDefaultDrop = false;
    bool rxDefaultMatchEvent = false;
    /// Create-side only — integer handles of the L2 / L3 filters
    /// participating in this policy. Each handle is the trailing
    /// integer of a filter's `InstanceID`. Serialised on the wire as
    /// a sequence of repeated `<r:FilterCreationHandles>` elements.
    QList<int> filterCreationHandles;
};

/// One `AMT_Hdr8021Filter` (L2) row — VLAN / ethertype / 802.1 priority.
struct Hdr8021Filter
{
    QString instanceId;
    QString name;
    int filterDirection = -1;       ///< 0=tx (out), 1=rx (in).
    int vlanTag = -1;               ///< -1 = not set.
    /// AMT field name on the wire is `HdrProtocolID8021`; 2048=IP,
    /// 2054=ARP. See #353.
    int etherType = -1;
    int priority = -1;
    /// Create-side only. 0=Allow+Count, 1=Drop+Count, 2=Rate Limit,
    /// 3=Allow, 4=Drop. -1 on the read path (firmware doesn't echo
    /// it back consistently per AMT version).
    int filterProfile = -1;
    /// Create-side only — packets/second cap for `filterProfile == 2`.
    int filterProfileData = 0;
    /// Create-side only — fire an event on match.
    bool actionEventOnMatch = false;
};

/// One `AMT_IPHeadersFilter` (L3/L4) row.
struct IpHeadersFilter
{
    QString instanceId;
    QString name;
    int filterDirection = -1;       ///< 0=tx (out), 1=rx (in).
    /// Hex string on the wire (`HdrSrcAddress`). Read side surfaces
    /// it verbatim; write side rebuilds from dotted-quad / colon-hex.
    QString srcAddress;
    QString dstAddress;
    int protocol = -1;              ///< IPv4 protocol number (`HdrProtocolID8`); -1 if any.
    int srcPort = -1;               ///< `HdrSrcPortStart`.
    int dstPort = -1;               ///< `HdrDestPortStart`.
    /// Create-side only — IP version. 4 = IPv4, 6 = IPv6. Read side
    /// leaves at -1. See #358.
    int ipVersion = -1;
    /// Create-side only — see Hdr8021Filter.
    int filterProfile = -1;
    int filterProfileData = 0;
    bool actionEventOnMatch = false;
    /// Create-side only — `HdrSrcPortEnd` / `HdrDestPortEnd`. -1
    /// reuses the start value for a single-port match.
    int srcPortEnd = -1;
    int dstPortEnd = -1;
};

/// One row from `AMT_ActiveFilterStatistics` — packets-passed /
/// packets-dropped counters for a single filter, keyed by the same
/// `InstanceID` that identifies the filter row in
/// `AMT_Hdr8021Filter` / `AMT_IPHeadersFilter`. See #346.
struct ActiveFilterStatRow
{
    QString filterInstanceId;
    quint64 packetsPassed = 0;
    quint64 packetsDropped = 0;
};

/// One row from `AMT_NetworkPortSystemDefensePolicy` — the join
/// linking a `CIM_EthernetPort` (Antecedent) to an
/// `AMT_SystemDefensePolicy` (Dependent). The read side surfaces
/// just the two scalar keys; the controller rebuilds the EPRs when
/// the operator changes the binding. See #359.
struct PortPolicyBinding
{
    /// Ethernet port DeviceID as AMT reports it, e.g.
    /// `"Intel(r) AMT Ethernet Port 0"`.
    QString portDeviceId;
    QString policyInstanceId;
};

/// Snapshot of the System Defense classes (#165 phase A). ACM-only —
/// the controller decides visibility off `provisioningMode`. `supported`
/// is `false` when the firmware doesn't expose the classes at all
/// (ISM SKUs / older AMT).
struct SystemDefenseResult
{
    bool ok = false;
    QString error;
    bool supported = true;
    QList<SystemDefensePolicy> policies;
    QList<Hdr8021Filter>       hdrFilters;
    QList<IpHeadersFilter>     ipFilters;
    /// #359 — port → policy bindings. Empty if no port has a policy
    /// activated on it (the firmware's "no defense" default).
    QList<PortPolicyBinding>   portBindings;
    /// #346 — live per-filter counters. Empty when the firmware
    /// rejects the AMT_ActiveFilterStatistics enumeration (older
    /// boxes that expose the policy/filter classes but not the
    /// statistics class).
    QList<ActiveFilterStatRow> stats;
};

/// Enumerate the System Defense classes. See #165 phase A.
void getSystemDefense(WsmanClient *client,
                      std::function<void(SystemDefenseResult)> callback);

/// Enumerate `AMT_ActiveFilterStatistics` only. Used by the section
/// when the operator clicks "Refresh stats" to repoll counters
/// without bouncing the whole policy/filter tree. See #346.
void getActiveFilterStatistics(WsmanClient *client,
                                std::function<void(QList<ActiveFilterStatRow>, QString /*err*/)> callback);

/// WS-Transfer Delete on `AMT_SystemDefensePolicy` keyed by
/// `InstanceID`. AMT cascades any `AMT_NetworkPortSystemDefensePolicy`
/// bindings that referenced the policy. See #346.
void deleteSystemDefensePolicy(WsmanClient *client, const QString &instanceId,
                                 std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on `AMT_Hdr8021Filter` keyed by `InstanceID`.
/// See #346.
void deleteHdr8021Filter(WsmanClient *client, const QString &instanceId,
                          std::function<void(InvokeResult)> callback);

/// WS-Transfer Create of a new `AMT_Hdr8021Filter`. The firmware
/// assigns InstanceID + the three creation-class fields; the caller
/// only fills name / direction / ethertype / filterProfile and
/// (optionally) the rate-limit data. See #353.
void addHdr8021Filter(WsmanClient *client, const Hdr8021Filter &filter,
                       std::function<void(InvokeResult)> callback);

/// Test-only seam for `addHdr8021Filter`.
[[nodiscard]] QByteArray buildAddHdr8021FilterEnvelopeForTesting(
    const Hdr8021Filter &filter,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:add-hdr8021-test"));

/// WS-Transfer Create of a new `AMT_IPHeadersFilter`. IP version,
/// protocol, address, and port fields are all optional — any
/// unspecified field is omitted from the wire so AMT's strict-mode
/// validation doesn't reject it. The struct's address fields
/// (`srcAddress` / `dstAddress`) accept dotted-quad / colon-hex
/// input; the builder converts them to the wire hex form. See #358.
void addIpHeadersFilter(WsmanClient *client, const IpHeadersFilter &filter,
                         std::function<void(InvokeResult)> callback);

/// Test-only seam for `addIpHeadersFilter`.
[[nodiscard]] QByteArray buildAddIpHeadersFilterEnvelopeForTesting(
    const IpHeadersFilter &filter,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:add-ipheaders-test"));

/// WS-Transfer Create of a new `AMT_SystemDefensePolicy`. Wraps an
/// arbitrary number of `FilterCreationHandles` integer references
/// into a single envelope (one repeated element per handle), with
/// six independent Tx/Rx default-action flags. See #357.
void addSystemDefensePolicy(WsmanClient *client,
                              const SystemDefensePolicy &policy,
                              std::function<void(InvokeResult)> callback);

/// Test-only seam for `addSystemDefensePolicy`.
[[nodiscard]] QByteArray buildAddSystemDefensePolicyEnvelopeForTesting(
    const SystemDefensePolicy &policy,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:add-sd-policy-test"));

/// WS-Transfer Create of a new `AMT_NetworkPortSystemDefensePolicy`
/// row, binding the policy with `policyInstanceId` to the Ethernet
/// port indexed by `portIndex` (the firmware naming convention is
/// `"Intel(r) AMT Ethernet Port <portIndex>"`). Both Antecedent and
/// Dependent are EPRs embedded in the Create body. See #359.
void bindSystemDefensePolicyToPort(WsmanClient *client,
                                      int portIndex,
                                      const QString &policyInstanceId,
                                      std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on `AMT_NetworkPortSystemDefensePolicy` keyed
/// by EPR-valued `Antecedent` + `Dependent` selectors. Same EPR
/// shape as the Create — the join row is identified by both
/// endpoints. See #359.
void unbindSystemDefensePolicyFromPort(WsmanClient *client,
                                          int portIndex,
                                          const QString &policyInstanceId,
                                          std::function<void(InvokeResult)> callback);

/// Test-only seams for the bind / unbind envelopes.
[[nodiscard]] QByteArray buildBindSystemDefensePolicyEnvelopeForTesting(
    int portIndex, const QString &policyInstanceId,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:bind-sd-test"));
[[nodiscard]] QByteArray buildUnbindSystemDefensePolicyEnvelopeForTesting(
    int portIndex, const QString &policyInstanceId,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:unbind-sd-test"));

/// Extract the trailing integer handle from a System Defense filter's
/// `InstanceID` (e.g. `"Intel(r) AMT:IP Filter Set:Handle 5"` → `5`,
/// `"Intel(r) AMT:Hdr 8021 Filter Set 7"` → `7`). Returns -1 if no
/// trailing digits are present. Exposed for tests + the policy
/// dialog's filter-picker. See #357.
[[nodiscard]] int extractFilterHandleFromInstanceId(const QString &instanceId);

/// Encode a dotted-quad IPv4 address (e.g. `10.0.0.5`) into the
/// 8-char uppercase hex form AMT puts on the wire (e.g. `0A000005`).
/// Returns an empty string on malformed input. Exposed in the
/// header so the tests can lock the round-trip; production code
/// only ever calls `addIpHeadersFilter`. See #358.
[[nodiscard]] QString encodeIPv4ForFilter(const QString &dottedQuad);

/// WS-Transfer Delete on `AMT_IPHeadersFilter` keyed by `InstanceID`.
/// See #346.
void deleteIpHeadersFilter(WsmanClient *client, const QString &instanceId,
                            std::function<void(InvokeResult)> callback);

/// Read `AMT_BootCapabilities` — which boot-source-override flags the
/// firmware will accept. Drives the gating of menu entries
/// (Secure Erase / Platform Erase / HTTPS Boot etc.).
void getBootCapabilities(WsmanClient *client,
                         std::function<void(BootCapabilitiesResult)> callback);

/// Read `AMT_SetupAndConfigurationService` — provisioning state +
/// mode. Used to render the "Activated in ACM/CCM" label on the
/// Overview pane.
void getSetupAndConfiguration(WsmanClient *client,
                              std::function<void(SetupAndConfigResult)> callback);

/// Enumerate `CIM_SoftwareIdentity` and pick out the instance whose
/// `InstanceID` is `"AMT"` — that's the Intel ME firmware version.
void getMeVersion(WsmanClient *client,
                  std::function<void(MeVersionResult)> callback);

/// Read `AMT_RedirectionService` and `CIM_KVMRedirectionSAP` and merge
/// into a single result. Mirrors `getOptInStatus` — two Gets, soft
/// failure on the KVM SAP (older firmware doesn't expose it).
void getRedirectionStatus(WsmanClient *client,
                          std::function<void(RedirectionStatusResult)> callback);

/// Invoke `CIM_PowerManagementService.RequestPowerStateChange`. `powerState`
/// is the CIM target enum: 2=On, 8=Off (Hard), 5=Reset, 4=Sleep,
/// 12=Off-Soft-Graceful, 13=Off-Hard-Graceful, etc.
void requestPowerStateChange(WsmanClient *client, int powerState,
                             std::function<void(InvokeResult)> callback);

/// Invoke `IPS_PowerManagementService.RequestOSPowerStateChange`. AMT
/// 10+ only — older firmware doesn't expose this service. `osPowerState`
/// uses 2 = wake from sleep / OS resume, 3 = put OS to sleep.
void requestOsPowerStateChange(WsmanClient *client, int osPowerState,
                               std::function<void(InvokeResult)> callback);

/// Inputs to a single power-to-X action. Set by the controller from
/// the per-action presets and consumed by `performBootAction` below.
struct BootActionParams
{
    /// CIM RequestPowerStateChange code — 2 = power on, 10 = reset
    /// graceful, 5 = reset immediate, etc.
    int targetPowerState = 2;

    /// AMT-side InstanceID of the CIM_BootSourceSetting to use (the
    /// "Intel(r) AMT: " prefix is added by the codec). Empty string
    /// means "no boot-source override" — used for BIOS-Setup-only.
    QString amtBootSource;

    /// AMT_BootSettingData fields. AMT requires us to send the entire
    /// boot setting record on Put; the firmware defaults the rest if
    /// we omit them, so we only set the bits the per-action preset
    /// cares about.
    bool biosSetup = false;
    bool biosPause = false;
    bool useIder = false;
    /// 0 = floppy / 1 = CD-ROM in the IDE-R bus.
    int iderBootDevice = 0;
    bool useSol = false;
    bool secureErase = false;
    /// AMT RSE (Remote Secure Erase) password — required when
    /// `secureErase = true` and the firmware enforces it.
    QString rsePassword;

    /// Platform Erase ("Remote Platform Erase"). When set, the chain
    /// includes `PlatformErase = true` plus the base64-encoded
    /// `UefiBootParametersArray` TLV blob built by
    /// `buildPlatformEraseTlv()` and the matching parameter count.
    bool platformErase = false;
    QString platformEraseTlvBase64;
    int platformEraseTlvCount = 0;

    /// HTTPS Boot via URL (legacy bootSourceIndex == 7). When set, the
    /// chain selects "Force OCR UEFI HTTPS Boot" and the Put writes
    /// a base64 TLV blob built from `httpsBootUrl` plus the count.
    bool httpsBootUrl = false;
    QString httpsBootUrlStr;

    /// One-Click Recovery (#170). When `oneClickRecovery == true` the
    /// chain attaches `ocrTlvBase64` + `ocrTlvCount` to the
    /// `AMT_BootSettingData` Put the same way the platform-erase /
    /// UEFI HTTPS Boot branches do, and the caller has already set
    /// `amtBootSource` to the right `Force OCR UEFI ...` instance.
    /// Build the TLV with `buildOcrPbaBootTlv` (PBA / WinRE) or
    /// `buildOcrHttpsBootPinnedTlv` (HTTPS recovery image).
    bool oneClickRecovery = false;
    QString ocrTlvBase64;
    int ocrTlvCount = 0;
};

/// Build the `UefiBootParametersArray` TLV blob for a Platform Erase
/// request. Format (matches the legacy NW.js port's `makeUefiBootParam`):
///
///   * Header entry  — vendor 0x8086, type 1, 4 bytes = `flags` bitmask
///   * Pyrite Revert (bit 1) → vendor 0x8086, type 10, value = `psid` bytes
///   * SSD Erase    (bit 2) → vendor 0x8086, type 20, value = `ssdPassword` bytes
///
/// `*tlvCount` is populated with the number of entries we emitted; the
/// caller assigns this to `BootActionParams::platformEraseTlvCount`.
[[nodiscard]] QByteArray buildPlatformEraseTlv(quint32 flags, const QString &psid,
                                                const QString &ssdPassword,
                                                int *tlvCount);

/// Build the `UefiBootParametersArray` TLV blob for an OCR HTTPS-Boot
/// request against a pre-hosted ISO at `url`. Same TLV framing as the
/// platform-erase builder. Three entries:
///   * type 1  — URL bytes
///   * type 20 — 1-byte "sync root CA" flag (1)
///   * type 30 — 2-byte HTTPS request timeout (0 = firmware default)
[[nodiscard]] QByteArray buildHttpsBootUrlTlv(const QString &url, int *tlvCount);

/// One row of `CIM_BootSourceSetting`. AMT enumerates per-source rows
/// for every BIOS-registered firmware boot option; the OCR-specific
/// rows have InstanceIDs of the form `Intel(r) AMT: Force OCR UEFI
/// Boot Option N`, with a `BIOSBootString` the firmware uses to
/// identify which OCR sub-target (PBA, WinRE) the row references.
struct BootSourceSetting
{
    QString instanceId;
    QString elementName;
    /// The firmware-registered string AMT uses to address this boot
    /// option in the OCR TLV (`OCR_EFI_FILE_DEVICE_PATH`).
    QString bootString;
    /// Friendly label set by BIOS — usually contains "WinRe" for the
    /// Windows Recovery row.
    QString biosBootString;
    QString structuredBootString;
};

struct BootSourceSettingsResult
{
    bool ok = false;
    QString error;
    QList<BootSourceSetting> sources;
};

/// Enumerate `CIM_BootSourceSetting`. Used by the One-Click Recovery
/// flow (#170) to find the InstanceID + BootString of the PBA / WinRE
/// boot option the operator wants to trigger.
void enumerateBootSourceSettings(WsmanClient *client,
                                  std::function<void(BootSourceSettingsResult)> callback);

/// One-Click Recovery (#170) parameter-type table — values match
/// Intel's `bootOptionsValidator.ts` in the open-amt-cloud-toolkit
/// MPS reference. Public so tests can assert by name.
enum class OcrTlvType : quint16
{
    EfiNetworkDevicePath        = 1,
    EfiFileDevicePath           = 2,
    EfiDevicePathLen            = 3,
    BootImageHashSha256         = 4,
    BootImageHashSha384         = 5,
    BootImageHashSha512         = 6,
    EfiBootOptionalData         = 10,
    HttpsCertSyncRootCa         = 20,
    HttpsCertServerName         = 21,
    HttpsServerNameVerifyMethod = 22,
    HttpsServerCertHashSha256   = 23,
    HttpsServerCertHashSha384   = 24,
    HttpsServerCertHashSha512   = 25,
    HttpsRequestTimeout         = 30,
    HttpsUserName               = 40,
    HttpsPassword               = 41,
};

/// Build the OCR PBA / WinRE TLV — `OCR_EFI_FILE_DEVICE_PATH` (the
/// firmware-registered BootString) plus `OCR_EFI_DEVICE_PATH_LEN`
/// (the BootString length as little-endian u16). `*tlvCount` returns
/// the number of parameters written (always 2). See #170.
[[nodiscard]] QByteArray buildOcrPbaBootTlv(const QString &bootString, int *tlvCount);

/// Build the OCR HTTPS-Boot TLV with optional cert + image-hash
/// pinning. Fields:
///   * type 1                — URL bytes
///   * type 4/5/6            — boot image hash (alg picks the type)
///   * type 20               — sync root CA flag (always 1)
///   * type 23/24/25         — server cert hash pin (alg picks)
///   * type 40 / 41          — HTTPS basic-auth username / password
///
/// `hashAlg` is `"sha256"`, `"sha384"`, or `"sha512"`; an empty string
/// skips the image-hash entry. Likewise for `pinnedServerCertHash*`.
/// `username` / `password` are optional. See #170.
[[nodiscard]] QByteArray buildOcrHttpsBootPinnedTlv(
    const QString &url,
    const QString &hashAlg,
    const QByteArray &imageHash,
    const QString &pinnedServerCertHashAlg,
    const QByteArray &pinnedServerCertHash,
    const QString &username,
    const QString &password,
    int *tlvCount);

/// One processor — `CIM_Processor` zipped with `CIM_Chip` by index.
struct HardwareCpu
{
    QString manufacturer;   ///< `CIM_Chip.Manufacturer`
    QString version;        ///< `CIM_Chip.Version` (model number)
    int family = -1;        ///< `CIM_Processor.Family` (DMTF code)
    QString familyLabel;    ///< Decoded `Family` (or "Family 0xNN" for unknowns).
    int maxClockSpeedMhz = 0; ///< `CIM_Processor.MaxClockSpeed`
    int cpuStatus = -1;     ///< `CIM_Processor.CPUStatus`
    QString cpuStatusLabel;
};

/// One DIMM — `CIM_PhysicalMemory`.
struct HardwareDimm
{
    QString bankLabel;
    QString manufacturer;
    QString serialNumber;
    qint64  capacityBytes = 0;
    int     formFactor = -1;
    QString formFactorLabel;
    int     memoryType = -1;
    QString memoryTypeLabel;
    QString assetTag;       ///< `Tag`
    QString partNumber;
};

/// One storage device — `CIM_MediaAccessDevice` zipped with the
/// corresponding `CIM_PhysicalPackage` entry. The legacy implementation
/// shifts the package index by +1 because the first package entry is the
/// chassis itself.
struct HardwareStorage
{
    QString model;
    QString serialNumber;
    /// `CIM_MediaAccessDevice.MaxMediaSize` — units of 1000 bytes per
    /// the schema; convert to MB on render.
    qint64  maxMediaSizeKb = 0;
};

/// Battery — optional. Composed from `CIM_Battery` + the matching
/// `CIM_PhysicalPackage` where `PackageType == 11`.
struct HardwareBattery
{
    bool    present = false;
    QString deviceId;
    QString manufacturer;
    QString manufactureDate;
    QString serialNumber;
    int     chemistry = -1;
    QString chemistryLabel;
    qint64  designCapacityMwh = 0;
    qint64  designVoltageMv = 0;
    QString otherIdentifyingInfo;
};

/// Aggregate result of `getHardwareInventory`. Each section is filled
/// independently; partial failures leave the unfilled bits at their
/// defaults (empty string / -1 / empty list) and report success on the
/// outer struct unless every enumeration failed.
struct HardwareInventoryResult
{
    bool ok = false;
    QString error;

    // Platform (CIM_Chassis + CIM_SystemPackaging).
    QString platformModel;
    QString platformManufacturer;
    QString platformVersion;
    QString platformSerialNumber;
    QString platformSystemId;   ///< guid-formatted PlatformGUID

    // Baseboard (CIM_Card).
    QString baseboardManufacturer;
    QString baseboardModel;
    QString baseboardVersion;
    QString baseboardSerialNumber;
    QString baseboardAssetTag;
    bool    baseboardReplaceable = false;
    bool    baseboardCanBeFRUedKnown = false;

    // BIOS (CIM_BIOSElement).
    QString biosVendor;
    QString biosVersion;
    QString biosReleaseDate;

    QList<HardwareCpu>     processors;
    QList<HardwareDimm>    memoryModules;
    QList<HardwareStorage> storageDevices;
    HardwareBattery        battery;
};

/// Enumerate the ten hardware classes the legacy Commander's
/// `PullHardware` BatchEnums and stitch them into a single inventory
/// result. Empty / faulted classes are tolerated (older firmware,
/// desktops without battery).
void getHardwareInventory(WsmanClient *client,
                          std::function<void(HardwareInventoryResult)> callback);

/// State half of `AMT_AuditLog` — read via WS-Transfer Get.
struct AuditLogState
{
    bool ok = false;
    QString error;
    /// `AuditState` bitmask. Bit 0 = audit enabled; bit 1 = log locked;
    /// bit 2 = almost full; bit 3 = full; bit 4 = no signing key.
    int auditState = 0;
    int overwritePolicy = 0;        ///< 1 = wraps; 2 = never overwrites.
    int currentNumberOfRecords = 0;
    int percentageFree = 0;
    int maxAllowedAuditors = 0;
    int enabledState = 0;           ///< CIM EnabledState code.
};

/// One audit-log record, decoded from the base64-encoded binary blob
/// AMT returns in `ReadRecords.EventRecords[]`.
struct AuditLogEntry
{
    int auditAppId = 0;
    int eventId = 0;
    QString auditAppLabel;
    QString eventLabel;
    /// 0 = HTTP digest, 1 = Kerberos, 2 = Local, 3 = KVM Default Port.
    int initiatorType = -1;
    QString initiator;
    qint64 unixSeconds = 0;
    int mcLocationType = 0;
    QString netAddress;
    QByteArray ex;                  ///< Raw extended-data bytes.
};

struct AuditLogResult
{
    bool ok = false;
    QString error;
    QList<AuditLogEntry> entries;
};

/// Read `AMT_AuditLog` state (storage / enabled / lock / etc.).
void getAuditLogState(WsmanClient *client,
                      std::function<void(AuditLogState)> callback);

/// One installed certificate, decoded from `AMT_PublicKeyCertificate`.
/// Subject / Issuer arrive as RFC2253 DN strings — already broken down
/// into common slots by `splitDn`.
struct DeviceCertificate
{
    QString instanceId;
    /// Common Name; first label of the DN. Empty if missing.
    QString subjectCn;
    QString issuerCn;
    QString subjectRaw;          ///< Original Subject DN string.
    QString issuerRaw;           ///< Original Issuer DN string.
    bool trustedRoot = false;
    /// Decoded DER size in bytes — handy for the UI.
    int derSizeBytes = 0;
    QString x509Base64;          ///< Raw `X509Certificate` field, base64.
};

/// One installed key pair, from `AMT_PublicPrivateKeyPair`. We only
/// surface enough to detect orphans (pairs with no matching cert).
struct DeviceKeyPair
{
    QString instanceId;
    int derSizeBytes = 0;        ///< For the orphan list.
};

/// One row of `AMT_TLSSettingData`. AMT exposes two: the LMS (local)
/// instance and the remote-port (16993) instance.
struct TlsSettingsRow
{
    QString instanceId;
    bool enabled = false;
    bool mutualAuthentication = false;
    bool acceptNonSecureConnections = false;
    QStringList trustedCn;
};

/// Aggregate result of `getDeviceCertStore`.
struct DeviceCertResult
{
    bool ok = false;
    QString error;
    QList<DeviceCertificate> certificates;
    QList<DeviceKeyPair>     keyPairs;
    QList<TlsSettingsRow>    tlsSettings;
    /// InstanceIDs the `AMT_TLSCredentialContext` enumeration marks
    /// as the currently active TLS endpoint cert. Empty when no cert
    /// is bound (firmware on default self-signed certs).
    QStringList activeCertInstanceIds;
};

/// Split an RFC2253 DN like "CN=Intel(R) AMT, O=Intel" into a
/// QHash<key, value> map. Whitespace around values is trimmed. The
/// legacy `parseCertName` mirror.
[[nodiscard]] QHash<QString, QString> splitDn(const QString &dn);

/// Enumerate `AMT_PublicKeyCertificate`, `AMT_PublicPrivateKeyPair`,
/// `AMT_TLSSettingData`, and `AMT_TLSCredentialContext` in parallel
/// and stitch into a single result. Tolerates per-class faults — a
/// faulted enumeration just leaves its list empty.
void getDeviceCertStore(WsmanClient *client,
                        std::function<void(DeviceCertResult)> callback);

/// Invoke `AMT_PublicKeyManagementService.AddTrustedRootCertificate`.
/// `certB64` is the cert's DER encoding, base64-encoded. AMT installs
/// it under the trusted-root list and returns a new
/// `AMT_PublicKeyCertificate` instance the caller can locate on the
/// next re-enumeration. See #157.
void addTrustedRootCertificate(WsmanClient *client, const QString &certB64,
                                std::function<void(InvokeResult)> callback);

/// Invoke `AMT_PublicKeyManagementService.AddCertificate`. Same shape
/// as the trusted-root variant but the cert is added to the
/// chain / intermediate-CA list rather than marked as a trust anchor.
/// See #157.
void addCertificate(WsmanClient *client, const QString &certB64,
                    std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on `AMT_PublicKeyCertificate` with the
/// `InstanceID` selector. The caller must guard against deleting an
/// ACTIVE cert — AMT will fault otherwise, but the QML side prompts
/// first. See #157.
void deleteDeviceCertificate(WsmanClient *client, const QString &instanceId,
                              std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on `AMT_PublicPrivateKeyPair`. Typically used to
/// clean up orphan key pairs (no matching cert). See #157.
void deleteDeviceKeyPair(WsmanClient *client, const QString &instanceId,
                          std::function<void(InvokeResult)> callback);

/// Put on `AMT_TLSSettingData` with the InstanceID-targeted endpoint
/// updated to the given state. `trustedCn` is sent only when
/// `mutualAuth` is true (AMT ignores it otherwise). See #157.
void setTlsSettings(WsmanClient *client, const QString &instanceId,
                    bool enabled, bool mutualAuth,
                    bool acceptNonSecureConnections,
                    const QStringList &trustedCn,
                    std::function<void(InvokeResult)> callback);

/// `AMT_PublicKeyManagementService.GenerateKeyPair` reply. The
/// `returnValue` mirrors the standard Invoke shape; on success the
/// `KeyPair` EPR in the body carries an `InstanceID` selector pointing
/// at the freshly created `AMT_PublicPrivateKeyPair` row. See #222.
struct GenerateKeyPairResult
{
    bool ok = false;
    QString error;
    int returnValue = -1;
    /// `Intel(r) AMT Key: Handle: N` style identifier; matches what
    /// `getDeviceCertStore` returns for the same row's `instanceId`.
    QString keyPairInstanceId;
};

/// Invoke `AMT_PublicKeyManagementService.GenerateKeyPair`. AMT
/// generates the key pair entirely inside firmware; only the public
/// half is reachable from this side (via a follow-up Get on the
/// returned EPR, see `getPublicPrivateKeyPair`).
///
/// `keyAlgorithm` matches AMT's enum: `0 = RSA` (the only value
/// firmware accepts today; ECC is parked behind a future firmware
/// release). `keyLength` is the modulus size in bits — pick 2048,
/// 3072, or 4096; 1024 is no longer accepted from CSME 18.0+.
void generateKeyPair(WsmanClient *client, int keyAlgorithm, int keyLength,
                     std::function<void(GenerateKeyPairResult)> callback);

/// Result of `getPublicPrivateKeyPair`. AMT returns the public-key
/// component of a `AMT_PublicPrivateKeyPair` as a base64 string in
/// the `DERKey` field; we decode it into raw DER for the caller.
struct PublicPrivateKeyPairGetResult
{
    bool ok = false;
    QString error;
    /// SubjectPublicKeyInfo (X.509) bytes — the same shape OpenSSL's
    /// `d2i_PUBKEY` expects.
    QByteArray derKey;
};

/// WS-Transfer Get on `AMT_PublicPrivateKeyPair[InstanceID=…]`. Used
/// by the Phase C issue-certificate flow to pull the public key off
/// the freshly generated pair so we can build the null-signed PKCS#10
/// template.
void getPublicPrivateKeyPair(WsmanClient *client, const QString &instanceId,
                              std::function<void(PublicPrivateKeyPairGetResult)> callback);

/// `AMT_PublicKeyManagementService.GeneratePKCS10RequestEx` reply.
/// `signedRequestDer` is the firmware-signed CertificationRequest in
/// raw DER (already base64-decoded). Wrap as PEM at the QML side.
struct GeneratePkcs10RequestResult
{
    bool ok = false;
    QString error;
    int returnValue = -1;
    QByteArray signedRequestDer;
};

/// Invoke `AMT_PublicKeyManagementService.GeneratePKCS10RequestEx`.
/// The `keyPairInstanceId` identifies the `AMT_PublicPrivateKeyPair`
/// the firmware should sign with (returned by `generateKeyPair`).
/// `signingAlgorithm` is AMT's enum: `0 = SHA1-RSA` (removed in CSME
/// 18.0+), `1 = SHA256-RSA`. `nullSignedCsrDer` is the un-signed
/// PKCS#10 template; build it with `buildNullSignedPkcs10Csr` from
/// `wsman/cert_request_builder.h`. See #222.
void generatePkcs10Request(WsmanClient *client,
                            const QString &keyPairInstanceId,
                            int signingAlgorithm,
                            const QByteArray &nullSignedCsrDer,
                            std::function<void(GeneratePkcs10RequestResult)> callback);

/// Put on `AMT_TLSCredentialContext` to bind a freshly imported cert
/// to a TLS endpoint. The PutResource's `ElementInContext` references
/// the cert (`AMT_PublicKeyCertificate[InstanceID]`); `ElementProvidingContext`
/// references the endpoint (`AMT_TLSProtocolEndpointCollection`).
///
/// AMT actually exposes the binding via a *Create* on
/// `AMT_TLSCredentialContext` when no prior binding exists, and a
/// Delete-then-Create when replacing; this op picks the operation
/// automatically based on whether the endpoint currently has a cert
/// bound — that decision is the caller's via `replaceExisting`. See #222.
void bindCertToTlsEndpoint(WsmanClient *client,
                            const QString &certInstanceId,
                            const QString &tlsEndpointCollectionId,
                            bool replaceExisting,
                            std::function<void(InvokeResult)> callback);

/// One Management Presence Server — from `AMT_ManagementPresenceRemoteSAP`.
struct MpsServer
{
    QString name;                ///< `Name` (the InstanceID equivalent).
    QString accessInfo;
    int port = 0;
    QString cn;                  ///< Optional trusted CN.
    int mpsType = 0;             ///< 0 = CIRA (external), 1 = CILA.
};

/// One HTTP proxy — from `IPS_HTTPProxyAccessPoint` (AMT 11+).
struct MpsHttpProxy
{
    /// Instance `Name` (the CIM key). Needed as the selector when
    /// deleting the entry — Intel AMT uses `Name` as the load-bearing
    /// identity for HTTP proxy access points.
    QString name;
    QString accessInfo;
    int port = 0;
    QString networkDnsSuffix;
};

/// One remote-access policy — `AMT_RemoteAccessPolicyRule`. AMT exposes
/// at most three rules (`User Initiated`, `Alert`, `Periodic`) and the
/// matching MPS servers come from the `…AppliesToMPS` link enumeration.
struct RemoteAccessPolicy
{
    QString name;                ///< "User Initiated" / "Alert" / "Periodic"
    int trigger = -1;            ///< CIM Trigger code (0=User, 1=Alert, 2=Periodic).
    int tunnelLifeTime = 0;
    /// Periodic-only extras decoded from `ExtendedData`:
    bool periodicInterval = false; ///< When true, `periodicSeconds` is meaningful.
    int periodicSeconds = 0;
    bool periodicTimeOfDay = false; ///< When true, `periodicHour`/`periodicMinute`.
    int periodicHour = 0;
    int periodicMinute = 0;
    /// MPS-server names this policy applies to (matches `MpsServer.name`).
    QStringList mpsNames;
};

struct EnvironmentDetection
{
    QStringList domains;
};

struct UserInitiatedCira
{
    /// 32768 Disabled / 32769 BIOS / 32770 OS / 32771 BIOS+OS.
    int enabledState = 32768;
};

struct RemoteAccessResult
{
    bool ok = false;
    QString error;
    EnvironmentDetection envDetection;
    UserInitiatedCira    userInitiated;
    QList<RemoteAccessPolicy> policies;
    QList<MpsServer>          servers;
    QList<MpsHttpProxy>       httpProxies;
    /// `true` when AMT actually exposed `IPS_HTTPProxyService` — older
    /// firmware (< 11) doesn't, and the QML should hide the section
    /// rather than render "(no proxies configured)".
    bool httpProxySupported = false;
};

/// Render the `AMT_UserInitiatedConnectionService.EnabledState` numeric
/// code into a human-readable label.
[[nodiscard]] QString userInitiatedCiraLabel(int code);

/// Run the legacy `PullRemoteAccess` BatchEnum and stitch the eight
/// classes into one result. Read-only — edit flows are out of scope.
void getRemoteAccess(WsmanClient *client,
                     std::function<void(RemoteAccessResult)> callback);

/// Invoke `IPS_HTTPProxyService.AddProxyAccessPoint` to register a new
/// HTTP proxy with the CIRA stack. `infoFormat` follows the CIM
/// `CIM_RemoteServiceAccessPoint.InfoFormat` codes — 3 = IPv4,
/// 4 = IPv6, 201 = DNS name (Intel's extension; legacy code uses
/// the same value). AMT caps the proxy list at 15 entries.
void addHttpProxy(WsmanClient *client, const QString &accessInfo,
                  int infoFormat, int port,
                  const QString &networkDnsSuffix,
                  std::function<void(InvokeResult)> callback);

/// Helper for `addHttpProxy`: classify `accessInfo` into a CIM
/// InfoFormat code. IPv4 → 3, IPv6 → 4, anything else (FQDN) → 201.
/// Pure function so the controller can call it for validation too.
[[nodiscard]] int classifyAccessInfo(const QString &accessInfo);

/// WS-Transfer Delete on `IPS_HTTPProxyAccessPoint` with the
/// `Name` selector populated from `MpsHttpProxy::name`. The class
/// has additional CIM keys (`SystemName`, `CreationClassName`,
/// `SystemCreationClassName`) but Intel AMT accepts a Delete that
/// only selects on `Name`.
void deleteHttpProxy(WsmanClient *client, const QString &name,
                     std::function<void(InvokeResult)> callback);

/// Put the `AMT_EnvironmentDetectionSettingData` with a new list of
/// detection-domain strings. AMT decides the device is "on-network"
/// when DNS resolves any of these, and only opens CIRA tunnels when
/// it can't. An empty list means "always treat as off-network".
/// See #158.
void setEnvironmentDetection(WsmanClient *client,
                              const QStringList &detectionStrings,
                              std::function<void(InvokeResult)> callback);

/// Invoke `AMT_UserInitiatedConnectionService.RequestStateChange`.
/// `requestedState` is one of the EnabledState codes the read side
/// exposes: 32768 Disabled / 32769 BIOS / 32770 OS / 32771 BIOS+OS.
/// See #158.
void setUserInitiatedConnectionState(WsmanClient *client, int requestedState,
                                      std::function<void(InvokeResult)> callback);

/// Input shape for `addMpServer`. `mpsType` is 0 = CIRA, 1 = CILA.
/// `authMethod` follows the AMT enum: 1 = none, 2 = MSChapV2-style
/// HTTP digest (legacy default). When `authMethod == 1` the username
/// and password are ignored.
struct MpServerInput
{
    QString accessInfo;        ///< IPv4 / IPv6 / FQDN string.
    int     infoFormat = 201;  ///< CIM code: 3=IPv4, 4=IPv6, 201=DNS.
    int     port = 4433;
    int     authMethod = 2;
    QString username;
    QString password;
    QString commonName;        ///< Trusted CN for cert validation.
    int     mpsType = 0;
};

/// Invoke `AMT_RemoteAccessService.AddMpServer`. AMT creates a new
/// `AMT_ManagementPresenceRemoteSAP` instance plus (when auth is
/// non-empty) a linked `AMT_MPSUsernamePassword` row. See #158.
void addMpServer(WsmanClient *client, const MpServerInput &input,
                  std::function<void(InvokeResult)> callback);

/// Put on `AMT_ManagementPresenceRemoteSAP` with the SAP's `Name`
/// selector. Edits AccessInfo / Port / InfoFormat / CN in place
/// without touching the auth credentials. See #158.
void updateMpServer(WsmanClient *client, const QString &name,
                    const QString &accessInfo, int infoFormat, int port,
                    const QString &commonName,
                    std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on `AMT_ManagementPresenceRemoteSAP` with the
/// `Name` selector. AMT cascades the matching MPSUsernamePassword
/// row, so a follow-up delete on the auth class isn't needed. See
/// #158.
void removeMpServer(WsmanClient *client, const QString &name,
                     std::function<void(InvokeResult)> callback);

/// Input shape for `addRemoteAccessPolicyRule`. AMT exposes exactly
/// three triggers (User Initiated, Alert, Periodic) and the firmware
/// is idempotent on `Trigger` — calling Add for an existing trigger
/// overwrites in place, so the same call path covers both add and
/// edit. See #224.
struct CiraPolicyInput
{
    /// CIM Trigger code: 0 = User Initiated, 1 = Alert, 2 = Periodic.
    int trigger = 0;
    /// `TunnelLifeTime` in seconds (0 = AMT default).
    int tunnelLifeTime = 0;
    /// Periodic-only — picks the `ExtendedData` shape.
    /// When `trigger != 2 (Periodic)` both flags are ignored.
    bool periodicInterval = false;   ///< true → encode interval branch.
    int  periodicSeconds = 0;        ///< only used when `periodicInterval`.
    bool periodicTimeOfDay = false;  ///< true → encode HH:MM branch.
    int  periodicHour = 0;
    int  periodicMinute = 0;
    /// `MpsServer::name` values to bind to this policy, split by
    /// `mpsType`: `mpsType == 0` → ciraMpsNames, `mpsType == 1`
    /// → cilaMpsNames. AMT routes the two arrays to separate
    /// parameters in `AddRemoteAccessPolicyRule`.
    QStringList ciraMpsNames;
    QStringList cilaMpsNames;
};

/// Encode the periodic-schedule fields of an `AMT_RemoteAccessPolicy`-
/// Rule into the `ExtendedData` blob (base64 over 8 or 12 bytes).
/// The decoder is `decodeExtendedData`; this is its symmetric writer.
///
///   [4 BE] type — 0 = interval, 1 = time-of-day
///   if interval:    [4 BE] seconds
///   if time-of-day: [4 BE] hour, [4 BE] minute
///
/// `intervalMode == true` → encode the interval branch using
/// `intervalSeconds`. Otherwise → encode the time-of-day branch using
/// `hour`/`minute`. Returns the base64 of the encoded bytes.
[[nodiscard]] QString buildExtendedData(bool intervalMode, int intervalSeconds,
                                        int hour, int minute);

/// Decode `AMT_RemoteAccessPolicyRule.ExtendedData`. Wire format and
/// usage are documented on `buildExtendedData`. The relevant fields
/// of `p` (`periodicInterval` / `periodicSeconds` /
/// `periodicTimeOfDay` / `periodicHour` / `periodicMinute`) are
/// updated in place; `p` is otherwise left alone.
void decodeExtendedData(const QString &b64, RemoteAccessPolicy &p);

/// Invoke `AMT_RemoteAccessService.AddRemoteAccessPolicyRule`. The
/// firmware is idempotent on `Trigger` — calling Add for an existing
/// trigger overwrites in place, so this is also the edit path. The
/// envelope is hand-rolled because `CIRAServers[]` and `CILAServers[]`
/// are EPR-shaped and `buildInvokeEnvelope` doesn't handle EPRs. See
/// #224.
void addRemoteAccessPolicyRule(WsmanClient *client,
                                const CiraPolicyInput &policy,
                                std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on `AMT_RemoteAccessPolicyRule` keyed by
/// `PolicyRuleName`. AMT cascades the linked
/// `AMT_RemoteAccessPolicyAppliesToMPS` rows, so the MPS-binding
/// table doesn't need a separate cleanup pass. See #224.
void removeRemoteAccessPolicyRule(WsmanClient *client,
                                   const QString &policyRuleName,
                                   std::function<void(InvokeResult)> callback);

/// WiFi port-level state from `CIM_WiFiPort` + `CIM_WiFiEndpoint` +
/// `AMT_WiFiPortConfigurationService`. Each field is independently
/// populated; missing ones leave defaults so the QML can render
/// "(unknown)" on partial firmware support.
struct WirelessPortStatus
{
    bool present = false;          ///< `true` when CIM_WiFiPort returned.
    int  portState = -1;           ///< 3 Disabled / 32768 S0 / 32769 S0+Sx/AC.
    int  radioState = -1;          ///< 2 On+Connected / 3 Off / 6 On+Disconnected.
    QString currentSsid;
    int  localProfileSyncEnabled = -1;   ///< 0 / 1 / -1 unknown.
    int  uefiProfileShareEnabled = -1;
};

/// One row from `CIM_WiFiEndpointSettings`. The optional 802.1x
/// linkage in `eap8021xProtocol` is populated when a
/// `CIM_IEEE8021xSettings` row with a matching `ElementName` is
/// returned.
struct WiFiProfile
{
    QString elementName;
    QString ssid;
    int authenticationMethod = -1;
    int encryptionMethod = -1;
    int priority = -1;
    /// `AuthenticationProtocol` integer from the linked
    /// `CIM_IEEE8021xSettings`. -1 when no 802.1x profile is linked.
    int eap8021xProtocol = -1;
};

/// Wired 802.1x — `AMT_8021XProfile` Get.
struct Wired8021xProfile
{
    bool present = false;
    bool enabled = false;
    int  authenticationProtocol = -1;
};

struct WirelessResult
{
    bool ok = false;
    QString error;
    WirelessPortStatus port;
    QList<WiFiProfile> profiles;     ///< Sorted by `priority` ascending.
    Wired8021xProfile  wired;
};

[[nodiscard]] QString wifiAuthMethodLabel(int code);
[[nodiscard]] QString wifiEncryptionLabel(int code);
[[nodiscard]] QString wifiPortStateLabel(int code);
[[nodiscard]] QString wifiRadioStateLabel(int code);
[[nodiscard]] QString eap8021xProtocolLabel(int code);

/// Ports the legacy `PullWireless` BatchEnum. Read-only.
void getWireless(WsmanClient *client,
                 std::function<void(WirelessResult)> callback);

/// Input shape for `addWiFiSettingsPsk` / `updateWiFiSettingsPsk`. AMT
/// distinguishes WPA2-PSK (`authenticationMethod=6`) and WPA3-PSK
/// (`authenticationMethod=7`); the dialog picks. `priority` is the
/// usual SSID-prioritisation knob (1 = top). PSK is the passphrase
/// the operator types — AMT hashes it on the way in.
///
/// Despite the "Psk" in the type name (kept for source compatibility
/// with Phase B callers), this struct also carries the optional EAP
/// fields the Phase C enterprise variant emits. When
/// `enterpriseEnabled` is false the envelope builder skips every EAP
/// field — the wire shape is byte-identical to Phase B. When true,
/// the matching `IEEE8021xSettingsInput` block and credential EPRs
/// are emitted. See #223.
struct WiFiPskProfile
{
    QString elementName;  ///< Identity key; required.
    QString ssid;
    /// PSK: 6 = WPA2-PSK, 7 = WPA3-PSK. Enterprise: 5 = WPA2-Enterprise,
    /// 8 = WPA3-Enterprise. The QML side flips this based on
    /// `enterpriseEnabled` × PSK/Enterprise.
    int authenticationMethod = 6;
    int encryptionMethod = 4;      ///< 3 = TKIP, 4 = CCMP.
    int priority = 1;
    /// Pre-Shared Key — operator's passphrase. Sent over the WSMAN
    /// channel (TLS-protected) and never echoed back on Get.
    QString psk;

    /// Phase C (#223) — when true, the envelope emits the EAP block.
    bool enterpriseEnabled = false;
    /// `CIM_IEEE8021xSettings.AuthenticationProtocol`:
    ///   0 = EAP-TLS, 1 = EAP-TTLS/MSCHAPv2,
    ///   2 = PEAPv0/MSCHAPv2, 3 = PEAPv1/GTC,
    ///   4 = EAP-FAST/MSCHAPv2, 5 = EAP-FAST/GTC.
    int authenticationProtocol = 0;
    /// PEAP / TTLS / EAP-FAST — RADIUS-side username + password.
    QString eapUsername;
    QString eapPassword;
    /// Optional "anonymous identity guard": AMT validates the
    /// CN/SAN of the RADIUS server cert against this name when
    /// `eapServerCertificateName` is non-empty.
    QString eapServerCertificateName;
    /// `CIM_IEEE8021xSettings.ServerCertificateNameComparison`:
    ///   1 = FullName, 2 = DomainSuffix.
    /// Ignored when `eapServerCertificateName` is empty.
    int eapServerCertificateNameComparison = 1;
    /// `InstanceID` of an `AMT_PublicKeyCertificate` row to use as
    /// the EAP-TLS client cert. Required for `authenticationProtocol
    /// == 0`; ignored otherwise.
    QString clientCertificateInstanceId;
    /// `InstanceID` of an `AMT_PublicKeyCertificate` row marked as
    /// a trusted root, used to validate the RADIUS server's cert.
    QString caCertificateInstanceId;
};

/// Invoke `AMT_WiFiPortConfigurationService.AddWiFiSettings` with a PSK
/// (or, when `profile.enterpriseEnabled`, EAP) profile. Hand-rolls
/// the envelope because the embedded `WiFiEndpointSettingsInput` and
/// `IEEE8021xSettingsInput` elements (plus credential EPRs) don't fit
/// the flat parameter shape `buildInvokeEnvelope` supports. See #159
/// and #223.
void addWiFiSettingsPsk(WsmanClient *client, const WiFiPskProfile &profile,
                         std::function<void(InvokeResult)> callback);

/// Test-only seam: hand back the bytes the `AddWiFiSettings` /
/// `UpdateWiFiSettings` envelope builder would send for `profile`,
/// without round-tripping through a `WsmanClient`. Used by
/// `wsman/tests/test_soap_envelope.cpp` to lock in the EAP-TLS /
/// PEAP wire shape from #223 without needing an HTTP server mock.
/// `methodName` is `"AddWiFiSettings"` or `"UpdateWiFiSettings"`.
[[nodiscard]] QByteArray buildWiFiSettingsEnvelopeForTesting(
    const QString &methodName, const WiFiPskProfile &profile,
    const QString &to = QStringLiteral("http://10.0.0.5:16992/wsman"),
    const QString &messageId = QStringLiteral("uuid:wifi-test"));

/// Invoke `AMT_WiFiPortConfigurationService.UpdateWiFiSettings` for an
/// existing PSK profile, keyed by `WiFiEndpointSettings` ElementName.
/// AMT replaces the entire row with the supplied values. See #159.
void updateWiFiSettingsPsk(WsmanClient *client, const WiFiPskProfile &profile,
                            std::function<void(InvokeResult)> callback);

/// WS-Transfer Delete on a single `CIM_WiFiEndpointSettings`,
/// identified by `InstanceID = "Intel(r) AMT:WiFi Endpoint Settings <ElementName>"`.
/// AMT also accepts `{ ElementName }` directly on this class. See #159.
void deleteWiFiProfile(WsmanClient *client, const QString &elementName,
                        std::function<void(InvokeResult)> callback);

/// Invoke `DeleteAllITProfiles` — wipes every WiFi profile the IT
/// channel installed. Loud confirmation needed in the UI. See #159.
void deleteAllITWiFiProfiles(WsmanClient *client,
                              std::function<void(InvokeResult)> callback);

/// Invoke `DeleteAllUserProfiles` — wipes every WiFi profile the OS-
/// side user added via Local Profile Sync. See #159.
void deleteAllUserWiFiProfiles(WsmanClient *client,
                                std::function<void(InvokeResult)> callback);

/// Invoke `CIM_WiFiPort.RequestStateChange(RequestedState)`. AMT uses
/// `3` for Disabled and `32768` for "Enabled in S0" (the practical
/// "on" value); `32769` is "Enabled in S0+Sx/AC". This helper picks
/// 32768 for `enabled=true` and 3 for `enabled=false`. See #159.
void setWiFiPortState(WsmanClient *client, bool enabled,
                       std::function<void(InvokeResult)> callback);

/// Put on `AMT_WiFiPortConfigurationService` with the
/// `LocalProfileSynchronizationEnabled` and `UEFIWiFiProfileShareEnabled`
/// fields updated. Other fields are not echoed — AMT preserves them
/// on the Put. See #159.
void setWiFiSyncSettings(WsmanClient *client,
                          int localProfileSynchronization,
                          int uefiWiFiProfileShare,
                          std::function<void(InvokeResult)> callback);

/// Put on `AMT_8021XProfile` for the wired enterprise profile. Sets
/// `Enabled` + `AuthenticationProtocol` (CIM
/// `CIM_IEEE8021xSettings.AuthenticationProtocol` codes: 0 = EAP-TLS,
/// 1 = EAP-TTLS/MSCHAPv2, 2 = PEAPv0/MSCHAPv2, 3 = PEAPv1/GTC,
/// 4 = EAP-FAST/MSCHAPv2, 5 = EAP-FAST/GTC). See #159.
void setWired8021xProfile(WsmanClient *client, bool enabled,
                           int authenticationProtocol,
                           std::function<void(InvokeResult)> callback);

/// One row from one of the three `IPS_*SessionUsingPort` enumerations.
/// AMT exposes only a handful of scalars per row: the source IP / MAC
/// the remote console used to connect, plus a system-side EPR that
/// points at the active session record. All three classes (SOL, KVM,
/// IDE-R) share the same shape — we collapse them into one struct.
struct ActiveRedirectionSession
{
    /// `SourceAddress` field from the row — usually the IPv4 or IPv6
    /// the remote console initiated from.
    QString sourceAddress;
    /// `SourcePort` — the remote's ephemeral port. 0 when missing.
    int sourcePort = 0;
    /// CIM-style EPR text reduced to a single identifying string;
    /// surfaced read-only for the operator. The legacy NW.js console
    /// rendered it as a small monospace caption.
    QString sessionInstanceId;
};

struct ActiveSessionsResult
{
    bool ok = false;
    QString error;
    QList<ActiveRedirectionSession> sol;
    QList<ActiveRedirectionSession> kvm;
    QList<ActiveRedirectionSession> ider;
};

/// Enumerate `IPS_SolSessionUsingPort`, `IPS_KvmSessionUsingPort`,
/// and `IPS_IderSessionUsingPort` in parallel and stitch into one
/// result. Tolerates per-class faults — a faulted enumeration just
/// leaves its list empty. AMT allows at most one session per channel
/// so the lists are typically 0–1 entries long, but we surface
/// whatever the firmware returns. See #160.
void getActiveSessions(WsmanClient *client,
                       std::function<void(ActiveSessionsResult)> callback);

/// Walk every page of `AMT_AuditLog.ReadRecords` and return the parsed
/// records. Pages are 16-record chunks per the AMT contract. Faulted
/// pages short-circuit and return the entries collected so far with
/// `ok=false` and `error` set.
void enumerateAuditLog(WsmanClient *client,
                       std::function<void(AuditLogResult)> callback);

struct EventLogEntry
{
    /// Synthetic 1-based index within the current GetRecords sweep —
    /// AMT_MessageLog records have no stable RecordID across iterations.
    QString recordId;
    /// `yyyy-MM-dd hh:mm:ss`, treating the AMT seconds-since-epoch as a
    /// raw clock value (no client-side timezone conversion, matching
    /// the legacy MeshCommander display).
    QString timestamp;
    /// CIM severity bucket as a decimal string (0/1=OK, 2=degraded,
    /// 3=minor, 4=major, 5=critical, 6=fatal). Kept as a string so QML
    /// can drive its existing colour map without a custom type.
    QString severity;
    /// Decoded message — built from the 21-byte record's
    /// (EventSensorType, EventOffset, EventSourceType, EventData[8])
    /// tuple via the legacy lookup tables.
    QString message;
    /// Human-readable entity name from `kSystemEntityTypes` (e.g.
    /// "BIOS", "Intel(r) ME"). Empty if the entity code is out of
    /// range. Surfaced as tooltip / secondary text in the UI.
    QString entityLabel;
};

struct EventLogResult
{
    bool ok = false;
    QString error;
    QList<EventLogEntry> entries;
};

/// One AMT user ACL entry — composed of `GetUserAclEntryEx` +
/// `GetAclEnabledState` per handle, with the admin entry rendered as
/// a synthetic row with `handle = -1` and `accessPermission = 999`.
struct UserAccount
{
    /// Per-AMT Handle for the user. -1 means this is the synthetic
    /// admin entry (`AMT_AuthorizationService.GetAdminAclEntry`).
    int handle = 0;
    /// Either `digestUsername` or `kerberosUserSidB64` is set,
    /// depending on whether AMT stores the entry as HTTP-digest or
    /// as a Kerberos SID. The admin entry has only the username.
    QString digestUsername;
    QString kerberosUserSidB64;
    /// Friendly name — DigestUsername if set, else SID-string for
    /// Kerberos entries. Computed by the controller.
    QString name;
    /// 0 = Local only, 1 = Network only, 2 = All. 999 = the synthetic
    /// admin entry.
    int accessPermission = -1;
    /// CIM realm bitmask the user is authorised on. Indexes into the
    /// legacy `RealmNames` table; bit 3 means "Administrator" (full
    /// access).
    QList<int> realms;
    bool enabled = true;
    /// `true` when the digest username starts with `$$` — these are
    /// internal AMT system accounts that the legacy app hides
    /// behind a "Show hidden" toggle.
    bool hidden = false;
};

struct UserAccountsResult
{
    bool ok = false;
    QString error;
    QList<UserAccount> accounts;
};

/// Map an AMT realm bit-index to its human-readable name. Returns an
/// empty string for the slots that are reserved / unused in the
/// legacy `RealmNames` table.
[[nodiscard]] QString realmName(int index);

/// Map `AccessPermission` (0/1/2 — local/network/both) to a label.
/// The synthetic admin sentinel `999` returns "Administrator".
[[nodiscard]] QString accessPermissionLabel(int code);

/// Fetch the AMT event log via `AMT_MessageLog.PositionToFirstRecord`
/// followed by repeated `GetRecords` calls (batch size 390) until
/// `NoMoreRecords` is set. Each base64 record is a 21-byte structure
/// containing a UNIX-style timestamp and the (sensorType, offset,
/// sourceType, data[8]) tuple that — combined with the lookup tables
/// in `operations.cpp` — yields a human-readable message. The
/// `AMT_EventLogEntry` CIM class is *not* used: AMT firmware does not
/// populate human-readable text there.
void enumerateEventLog(WsmanClient *client,
                       std::function<void(EventLogResult)> callback);

/// Decode a single 21-byte AMT event record (already base64-decoded)
/// into an `EventLogEntry`. Returns an empty `recordId` if the record
/// is too short or carries a zero/sentinel timestamp. Exposed for
/// unit testing — the production path inside `enumerateEventLog`
/// calls the same helper.
[[nodiscard]] EventLogEntry decodeEventRecord(const QByteArray &raw);

/// Enumerate `CIM_Account` instances. Returns name, InstanceID,
/// element name, and enabled state for each. AMT exposes both the
/// AMT-side accounts and any Active Directory bindings through this
/// class; the QML side surfaces what AMT reports.
void enumerateUserAccounts(WsmanClient *client,
                            std::function<void(UserAccountsResult)> callback);

/// Compute the digest-password representation `AMT_AuthorizationService`
/// wants: `base64(MD5(username + ":" + realm + ":" + plaintext))`. Pure
/// function — tested in isolation. The realm comes from
/// `AMT_GeneralSettings.DigestRealm`. AMT verifies the resulting digest
/// against the same HTTP-digest hash it would compute internally for
/// the next session.
[[nodiscard]] QString computeDigestPassword(const QString &username,
                                            const QString &realm,
                                            const QString &plaintext);

/// Patch payload for `updateUserAclEntryEx`. Each `set*` flag gates
/// whether the corresponding field is included in the invoke; unset
/// fields preserve their existing value. `realms` is the new full
/// realm set when `setRealms` is true (AMT does not merge).
struct UserAclEntryPatch
{
    bool setDigestUsername = false;   QString digestUsername;
    /// Pre-hashed digest password — caller computes via
    /// `computeDigestPassword`. Set only on actual rotation.
    bool setDigestPassword = false;   QString digestPassword;
    bool setAccessPermission = false; int accessPermission = 2;
    bool setRealms = false;           QList<int> realms;
};

/// Invoke `AMT_AuthorizationService.AddUserAclEntryEx`. AMT assigns a
/// fresh `Handle` to the new entry which the read-side re-enumeration
/// surfaces; this call doesn't report the handle itself, just the
/// `ReturnValue`. `digestPassword` is the output of
/// `computeDigestPassword`. See #156.
void addUserAclEntryEx(WsmanClient *client, const QString &digestUsername,
                       const QString &digestPassword, int accessPermission,
                       const QList<int> &realms,
                       std::function<void(InvokeResult)> callback);

/// Invoke `AMT_AuthorizationService.UpdateUserAclEntryEx`. `handle` is
/// the existing per-AMT handle from the read side. Only patch-set
/// fields are sent in the body. See #156.
void updateUserAclEntryEx(WsmanClient *client, int handle,
                          const UserAclEntryPatch &patch,
                          std::function<void(InvokeResult)> callback);

/// Invoke `AMT_AuthorizationService.RemoveUserAclEntry(Handle)`.
/// Deleting the AMT admin is firmware-protected; deleting the
/// operator's own row is allowed by AMT but locks the session out —
/// the controller / QML side adds a guard. See #156.
void removeUserAclEntry(WsmanClient *client, int handle,
                        std::function<void(InvokeResult)> callback);

/// Invoke `AMT_AuthorizationService.SetAclEnabledState(Handle, Enabled)`.
/// Disabling an account preserves the entry but rejects auth attempts
/// against it. See #156.
void setAclEnabledState(WsmanClient *client, int handle, bool enabled,
                        std::function<void(InvokeResult)> callback);

/// Invoke `AMT_AuthorizationService.SetAdminAclEntryEx(Username,
/// DigestPassword)`. Rotates the AMT admin credentials. `digestPassword`
/// is `computeDigestPassword(username, realm, plaintext)`. Admin realms
/// are firmware-fixed — only the username + password change here.
/// See #156.
void setAdminAclEntryEx(WsmanClient *client, const QString &username,
                        const QString &digestPassword,
                        std::function<void(InvokeResult)> callback);

/// Run the full boot-source-override chain:
///   1. ChangeBootOrder(null)   — clear the boot order
///   2. Put AMT_BootSettingData — write the action's flags
///   3. SetBootConfigRole(1)    — mark this config as the next-boot one
///   4. ChangeBootOrder(source) — when `amtBootSource` is non-empty
///   5. RequestPowerStateChange — apply the power action
/// `callback` fires once with the final result; intermediate failures
/// short-circuit and surface their own error string.
void performBootAction(WsmanClient *client, BootActionParams params,
                       std::function<void(InvokeResult)> callback);

/// Invoke `AMT_AuditLog.ClearLog`. Clears every audit-log entry the
/// firmware has stored. The caller must have `Audit Log Reader` realm
/// (or equivalent admin) — otherwise `r.error` is the firmware fault.
/// No parameters; AMT's ClearLog is idempotent and returns `0` on
/// success.
void clearAuditLog(WsmanClient *client,
                   std::function<void(InvokeResult)> callback);

/// Invoke `AMT_MessageLog.ClearLog`. Clears the firmware event log
/// (the same log surfaced via `enumerateEventLog`). AMT's ClearLog
/// takes no parameters and returns `0` on success.
void clearEventLog(WsmanClient *client,
                   std::function<void(InvokeResult)> callback);

} // namespace qumesh::wsman
