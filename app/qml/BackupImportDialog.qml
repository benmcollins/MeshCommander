// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Three-step import flow:
///   1. `start()` opens a file picker.
///   2. Password modal — `BackupController.prepareImport` runs on submit.
///   3. On `NeedsConfirm`, the password modal flips to a preview that
///      shows N new / M updated and a final confirm button.
///   `cancelImport()` resets staged state.
Item {
    id: root

    property url sourceFile
    property string draftPassword
    property bool revealPassword: false

    function start() {
        draftPassword = "";
        revealPassword = false;
        sourceFile = "";
        BackupController.cancelImport();
        openDialog.open();
    }

    /// True once `prepareImport` succeeds — the modal flips from
    /// "enter password" to "confirm preview".
    readonly property bool inPreview:
        BackupController.state === BackupController.State.NeedsConfirm

    function submitPassword() {
        if (root.draftPassword.length === 0) return;
        BackupController.prepareImport(sourceFile, draftPassword);
    }

    function applyAndDismiss() {
        if (BackupController.applyImport()) {
            passwordDialog.accept();
        }
    }

    function errorCopy(code) {
        // Mirrors BackupController::Error. Keep in sync.
        switch (code) {
        case BackupController.Error.BadFile:
            return qsTr("Couldn't open the file.");
        case BackupController.Error.BadFormat:
            return qsTr("This doesn't look like a QuMesh backup file.");
        case BackupController.Error.BadPassword:
            return qsTr("Incorrect password.");
        case BackupController.Error.Corrupt:
            return qsTr("Backup file is damaged or has been modified.");
        case BackupController.Error.SaveFailed:
            return qsTr("Couldn't save the imported machines.");
        case BackupController.Error.Internal:
            return qsTr("Something went wrong unsealing the backup.");
        }
        return "";
    }

    FileDialog {
        id: openDialog
        title: qsTr("Import machine list…")
        nameFilters: [qsTr("QuMesh backup (*.qumesh-backup)"), qsTr("All files (*)")]
        onAccepted: {
            root.sourceFile = openDialog.selectedFile;
            passwordDialog.open();
        }
    }

    Dialog {
        id: passwordDialog

        title: root.inPreview
               ? qsTr("Confirm import")
               : qsTr("Unlock backup")
        modal: true
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        implicitWidth: 480

        onClosed: {
            root.draftPassword = "";
            // Always reset the controller — `cancelImport()` is a no-op
            // from Idle/Done, so this is safe regardless of how the
            // dialog was dismissed (Esc, Cancel, after a successful
            // Import). Pre-fix the state-specific guard skipped reset
            // during a hypothetical Working-state dismiss.
            BackupController.cancelImport();
        }

        contentItem: ColumnLayout {
            spacing: 14

            Accessible.role: Accessible.Dialog
            Accessible.name: passwordDialog.title

            Keys.onReturnPressed: function(event) {
                if (root.inPreview) root.applyAndDismiss();
                else                root.submitPassword();
                event.accepted = true;
            }
            Keys.onEnterPressed: function(event) {
                if (root.inPreview) root.applyAndDismiss();
                else                root.submitPassword();
                event.accepted = true;
            }

            // --- Stage 1: password entry --------------------------
            ColumnLayout {
                visible: !root.inPreview
                spacing: 14
                Layout.fillWidth: true

                Text {
                    text: qsTr("Enter the password used when this backup was created.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true
                    TextField {
                        echoMode: root.revealPassword ? TextInput.Normal : TextInput.Password
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
                    visible: BackupController.state === BackupController.State.Failed
                    text: root.errorCopy(BackupController.lastErrorCode)
                    color: Colors.standby
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            // --- Stage 2: preview ---------------------------------
            ColumnLayout {
                visible: root.inPreview
                       || (BackupController.state === BackupController.State.Failed
                           && BackupController.previewTotal > 0)
                spacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("%1 new, %2 updated")
                            .arg(BackupController.previewAddCount)
                            .arg(BackupController.previewUpdateCount)
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeL
                    font.weight: Font.Medium
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("New machines will be added. Existing machines that match by id, or by host + name, will be overwritten — including their TLS-pinned certificates and SSH host keys.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                // Surface applyImport-time failures (SaveFailed, etc.)
                // in the same stage so the user gets feedback instead
                // of the dialog silently staying open.
                Text {
                    visible: BackupController.state === BackupController.State.Failed
                    text: root.errorCopy(BackupController.lastErrorCode)
                    color: Colors.standby
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
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
                    text: root.inPreview ? qsTr("Import") : qsTr("Unlock")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: root.inPreview
                             ? BackupController.previewTotal > 0
                             : root.draftPassword.length > 0
                    onClicked: {
                        if (root.inPreview) root.applyAndDismiss();
                        else                root.submitPassword();
                    }
                }
            }
        }
    }
}
