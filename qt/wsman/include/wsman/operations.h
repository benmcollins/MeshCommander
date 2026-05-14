// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

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

struct EthernetSettingsResult
{
    bool ok = false;
    QString error;
    QString macAddress;
    bool dhcpEnabled = false;
    bool ipv4Enabled = false;
    QString ipAddress;
    QString subnetMask;
    QString defaultGateway;
    QString primaryDns;
    QString secondaryDns;
    QString linkPolicy;          ///< AMT-only / OS-shared / etc. (best-effort)
};

struct TimeSettingsResult
{
    bool ok = false;
    QString error;
    qint64 secondsSinceEpoch = 0; ///< 0 if not parsed.
};

/// Subset of `AMT_BootCapabilities` flags the UI gates power actions on.
/// Each is `true` when the firmware reports the capability.
struct BootCapabilitiesResult
{
    bool ok = false;
    QString error;
    bool biosSetup = false;
    bool biosPause = false;
    bool secureErase = false;
    bool forceUefiHttpsBoot = false;
    bool platformErase = false;
};

struct InvokeResult
{
    bool ok = false;
    QString error;
    int returnValue = -1;        ///< Vendor-specific status code from the Invoke reply.
};

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

/// Read `AMT_BootCapabilities` — which boot-source-override flags the
/// firmware will accept. Drives the gating of menu entries
/// (Secure Erase / Platform Erase / HTTPS Boot etc.).
void getBootCapabilities(WsmanClient *client,
                         std::function<void(BootCapabilitiesResult)> callback);

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
};

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
