// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

#pragma once

#include <QString>
#include <functional>

namespace meshcommander::wsman {

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

/// Send the DMTF `Identify` discovery message to the endpoint configured on
/// `client` and invoke `callback` exactly once with the result. Requires
/// no credentials; useful for connection sanity-checks.
void identify(WsmanClient *client, std::function<void(IdentifyResult)> callback);

/// Read `CIM_AssociatedPowerManagementService.PowerState` via WS-Transfer
/// Get. Requires credentials to be set on the client. PowerState is a CIM
/// enum: 2=On, 6=Off (Soft), 8=Off (Hard), 13=Sleep, etc.
void getPowerState(WsmanClient *client, std::function<void(PowerStateResult)> callback);

} // namespace meshcommander::wsman
