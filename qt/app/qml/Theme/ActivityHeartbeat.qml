// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton

import QtQuick

/// Process-wide pulse bus. Any controller window that completes a
/// user-initiated op (session connect, WSMAN refresh, power action,
/// …) calls `reportSuccess()` / `reportFailure()` here, and the
/// HeartbeatBar under the title bar in `Main.qml` lights up
/// accordingly. We deliberately don't auto-poll — see issue #77.
/// This bus replaces the original poll-driven heartbeat the legacy
/// MeshCommander had.
QtObject {
    /// Bumped each time a controller reports a successful op.
    /// HeartbeatBar listens for the changed signal to kick off its
    /// sweep animation.
    property int successCounter: 0
    property int failureCounter: 0
    /// Carries the human-readable last-error string for tooltips.
    property string lastFailure: ""

    function reportSuccess() {
        ++successCounter;
    }

    function reportFailure(message) {
        lastFailure = message || "";
        ++failureCounter;
    }
}
