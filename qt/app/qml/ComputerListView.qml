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

    function setCurrentRow(row) { list.currentIndex = row; }

    signal addRequested
    signal openDetailsRequested(int row)

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

            // Stagger entrance for the first paint and any subsequent
            // inserts. Items fade in + slide up slightly so a fresh
            // model doesn't all appear at once.
            // `ViewTransition.index` is the row's position so each
            // delegate kicks off ~40 ms after the previous one.
            populate: Transition {
                SequentialAnimation {
                    PauseAnimation { duration: ViewTransition.index * 40 }
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
                        NumberAnimation { property: "y"; from: ViewTransition.item.y + 8; to: ViewTransition.item.y; duration: Motion.normal; easing.type: Easing.OutCubic }
                    }
                }
            }
            add: Transition {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
                    NumberAnimation { property: "y"; from: ViewTransition.item.y + 8; to: ViewTransition.item.y; duration: Motion.normal; easing.type: Easing.OutCubic }
                }
            }
            displaced: Transition {
                NumberAnimation { properties: "y"; duration: Motion.fast }
            }

            delegate: Rectangle {
                id: rowItem

                required property int index
                required property string name
                required property string host

                width: list.width
                height: 48
                color: list.currentIndex === rowItem.index
                       ? Colors.accentSoft
                       : (hoverHandler.hovered ? Colors.elevated : "transparent")

                Behavior on color { ColorAnimation { duration: Motion.fast } }

                HoverHandler { id: hoverHandler }
                TapHandler {
                    onTapped: list.currentIndex = rowItem.index
                    onDoubleTapped: root.openDetailsRequested(rowItem.index)
                }

                Rectangle {
                    color: Colors.accent
                    implicitWidth: 2
                    visible: list.currentIndex === rowItem.index
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                }

                ColumnLayout {
                    spacing: 1
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6

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
                        text: rowItem.host
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
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
