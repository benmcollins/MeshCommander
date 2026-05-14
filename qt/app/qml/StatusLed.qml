// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QuMesh

Item {
    id: root

    /// on | off | standby | error | unknown
    /// "unknown" implies the system is still polling — see `pulse`.
    property string ledState: "unknown"

    /// When true and ledState === "unknown", the LED breathes between
    /// full and 35% opacity to signal "in flight" (vs. "no data").
    /// Default true; turn off for static displays.
    property bool pulse: true

    implicitWidth: 10
    implicitHeight: 10

    Rectangle {
        id: dot
        radius: width / 2
        color: root.ledState === "on"      ? Colors.on
             : root.ledState === "standby" ? Colors.standby
             : root.ledState === "off"     ? Colors.off
             : root.ledState === "error"   ? Colors.error
             : "transparent"
        border.width: root.ledState === "unknown" ? 1 : 0
        border.color: Colors.textFaint
        anchors.fill: parent

        Behavior on color { ColorAnimation { duration: Motion.normal } }

        // Breathing animation while we're waiting on a poller. The
        // outer Item's opacity stays at 1 so layout never shifts.
        SequentialAnimation on opacity {
            running: root.pulse && root.ledState === "unknown"
            loops: Animation.Infinite
            alwaysRunToEnd: false
            NumberAnimation { from: 1.0; to: 0.35; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.35; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
        }
    }
}
