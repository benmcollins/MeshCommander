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
    signal openSetupBin()
    signal toggleTheme()
    signal openAbout()

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

        FlatButton {
            text: qsTr("Setup.bin")
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 1
            ToolTip.visible: hovered
            ToolTip.text: qsTr("USB-key provisioning file editor")
            onClicked: root.openSetupBin()
        }

        // Hidden when no auto-update backend is wired in (non-Apple
        // builds or QUMESH_AUTOUPDATE=OFF). Sparkle handles the entire
        // dialog flow; the button just kicks off a manual check.
        FlatButton {
            visible: Updater.available()
            text: qsTr("Updates")
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 1
            onClicked: Updater.checkForUpdates()
        }

        // Version chip on the far right. Clickable to open the About
        // dialog (#246). Muted so it reads as metadata, not a button.
        FlatButton {
            id: versionChip
            text: qsTr("v%1").arg(AppInfo.version)
            font.family: Type.mono
            font.pixelSize: Type.sizeXs
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("About QuMesh")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("About QuMesh")
            onClicked: root.openAbout()
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
