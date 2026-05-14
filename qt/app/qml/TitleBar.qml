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

        Item { Layout.fillWidth: true }

        Button {
            text: Colors.dark ? "☀" : "☾"
            flat: true
            font.family: Type.sans
            font.pixelSize: Type.sizeM
            ToolTip.visible: hovered
            ToolTip.text: Colors.dark
                ? qsTr("Switch to light theme")
                : qsTr("Switch to dark theme")
            onClicked: root.toggleTheme()
        }

        Button {
            text: qsTr("Certificates")
            flat: true
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
