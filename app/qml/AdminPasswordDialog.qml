// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Rotate the AMT admin entry's username and/or password. AMT exposes
/// `SetAdminAclEntryEx(Username, DigestPassword)`; realms are fixed
/// for the admin and not editable. Risky operation — locks the
/// operator out if the new credentials are wrong.
Dialog {
    id: root

    required property var controller

    property string draftUsername: "admin"
    property string draftPassword
    property string draftPasswordConfirm
    property bool revealPassword: false

    function openForRotate(currentUsername) {
        draftUsername = (currentUsername && currentUsername.length > 0)
                        ? currentUsername : "admin";
        draftPassword = "";
        draftPasswordConfirm = "";
        revealPassword = false;
        open();
    }

    title: qsTr("Rotate admin credentials")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 480

    function passwordsAgree() {
        return draftPassword.length > 0
               && draftPassword === draftPasswordConfirm;
    }

    /// Centralised primary-action gate so the AccentButton and the
    /// Enter-key handler stay in sync — #279.
    function submitIfReady() {
        if (root.draftUsername.length === 0 || !root.passwordsAgree()) return;
        root.controller.setAdminPassword(root.draftUsername, root.draftPassword);
        root.accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("ADMIN")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Username")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    text: root.draftUsername
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftUsername = text
                }

                Text {
                    text: qsTr("New password")
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
                        placeholderText: qsTr("Required")
                        text: root.draftPassword
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
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
                    text: root.draftPasswordConfirm
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    color: root.passwordsAgree() ? Colors.text : Colors.standby
                    Layout.fillWidth: true
                    onTextEdited: root.draftPasswordConfirm = text
                }
            }
        }

        Text {
            text: qsTr("Locks the AMT device behind the new credentials. If they're wrong you will not be able to reconnect without USB re-provisioning.")
            color: Colors.standby
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
                onClicked: root.reject()
            }
            AccentButton {
                text: qsTr("Rotate")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.draftUsername.length > 0
                         && root.passwordsAgree()
                onClicked: root.submitIfReady()
            }
        }
    }
}
