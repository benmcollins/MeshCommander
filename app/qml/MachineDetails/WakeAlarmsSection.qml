// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "WakeAlarms" section. Pulled into its own file
/// — see OverviewSection.qml for the rationale. Pre-#325 this was the
/// inline body of the matching Loader in MachineDetailsWindow.qml.
ColumnLayout {
    id: root

    required property MachineDetailsController controller
    required property var wakeAlarmDialog

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
                    text: qsTr("WAKE ALARMS")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.letterSpacing: 2
                    font.weight: Font.Medium
                }
                Text {
                    text: root.controller.wakeAlarms.length === 0
                        ? qsTr("No wake alarms registered.")
                        : qsTr("%1 alarm%2 scheduled")
                              .arg(root.controller.wakeAlarms.length)
                              .arg(root.controller.wakeAlarms.length === 1 ? "" : "s")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: 20
                }
            }

            AccentButton {
                text: qsTr("Add alarm")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.wakeAlarmDialog.openForAdd()
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
        model: root.controller.wakeAlarms
        ScrollBar.vertical: ScrollBar {}
        spacing: 6

        delegate: Rectangle {
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: alarmCol.implicitHeight + 16
            color: Colors.surface
            border.color: Colors.borderMuted
            border.width: 1
            radius: 8

            ColumnLayout {
                id: alarmCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 8
                spacing: 2

                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: alarmCol.parent.modelData.elementName
                              || alarmCol.parent.modelData.instanceId
                              || qsTr("(unnamed)")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        font.weight: Font.Medium
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        visible: alarmCol.parent.modelData.deleteOnCompletion
                        implicitWidth: dotcText.implicitWidth + 12
                        implicitHeight: dotcText.implicitHeight + 6
                        radius: 4
                        color: Qt.rgba(Colors.textFaint.r, Colors.textFaint.g, Colors.textFaint.b, 0.10)
                        border.color: Colors.borderMuted
                        border.width: 1

                        Text {
                            id: dotcText
                            anchors.centerIn: parent
                            text: qsTr("Delete after fire")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 1
                        }
                    }

                    FlatButton {
                        text: qsTr("Edit")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.wakeAlarmDialog.openForEdit(alarmCol.parent.modelData)
                    }
                    FlatButton {
                        text: qsTr("Delete")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        destructive: true
                        onClicked: {
                            wakeAlarmConfirmDialog.pendingInstanceId =
                                alarmCol.parent.modelData.instanceId || "";
                            wakeAlarmConfirmDialog.ask(
                                qsTr("Delete alarm"),
                                qsTr("Remove %1 from the AMT alarm clock. The wake will not fire.")
                                    .arg(alarmCol.parent.modelData.elementName
                                         || alarmCol.parent.modelData.instanceId
                                         || qsTr("the alarm")),
                                qsTr("Delete"),
                                true);
                        }
                    }
                }

                Text {
                    text: qsTr("Wakes at %1").arg(alarmCol.parent.modelData.startTimeLocal)
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                }
                Text {
                    visible: (alarmCol.parent.modelData.intervalLabel || "").length > 0
                    text: qsTr("and recurs every %1").arg(alarmCol.parent.modelData.intervalLabel)
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                }
            }
        }
    }
}
