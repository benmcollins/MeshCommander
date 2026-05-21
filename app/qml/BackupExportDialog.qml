// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Two-step export flow:
///   1. `start()` opens a save-file picker.
///   2. Once a destination is chosen, the password modal opens.
///   3. On submit, calls `BackupController.exportTo(url, password)`.
///
/// Single root Item so Main.qml can drop a single id-bearing element
/// rather than two parallel dialogs. The visible UI is the inner
/// modal `passwordDialog`; the FileDialog is borrowed from the
/// platform.
Item {
    id: root

    /// Minimum acceptable password length. 8 is a deliberately low
    /// floor — the KDF cost (#5) is what really gates a brute force,
    /// and we don't want to lock out users with their existing
    /// password manager picks.
    readonly property int minPasswordLength: 8

    /// Last path picked from the FileDialog, retained so the password
    /// modal knows where to write.
    property url destinationFile

    property string draftPassword
    property string draftConfirm
    property bool revealPassword: false
    property string statusText

    function start() {
        draftPassword = "";
        draftConfirm = "";
        revealPassword = false;
        statusText = "";
        destinationFile = "";
        saveDialog.open();
    }

    function passwordsAgree() {
        return draftPassword.length >= minPasswordLength
               && draftPassword === draftConfirm;
    }

    function submitIfReady() {
        if (!passwordsAgree()) return;
        // exportTo is synchronous today; using the bool return value
        // (rather than polling `state` afterwards) means this still
        // does the right thing if encode/IO is ever moved off-thread.
        if (BackupController.exportTo(destinationFile, draftPassword)) {
            passwordDialog.accept();
        } else {
            // Surface the controller's error inline; keep dialog open so
            // the user can retry without rewinding the file picker.
            root.statusText = BackupController.message;
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Export machine list…")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "qumesh-backup"
        nameFilters: [qsTr("QuMesh backup (*.qumesh-backup)"), qsTr("All files (*)")]
        onAccepted: {
            root.destinationFile = saveDialog.selectedFile;
            passwordDialog.open();
        }
    }

    Dialog {
        id: passwordDialog

        title: qsTr("Encrypt backup")
        modal: true
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        implicitWidth: 480

        onClosed: {
            // Clear the password so it doesn't sit in memory longer
            // than necessary. The controller has the value it needed.
            root.draftPassword = "";
            root.draftConfirm = "";
            root.statusText = "";
        }

        contentItem: ColumnLayout {
            spacing: 14

            // Dialog extends Popup, not Item, so Accessible.* must
            // sit on the contentItem (#423 pattern).
            Accessible.role: Accessible.Dialog
            Accessible.name: passwordDialog.title

            Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
            Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

            Text {
                text: qsTr("Pick a password to protect the export. You'll need it to import this file later.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Section {
                title: qsTr("PASSWORD")
                Layout.fillWidth: true

                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Password")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 110
                    }
                    RowLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        TextField {
                            echoMode: root.revealPassword ? TextInput.Normal : TextInput.Password
                            placeholderText: qsTr("8+ characters")
                            text: root.draftPassword
                            font.family: Type.mono
                            font.pixelSize: Type.sizeM
                            Layout.fillWidth: true
                            Accessible.name: qsTr("Backup password")
                            onTextEdited: root.draftPassword = text
                        }
                        FlatButton {
                            text: root.revealPassword ? qsTr("Hide") : qsTr("Show")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: root.revealPassword = !root.revealPassword
                        }
                    }

                    Text {
                        text: qsTr("Confirm")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 110
                    }
                    TextField {
                        echoMode: root.revealPassword ? TextInput.Normal : TextInput.Password
                        text: root.draftConfirm
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        color: root.passwordsAgree() || root.draftConfirm.length === 0
                               ? Colors.text
                               : Colors.standby
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Confirm password")
                        onTextEdited: root.draftConfirm = text
                    }
                }
            }

            Text {
                visible: root.statusText.length > 0
                text: root.statusText
                color: Colors.standby
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Includes every machine in the list with its passwords and SSH credentials. Keep the password somewhere safe — there's no recovery if you lose it.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Cancel")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: passwordDialog.reject()
                }
                AccentButton {
                    text: qsTr("Export")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: root.passwordsAgree()
                    onClicked: root.submitIfReady()
                }
            }
        }
    }
}
