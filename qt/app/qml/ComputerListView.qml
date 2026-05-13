// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

Rectangle {
    id: root

    readonly property int currentRow: list.currentIndex

    signal addRequested

    color: Colors.bg

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        ListView {
            id: list
            model: ComputerModel
            clip: true
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: Motion.fast
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: rowItem

                required property int index
                required property string name
                required property string host
                required property int port

                width: list.width
                height: 48
                color: list.currentIndex === rowItem.index
                       ? Colors.accentSoft
                       : (hoverHandler.hovered ? Colors.elevated : "transparent")

                Behavior on color { ColorAnimation { duration: Motion.fast } }

                HoverHandler { id: hoverHandler }
                TapHandler { onTapped: list.currentIndex = rowItem.index }

                Rectangle {
                    color: Colors.accent
                    implicitWidth: 2
                    visible: list.currentIndex === rowItem.index
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                }

                RowLayout {
                    spacing: 12
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12

                    StatusLed {
                        ledState: "unknown"
                    }

                    ColumnLayout {
                        spacing: 1
                        Layout.fillWidth: true

                        Text {
                            text: rowItem.name.length > 0
                                  ? rowItem.name
                                  : qsTr("Unnamed")
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeM
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "%1:%2".arg(rowItem.host).arg(rowItem.port)
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                Rectangle {
                    color: Colors.borderMuted
                    height: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                }
            }

            Text {
                visible: list.count === 0
                opacity: 0.6
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No machines yet.\nClick + to add one.")
                anchors.centerIn: parent
            }
        }

        Rectangle {
            color: Colors.border
            implicitHeight: 1
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.margins: 8

            Button {
                text: "+"
                font.family: Type.mono
                font.pixelSize: Type.sizeL
                implicitWidth: 32
                implicitHeight: 28
                ToolTip.text: qsTr("Add machine")
                ToolTip.visible: hovered
                onClicked: root.addRequested()
            }

            Button {
                text: "−"
                font.family: Type.mono
                font.pixelSize: Type.sizeL
                implicitWidth: 32
                implicitHeight: 28
                enabled: list.currentIndex >= 0
                ToolTip.text: qsTr("Remove selected")
                ToolTip.visible: hovered
                onClicked: ComputerModel.removeAt(list.currentIndex)
            }

            Item { Layout.fillWidth: true }

            Text {
                text: list.count === 1
                      ? qsTr("1 machine")
                      : qsTr("%1 machines").arg(list.count)
                color: Colors.textFaint
                font.family: Type.mono
                font.pixelSize: Type.sizeXs
            }
        }
    }
}
