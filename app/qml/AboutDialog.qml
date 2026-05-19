// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal "About QuMesh" dialog. Opened from the version chip in the
/// title bar. Shows version, build date, author, and the full Apache-2.0
/// license text. The license text is embedded as a Qt resource so the
/// dialog works the same in dev runs, macOS bundles, and the Windows
/// installer layout.
Dialog {
    id: root

    title: qsTr("About QuMesh")
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560
    implicitHeight: Math.min((parent ? parent.height : 720) * 0.85, 680)

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            spacing: 14
            Layout.fillWidth: true

            AppMark {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text {
                    text: "QuMesh"
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeL
                    font.weight: Font.Medium
                }
                Text {
                    text: qsTr("v%1").arg(AppInfo.version)
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                }
                Text {
                    text: qsTr("Built %1").arg(AppInfo.buildDate)
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Text {
                    text: qsTr("Author: %1").arg(AppInfo.author)
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.topMargin: 4
                }
            }
        }

        Rectangle {
            color: Colors.border
            implicitHeight: 1
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("LICENSE")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }

        Rectangle {
            color: Colors.surface
            border.color: Colors.borderMuted
            border.width: 1
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    // LICENSE.md is Markdown — render it with headings,
                    // emphasis, and paragraph breaks instead of dumping
                    // raw `#` and `*` characters at the user.
                    textFormat: TextEdit.MarkdownText
                    text: AppInfo.licenseText
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    background: null
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            FlatButton {
                text: qsTr("Close")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.close()
            }
        }
    }
}
