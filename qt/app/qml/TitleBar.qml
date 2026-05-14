// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

Rectangle {
    id: root

    signal openCertificates()
    signal toggleTheme()

    color: Colors.surface
    implicitHeight: 36

    RowLayout {
        spacing: 12
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16

        AppMark {
            small: true
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }

        Text {
            text: "QUMESH"
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }

        Rectangle {
            color: Colors.borderMuted
            implicitWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
        }

        Text {
            text: ComputerModel.rowCount() === 1
                  ? qsTr("1 machine")
                  : qsTr("%1 machines").arg(ComputerModel.rowCount())
            color: Colors.textMuted
            font.family: Type.mono
            font.pixelSize: Type.sizeXs
        }

        // Fleet-health chips — only the non-zero states render, so an
        // all-offline fleet doesn't show a stale "0 on" placeholder.
        Repeater {
            model: [
                { count: ComputerModel.countOn,
                  color: Colors.on,
                  label: qsTr("on") },
                { count: ComputerModel.countOff,
                  color: Colors.off,
                  label: qsTr("off") },
                { count: ComputerModel.countStandby,
                  color: Colors.standby,
                  label: qsTr("standby") },
                { count: ComputerModel.countUnreachable,
                  color: Colors.error,
                  label: qsTr("unreachable") },
            ]

            delegate: Row {
                id: chip
                required property var modelData
                spacing: 4
                visible: chip.modelData.count > 0

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: chip.modelData.color
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "%1 %2".arg(chip.modelData.count).arg(chip.modelData.label)
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    font.features: ({ "tnum": 1 })
                }
            }
        }

        Item { Layout.fillWidth: true }

        FlatButton {
            id: themeToggle
            implicitWidth: 28
            implicitHeight: 24
            padding: 4
            // Stock Button.icon would re-colorize the SVG; the baked
            // assets already carry the correct tint per theme, so use
            // a contentItem Image and let icon stay empty.
            contentItem: Image {
                source: Colors.dark
                    ? "qrc:/icons/icon-sun-light.svg"
                    : "qrc:/icons/icon-moon-slate.svg"
                sourceSize.width: 18
                sourceSize.height: 18
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            Accessible.role: Accessible.Button
            Accessible.name: Colors.dark
                ? qsTr("Switch to light theme")
                : qsTr("Switch to dark theme")
            ToolTip.visible: hovered
            ToolTip.text: themeToggle.Accessible.name
            onClicked: root.toggleTheme()
        }

        FlatButton {
            text: qsTr("Certificates")
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 1
            onClicked: root.openCertificates()
        }
    }

    Rectangle {
        color: Colors.border
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
