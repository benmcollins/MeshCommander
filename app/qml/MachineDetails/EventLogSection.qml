// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Event log" section (was the inline section 10
/// before #325). Pulled into its own file — see OverviewSection.qml
/// for the rationale.
ColumnLayout {
    id: root

    required property MachineDetailsController controller

    spacing: 8


    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true
        Layout.topMargin: 24
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        Text {
            text: qsTr("EVENT LOG")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }
        Text {
            text: controller.eventLog.length === 0
                ? qsTr("No entries.")
                : qsTr("%1 entries").arg(controller.eventLog.length)
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: 20
        }
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.bottomMargin: 24
        clip: true
        model: controller.eventLog
        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: 36
            color: index % 2 === 0 ? "transparent" : Colors.elevated
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12

                Text {
                    text: modelData.severity || "—"
                    color: {
                        // CIM Severity: 0/1 OK, 2 Degraded, 3 Minor,
                        // 4 Major, 5 Critical, 6 Fatal. Bucket into
                        // our three on/standby/error colours.
                        const s = parseInt(modelData.severity);
                        if (s >= 5) return Colors.error;
                        if (s >= 3) return Colors.standby;
                        return Colors.textMuted;
                    }
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 32
                }
                Text {
                    text: modelData.timestamp || "—"
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 160
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.message || ""
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            visible: controller.eventLog.length === 0 && !controller.busy
            anchors.centerIn: parent
            text: qsTr("No event log entries.")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeS
        }
    }
}
