// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

import QtQuick
import QuMesh

/// Thin "pulse-of-the-system" bar under the title bar. Idle state is
/// a 2 px borderMuted line; on `ActivityHeartbeat.succeeded` a single
/// accent-coloured gradient sweep crosses from left to right; on
/// `ActivityHeartbeat.failed` the whole bar pulses red. Both are
/// short, non-blocking, and intentionally subtle so the bar stays
/// out of the way until something interesting happens.
Item {
    id: root

    implicitHeight: 2

    Rectangle {
        id: idle
        anchors.fill: parent
        color: Colors.borderMuted
    }

    // Animated success sweep. A wide rectangle slides from left of the
    // bar to right; opacity peaks mid-sweep and fades on exit.
    Rectangle {
        id: sweep
        height: parent.height
        width: parent.width * 0.45
        x: -width
        opacity: 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.0) }
            GradientStop { position: 0.5; color: Colors.accent }
            GradientStop { position: 1.0; color: Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.0) }
        }

        SequentialAnimation {
            id: sweepAnim
            ParallelAnimation {
                NumberAnimation { target: sweep; property: "opacity"; from: 0; to: 1; duration: 200 }
                NumberAnimation { target: sweep; property: "x"; from: -sweep.width; to: root.width; duration: 900; easing.type: Easing.InOutCubic }
            }
            NumberAnimation { target: sweep; property: "opacity"; to: 0; duration: 150 }
        }
    }

    // Failure flash. Full-width red wash that pulses on then fades.
    Rectangle {
        id: fail
        anchors.fill: parent
        color: Colors.error
        opacity: 0

        SequentialAnimation {
            id: failAnim
            NumberAnimation { target: fail; property: "opacity"; from: 0; to: 0.85; duration: 120 }
            PauseAnimation { duration: 380 }
            NumberAnimation { target: fail; property: "opacity"; to: 0; duration: 600 }
        }
    }

    // Subscribe to the heartbeat singleton via Connections — the bus
    // bumps a counter we watch, so back-to-back reports don't get
    // dropped while the previous animation is still running.
    Connections {
        target: ActivityHeartbeat
        function onSuccessCounterChanged() { sweepAnim.restart() }
        function onFailureCounterChanged() { failAnim.restart() }
    }
}
