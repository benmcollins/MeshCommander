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

/// Invoke `CIM_PowerManagementService.RequestPowerStateChange`. `powerState`
/// is the CIM target enum: 2=On, 8=Off (Hard), 5=Reset, 4=Sleep,
/// 12=Off-Soft-Graceful, 13=Off-Hard-Graceful, etc.
void requestPowerStateChange(WsmanClient *client, int powerState,
                             std::function<void(InvokeResult)> callback);

} // namespace qumesh::wsman
