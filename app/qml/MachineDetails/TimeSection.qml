// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Time" section (was the inline section 5
/// before #325). Pulled into its own file — see header comment in
/// OverviewSection.qml.
ColumnLayout {
    id: root

    required property MachineDetailsController controller

    spacing: 18

    // Time data is fetched centrally via `root.refreshCurrent()`
    // when this section becomes active.

    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true
        Layout.topMargin: 24
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Text {
            text: qsTr("TIME")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }
        Text {
            text: root.controller.amtEpoch === 0
                ? qsTr("Not yet fetched")
                : Qt.formatDateTime(new Date(root.controller.amtEpoch * 1000),
                                     "yyyy-MM-dd  HH:mm:ss  t")
            color: Colors.text
            font.family: Type.mono
            font.pixelSize: 20
        }
    }

    Section {
        title: qsTr("DETAILS")
        Layout.fillWidth: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        GridLayout {
            columns: 2
            columnSpacing: 16
            rowSpacing: 6
            Layout.fillWidth: true

            Text { text: qsTr("Unix epoch"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
            Text { text: String(root.controller.amtEpoch); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

            Text { text: qsTr("Local skew"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
            Text {
                text: root.controller.amtEpoch === 0
                    ? qsTr("—")
                    : qsTr("%1 s")
                        .arg(Math.round(root.controller.amtEpoch -
                              (new Date().getTime() / 1000)))
                color: Colors.text
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
            }
        }
    }

    RowLayout {
        Layout.leftMargin: 24
        spacing: 8

        FlatButton {
            text: qsTr("Refresh")
            enabled: !root.controller.busy
            onClicked: root.controller.refreshTime()
        }
        AccentButton {
            text: qsTr("Sync now")
            // No live skew until the first read — disable the action
            // so the operator doesn't push garbage (host_now ≈ 0
            // device epoch) at the firmware.
            enabled: !root.controller.busy && root.controller.amtEpoch !== 0
            onClicked: root.controller.syncDeviceTime()
        }
    }

    Item { Layout.fillHeight: true }
}
