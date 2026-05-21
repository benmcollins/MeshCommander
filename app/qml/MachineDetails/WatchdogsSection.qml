// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Watchdogs" section. Pulled into its own file
/// — see OverviewSection.qml for the rationale. Pre-#325 this was the
/// inline body of the matching Loader in MachineDetailsWindow.qml.
ColumnLayout {
    id: root

    required property MachineDetailsController controller
    required property var watchdogDialog
    required property var watchdogActionsDialog

    /// True when the configured watchdog count has hit the firmware
    /// ceiling — used to grey out Add and explain why in the header.
    readonly property bool atCeiling: {
        const ap = root.controller.agentPresence || {};
        const cap = ap.maxTotalAgents || 0;
        const cur = (ap.watchdogs || []).length;
        return cap > 0 && cur >= cap;
    }

    /// Count the actions attached to a given watchdog so the row can
    /// show "Actions: N" without each delegate iterating the flat list
    /// itself. The flat list lives on `controller.agentPresence.actions`
    /// and is refreshed by `refreshAgentPresence`.
    function _actionsCountFor(deviceIdGuid) {
        const ap = root.controller.agentPresence || {};
        const all = ap.actions || [];
        let n = 0;
        for (let i = 0; i < all.length; i++) {
            if (all[i].watchdogDeviceIdGuid === deviceIdGuid) n++;
        }
        return n;
    }

    spacing: 8


    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true
        Layout.topMargin: 24
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        RowLayout {
            spacing: 12
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true

                Text {
                    text: qsTr("WATCHDOGS")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.letterSpacing: 2
                    font.weight: Font.Medium
                }
                Text {
                    text: {
                        const ap = root.controller.agentPresence || {};
                        const w = ap.watchdogs || [];
                        if (Object.keys(ap).length === 0)
                            return qsTr("Not yet fetched");
                        if (w.length === 0)
                            return qsTr("No agent presence watchdog configured.");
                        return qsTr("%1 watchdog%2 configured")
                            .arg(w.length)
                            .arg(w.length === 1 ? "" : "s");
                    }
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: 20
                }
                Text {
                    visible: (root.controller.agentPresence
                               && root.controller.agentPresence.maxTotalAgents > 0)
                    text: qsTr("Firmware ceiling: %1 watchdogs, %2 actions total.")
                        .arg(root.controller.agentPresence.maxTotalAgents)
                        .arg(root.controller.agentPresence.maxTotalActions)
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Text {
                    visible: root.atCeiling
                    text: qsTr("At the firmware ceiling — delete a watchdog before adding a new one.")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
            }

            AccentButton {
                text: qsTr("Add watchdog")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: !root.atCeiling
                onClicked: root.watchdogDialog.openForAdd()
            }
        }
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.bottomMargin: 24
        clip: true
        model: (root.controller.agentPresence
                 && root.controller.agentPresence.watchdogs) || []
        ScrollBar.vertical: ScrollBar {}
        spacing: 6

        delegate: Rectangle {
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: watchdogCol.implicitHeight + 16
            color: Colors.surface
            border.color: Colors.borderMuted
            border.width: 1
            radius: 8

            ColumnLayout {
                id: watchdogCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 8
                spacing: 4

                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: parent.parent.modelData.description
                              && parent.parent.modelData.description.length > 0
                            ? parent.parent.modelData.description
                            : parent.parent.modelData.deviceIdGuid
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        font.weight: Font.Medium
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        readonly property bool running:
                            parent.parent.parent.modelData.currentState === 4
                        implicitWidth: stateChip.implicitWidth + 14
                        implicitHeight: stateChip.implicitHeight + 6
                        radius: 4
                        color: running
                            ? Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.18)
                            : Qt.rgba(Colors.textFaint.r, Colors.textFaint.g, Colors.textFaint.b, 0.10)
                        border.color: running
                            ? Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.40)
                            : Colors.borderMuted
                        border.width: 1

                        Text {
                            id: stateChip
                            anchors.centerIn: parent
                            text: parent.parent.parent.parent.modelData.currentStateLabel
                            color: parent.running ? Colors.accent : Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.weight: Font.Medium
                            font.letterSpacing: 1
                        }
                    }

                    FlatButton {
                        text: qsTr("Actions")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.watchdogActionsDialog.openFor(watchdogCol.parent.modelData)
                    }
                    FlatButton {
                        text: qsTr("Edit")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.watchdogDialog.openForEdit(watchdogCol.parent.modelData)
                    }
                    FlatButton {
                        text: qsTr("Delete")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: {
                            watchdogConfirmDialog.pendingDeviceIdGuid =
                                watchdogCol.parent.modelData.deviceIdGuid || "";
                            watchdogConfirmDialog.ask(
                                qsTr("Delete watchdog"),
                                qsTr("Remove %1 from AMT. The watchdog and any actions cascading from it will be deleted on the device.")
                                    .arg(watchdogCol.parent.modelData.description
                                         || watchdogCol.parent.modelData.deviceIdGuid
                                         || qsTr("the watchdog")),
                                qsTr("Delete"),
                                true);
                        }
                    }
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 2
                    Layout.fillWidth: true
                    Layout.topMargin: 4

                    Text { text: qsTr("Monitored"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text {
                        text: watchdogCol.parent.modelData.monitoredEntityLabel
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                    }
                    Text { text: qsTr("Enabled"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text {
                        text: watchdogCol.parent.modelData.enabledStateLabel
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                    }

                    Text { text: qsTr("Startup"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text {
                        text: qsTr("%1 s").arg(watchdogCol.parent.modelData.startupIntervalSec)
                        color: Colors.text
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                    }
                    Text { text: qsTr("Timeout"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text {
                        text: qsTr("%1 s").arg(watchdogCol.parent.modelData.timeoutIntervalSec)
                        color: Colors.text
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                    }

                    Text { text: qsTr("Actions"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text {
                        readonly property int count:
                            root._actionsCountFor(watchdogCol.parent.modelData.deviceIdGuid)
                        text: count === 0
                            ? qsTr("None configured")
                            : qsTr("%1 configured").arg(count)
                        color: count === 0 ? Colors.textFaint : Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                        Layout.columnSpan: 3
                    }
                }

                Text {
                    text: watchdogCol.parent.modelData.deviceIdGuid
                    color: Colors.textFaint
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
