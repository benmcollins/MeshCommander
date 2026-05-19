// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

import QtQuick
import QtQuick.Layouts
import QuMesh

/// Small "verified" badge that flashes briefly when a TLS reconnect
/// matched a pinned fingerprint. Drive it by calling `flash(fp)` from
/// the consumer when `peerCertVerifiedByPin(fp)` fires on the relevant
/// controller. Auto-fades after ~1.5 s.
Rectangle {
    id: root

    function flash(fp) {
        fingerprint = fp || "";
        if (!shownAnim.running && !hideAnim.running) {
            opacity = 0;
            scale = 0.92;
        }
        shownAnim.restart();
    }

    property string fingerprint: ""

    implicitWidth: row.implicitWidth + 16
    implicitHeight: row.implicitHeight + 8
    radius: height / 2
    color: Qt.rgba(Colors.on.r, Colors.on.g, Colors.on.b, 0.18)
    border.color: Colors.on
    border.width: 1
    opacity: 0
    scale: 0.92
    visible: opacity > 0.01

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Icon {
            name: "lock"
            size: 14
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
        }
        Text {
            text: qsTr("Pinned certificate verified")
            color: Colors.on
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.weight: Font.Medium
        }
    }

    // 250 ms fade/scale in, hold 1500 ms, 350 ms fade out.
    SequentialAnimation {
        id: shownAnim
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1.0; duration: 250; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: "scale";   to: 1.0; duration: 250; easing.type: Easing.OutBack }
        }
        PauseAnimation { duration: 1500 }
        ScriptAction { script: hideAnim.start() }
    }
    NumberAnimation {
        id: hideAnim
        target: root
        property: "opacity"
        to: 0
        duration: 350
        easing.type: Easing.InCubic
    }
}
