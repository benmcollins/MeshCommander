// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit a Management Presence Server. CIRA tunnels target one
/// of these when the device is off-network. AMT supports both
/// external CIRA servers and CILA (local) servers — the mpsType
/// picker covers both. Auth credentials are optional and only sent on
/// the Add path; edits-after-the-fact leave credentials untouched.
Dialog {
    id: root

    required property var controller

    property bool isEdit: false
    property string editName

    property string draftAccessInfo
    property int draftPort: 4433
    property string draftCommonName
    property int draftMpsType: 0
    /// 1 = none, 2 = HTTP digest (MSCHAPv2-style).
    property int draftAuthMethod: 1
    property string draftUsername
    property string draftPassword
    property bool revealPassword: false

    function openForAdd() {
        isEdit = false;
        editName = "";
        draftAccessInfo = "";
        draftPort = 4433;
        draftCommonName = "";
        draftMpsType = 0;
        draftAuthMethod = 1;
        draftUsername = "";
        draftPassword = "";
        revealPassword = false;
        open();
    }

    function openForEdit(server) {
        isEdit = true;
        editName = server.name || "";
        draftAccessInfo = server.accessInfo || "";
        draftPort = server.port || 4433;
        draftCommonName = server.cn || "";
        draftMpsType = server.mpsType || 0;
        draftAuthMethod = 1;    // not editable here
        draftUsername = "";
        draftPassword = "";
        revealPassword = false;
        open();
    }

    title: isEdit ? qsTr("Edit MPS server") : qsTr("Add MPS server")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560

    function submitReady() {
        return draftAccessInfo.trim().length > 0
            && draftPort > 0 && draftPort <= 65535
            && (isEdit
                 || draftAuthMethod === 1
                 || (draftUsername.length > 0
                      && draftPassword.length > 0));
    }

    function submitIfReady() {
        if (!submitReady()) return;
        if (isEdit) {
            controller.updateMpServer(editName, {
                "accessInfo": draftAccessInfo,
                "port":       draftPort,
                "commonName": draftCommonName,
            });
        } else {
            controller.addMpServer({
                "accessInfo": draftAccessInfo,
                "port":       draftPort,
                "commonName": draftCommonName,
                "mpsType":    draftMpsType,
                "authMethod": draftAuthMethod,
                "username":   draftUsername,
                "password":   draftPassword,
            });
        }
        accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("SERVER")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Address")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    placeholderText: qsTr("mps.corp.example.com or 198.51.100.5")
                    text: root.draftAccessInfo
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftAccessInfo = text
                }

                Text {
                    text: qsTr("Port")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    text: root.draftPort
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.preferredWidth: 100
                    validator: IntValidator { bottom: 1; top: 65535 }
                    onTextEdited: root.draftPort = parseInt(text) || 0
                }

                Text {
                    text: qsTr("Trusted CN")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    placeholderText: qsTr("(optional) CN to require on the server cert")
                    text: root.draftCommonName
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftCommonName = text
                }

                Text {
                    text: qsTr("Kind")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("CIRA (external)")
                        enabled: !root.isEdit
                        checked: root.draftMpsType === 0
                        onClicked: root.draftMpsType = 0
                    }
                    RadioButton {
                        text: qsTr("CILA (local)")
                        enabled: !root.isEdit
                        checked: root.draftMpsType === 1
                        onClicked: root.draftMpsType = 1
                    }
                }
            }
        }

        Section {
            title: qsTr("AUTHENTICATION")
            visible: !root.isEdit
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("None")
                        checked: root.draftAuthMethod === 1
                        onClicked: root.draftAuthMethod = 1
                    }
                    RadioButton {
                        text: qsTr("HTTP digest")
                        checked: root.draftAuthMethod === 2
                        onClicked: root.draftAuthMethod = 2
                    }
                }

                GridLayout {
                    visible: root.draftAuthMethod !== 1
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
                }
            }
        }

        Text {
            visible: root.isEdit
            text: qsTr("Authentication credentials aren't editable here — to rotate them, remove and re-add the server.")
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
                onClicked: root.reject()
            }
            AccentButton {
                text: root.isEdit ? qsTr("Save changes") : qsTr("Add server")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.submitReady()
                onClicked: root.submitIfReady()
            }
        }
    }
}
