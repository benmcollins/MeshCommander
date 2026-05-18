// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <functional>

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
    QList<QPair<QString, QString>> identities;
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
    /// Runtime: `true` when a session that requires consent is in
    /// progress and the operator hasn't yet entered the code AMT
    /// shows on the target's local screen. Combines `OptInRequired`
    /// from the firmware (which itself derives from the various
    /// per-redir policy flags) with the per-firmware-version semantics.
    bool optInRequired = false;
    /// `IPS_OptInService.OptInState`: 0=NotStarted, 1=Requested,
    /// 2=Displayed (code on screen, waiting), 3=Received,
    /// 4=InSession (consent already granted).
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

/// Snapshot of `AMT_AgentPresenceCapabilities` (the per-firmware
/// max-watchdog / max-action ceiling) plus every
/// `AMT_AgentPresenceWatchdog` row. Phase A (#164) — no
/// transitions/actions association walking yet.
struct AgentPresenceResult
{
    bool ok = false;
    QString error;
    int maxTotalAgents = 0;
    int maxTotalActions = 0;
    QList<AgentPresenceWatchdog> watchdogs;
};

/// Enumerate `AMT_AgentPresenceWatchdog` and read
/// `AMT_AgentPresenceCapabilities`. See #164.
void getAgentPresence(WsmanClient *client,
                      std::function<void(AgentPresenceResult)> callback);

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
};

/// One `AMT_Hdr8021Filter` (L2) row — VLAN / ethertype / 802.1 priority.
struct Hdr8021Filter
{
    QString instanceId;
    QString name;
    int filterDirection = -1;       ///< 1=in, 2=out, 3=both.
    int vlanTag = -1;               ///< -1 = not set.
    int etherType = -1;
    int priority = -1;
};

/// One `AMT_IPHeadersFilter` (L3/L4) row.
struct IpHeadersFilter
{
    QString instanceId;
    QString name;
    int filterDirection = -1;
    QString srcAddress;
    QString dstAddress;
    int protocol = -1;              ///< IPv4 protocol number, -1 if any.
    int srcPort = -1;
    int dstPort = -1;
};

/// One `AMT_NetworkFilter` sub-rule reference.
struct NetworkFilterRow
{
    QString instanceId;
    QString name;
    QString filterClass;            ///< Either `Hdr8021Filter` or `IPHeadersFilter`.
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
    QList<NetworkFilterRow>    subFilters;
};

/// Enumerate the System Defense classes. See #165 phase A.
void getSystemDefense(WsmanClient *client,
                      std::function<void(SystemDefenseResult)> callback);

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

/// Walk every page of `AMT_AuditLog.ReadRecords` and return the parsed
/// records. Pages are 16-record chunks per the AMT contract. Faulted
/// pages short-circuit and return the entries collected so far with
/// `ok=false` and `error` set.
void enumerateAuditLog(WsmanClient *client,
                       std::function<void(AuditLogResult)> callback);

struct EventLogEntry
{
    QString recordId;
    QString timestamp;     ///< AMT-formatted hex string; QML formats it.
    QString severity;      ///< CIM severity enum as text.
    QString message;
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

/// Enumerate `AMT_EventLogEntry` instances via WS-Enumeration. Walks
/// Pull responses until `EndOfSequence`, parses each item's RecordID /
/// CreationTimeStamp / Severity / Message, and hands the full list to
/// the callback.
void enumerateEventLog(WsmanClient *client,
                       std::function<void(EventLogResult)> callback);

/// Enumerate `CIM_Account` instances. Returns name, InstanceID,
/// element name, and enabled state for each. AMT exposes both the
/// AMT-side accounts and any Active Directory bindings through this
/// class; the QML side surfaces what AMT reports.
void enumerateUserAccounts(WsmanClient *client,
                            std::function<void(UserAccountsResult)> callback);

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

} // namespace qumesh::wsman
