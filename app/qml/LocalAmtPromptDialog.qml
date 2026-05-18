// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Prompt the operator for the local AMT firmware credentials when
/// they click the "This PC" row in the sidebar. AMT on the local
/// machine is reached over the LMS loopback; QuMesh stores the
/// resulting ComputerInfo with host = 127.0.0.1 + the TLS flag the
/// probe surfaced.
Dialog {
    id: root

    property string draftUser: "admin"
    property string draftPassword
    property bool revealPassword: false
    property bool useTls: true

    /// Emitted when the operator confirms. The caller passes the
    /// credentials to ComputerModel.addComputer and opens details.
    signal credentialsConfirmed(string user, string password, bool tls)

    function openForLocal(tlsAvailable) {
        draftUser = "admin";
        draftPassword = "";
        useTls = tlsAvailable === true;
        revealPassword = false;
        open();
    }

    title: qsTr("Manage this PC over Intel AMT")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 480

    contentItem: ColumnLayout {
        spacing: 14

        Section {
            title: qsTr("CREDENTIALS")
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
                    Layout.preferredWidth: 100
                }
                TextField {
                    text: root.draftUser
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftUser = text
                }

                Text {
                    text: qsTr("Password")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 100
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

                Text {
                    text: qsTr("TLS")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 100
                }
                Switch {
                    checked: root.useTls
                    onCheckedChanged: root.useTls = checked
                }
            }
        }

        Text {
            text: qsTr("AMT on this machine is reachable through Intel's Local Manageability Service on the loopback. The saved entry will use host 127.0.0.1.")
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
                text: qsTr("Connect")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.draftUser.length > 0
                onClicked: {
                    root.credentialsConfirmed(root.draftUser,
                                              root.draftPassword,
                                              root.useTls);
                    root.accept();
                }
            }
        }
    }
}
