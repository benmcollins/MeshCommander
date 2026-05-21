// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Shown when the Linux Updater finds a newer release on the appcast
/// feed. apt does the actual install — this dialog only surfaces the
/// version + release notes and points the user at the two ways forward:
/// the GitHub Release page or a copyable `apt` one-liner. The macOS /
/// Windows builds never instantiate this dialog: Sparkle/WinSparkle
/// own their own UI for the same flow.
Dialog {
    id: root

    property string version: ""
    property string notesHtml: ""
    property string releaseUrl: ""

    title: qsTr("Update available")
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560
    implicitHeight: Math.min((parent ? parent.height : 720) * 0.85, 600)

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        RowLayout {
            spacing: 12
            Layout.fillWidth: true

            AppMark {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Text {
                    text: qsTr("QuMesh %1 is available").arg(root.version)
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeL
                    font.weight: Font.Medium
                }
                Text {
                    text: qsTr("You're running v%1.").arg(AppInfo.version)
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                }
            }
        }

        Rectangle {
            color: Colors.surface
            border.color: Colors.border
            border.width: 1
            radius: 6
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 10
                clip: true

                TextEdit {
                    width: parent ? parent.width : 0
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    text: root.notesHtml.length > 0
                        ? root.notesHtml
                        : qsTr("<i>No release notes available.</i>")
                }
            }
        }

        // apt one-liner for users who've added the QuMesh APT repo.
        // We don't shell out to apt directly (it needs sudo + an
        // interactive prompt) — copy the command, paste in a
        // terminal. Same UX pattern Cloudflare and others use.
        Rectangle {
            color: Colors.bg
            border.color: Colors.border
            border.width: 1
            radius: 6
            implicitHeight: aptRow.implicitHeight + 16
            Layout.fillWidth: true

            RowLayout {
                id: aptRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text {
                    text: "sudo apt update && sudo apt upgrade qumesh"
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                FlatButton {
                    id: copyButton
                    property bool copied: false
                    text: copied ? qsTr("Copied") : qsTr("Copy")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    onClicked: {
                        clipboardHelper.text =
                            "sudo apt update && sudo apt upgrade qumesh";
                        clipboardHelper.selectAll();
                        clipboardHelper.copy();
                        copied = true;
                        copyResetTimer.restart();
                    }
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
                font.pixelSize: Type.sizeXs
                onClicked: root.close()
            }

            AccentButton {
                text: qsTr("Open Release Page")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                enabled: root.releaseUrl.length > 0
                onClicked: Qt.openUrlExternally(root.releaseUrl)
            }
        }
    }

    // QML has no first-class "set clipboard" API in Qt 6; the standard
    // workaround is an invisible TextEdit that we drive programmatically.
    TextEdit {
        id: clipboardHelper
        visible: false
        width: 0
        height: 0
    }

    Timer {
        id: copyResetTimer
        interval: 1800
        onTriggered: copyButton.copied = false
    }
}
