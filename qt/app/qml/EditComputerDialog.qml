// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Modal form for adding a new machine or editing an existing one.
/// `row >= 0` edits the row at that index; `row === -1` creates a
/// new machine. Open with `openFor(row)`.
Dialog {
    id: root

    property int row: -1
    readonly property bool isNew: row < 0

    property string draftName
    property string draftHost
    property string draftUser
    property string draftPass
    property bool draftTls: false
    property bool revealPass: false

    // SSH tunnel
    property bool draftSshEnabled: false
    property string draftSshHost
    property int draftSshPort: 22
    property string draftSshUser
    property int draftSshAuthMode: 0  // 0 = password, 1 = key
    property string draftSshPassword
    property string draftSshKeyPath
    property string draftSshKeyPassphrase
    property bool revealSshPass: false
    property bool revealSshKeyPass: false

    function openFor(targetRow) {
        row = targetRow;
        if (targetRow >= 0) {
            const idx = ComputerModel.index(targetRow, 0);
            draftName = ComputerModel.data(idx, ComputerModel.NameRole) || "";
            draftHost = ComputerModel.data(idx, ComputerModel.HostRole) || "";
            draftUser = ComputerModel.data(idx, ComputerModel.UserRole) || "";
            draftPass = ComputerModel.data(idx, ComputerModel.PassRole) || "";
            draftTls  = ComputerModel.data(idx, ComputerModel.TlsRole) || false;
            const ssh = ComputerModel.sshConfigFor(targetRow) || ({});
            draftSshEnabled = ssh.enabled || false;
            draftSshHost = ssh.host || "";
            draftSshPort = ssh.port || 22;
            draftSshUser = ssh.user || "";
            draftSshAuthMode = ssh.authMode || 0;
            draftSshPassword = ssh.password || "";
            draftSshKeyPath = ssh.keyPath || "";
            draftSshKeyPassphrase = ssh.keyPassphrase || "";
        } else {
            draftName = "";
            draftHost = "";
            draftUser = "";
            draftPass = "";
            draftTls = false;
            draftSshEnabled = false;
            draftSshHost = "";
            draftSshPort = 22;
            draftSshUser = "";
            draftSshAuthMode = 0;
            draftSshPassword = "";
            draftSshKeyPath = "";
            draftSshKeyPassphrase = "";
        }
        revealPass = false;
        revealSshPass = false;
        revealSshKeyPass = false;
        open();
    }

    signal computerSaved(int row)

    title: isNew ? qsTr("Add machine") : qsTr("Edit machine")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 520

    contentItem: ColumnLayout {
        spacing: 14

        Section {
            title: qsTr("IDENTITY")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Name")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 80
                }
                TextField {
                    placeholderText: qsTr("My AMT machine")
                    text: root.draftName
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftName = text
                }
            }
        }

        Section {
            title: qsTr("CONNECTION")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Host")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 80
                }
                TextField {
                    placeholderText: "10.0.0.5"
                    text: root.draftHost
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftHost = text
                }

                Text {
                    text: qsTr("TLS")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 80
                }
                Switch {
                    checked: root.draftTls
                    onCheckedChanged: root.draftTls = checked
                }
            }
        }

        Section {
            title: qsTr("AUTHENTICATION")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("User")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 80
                }
                TextField {
                    placeholderText: "admin"
                    text: root.draftUser
                    color: Colors.text
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
                    Layout.preferredWidth: 80
                }
                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true

                    TextField {
                        echoMode: root.revealPass ? TextInput.Normal : TextInput.Password
                        text: root.draftPass
                        color: Colors.text
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftPass = text
                    }
                    FlatButton {
                        text: root.revealPass ? qsTr("Hide") : qsTr("Show")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.revealPass = !root.revealPass
                    }
                }
            }
        }

        Section {
            title: qsTr("SSH TUNNEL")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Route via SSH")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.fillWidth: true
                    }
                    Switch {
                        checked: root.draftSshEnabled
                        onCheckedChanged: root.draftSshEnabled = checked
                    }
                }

                Text {
                    visible: root.draftSshEnabled
                    text: qsTr("WSMAN traffic to the AMT host is encapsulated in a direct-tcpip channel over SSH. No port is bound on the local machine.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                GridLayout {
                    visible: root.draftSshEnabled
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("SSH host")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    TextField {
                        placeholderText: "jump.example.com"
                        text: root.draftSshHost
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftSshHost = text
                    }

                    Text {
                        text: qsTr("SSH port")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    TextField {
                        text: root.draftSshPort
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.preferredWidth: 80
                        validator: IntValidator { bottom: 1; top: 65535 }
                        onTextEdited: root.draftSshPort = parseInt(text) || 22
                    }

                    Text {
                        text: qsTr("SSH user")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    TextField {
                        placeholderText: "ubuntu"
                        text: root.draftSshUser
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftSshUser = text
                    }

                    Text {
                        text: qsTr("Auth")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true
                        RadioButton {
                            text: qsTr("Password")
                            checked: root.draftSshAuthMode === 0
                            onClicked: root.draftSshAuthMode = 0
                        }
                        RadioButton {
                            text: qsTr("Private key")
                            checked: root.draftSshAuthMode === 1
                            onClicked: root.draftSshAuthMode = 1
                        }
                    }

                    // Password row (visible for password auth)
                    Text {
                        visible: root.draftSshAuthMode === 0
                        text: qsTr("Password")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    RowLayout {
                        visible: root.draftSshAuthMode === 0
                        spacing: 6
                        Layout.fillWidth: true
                        TextField {
                            echoMode: root.revealSshPass ? TextInput.Normal : TextInput.Password
                            text: root.draftSshPassword
                            font.family: Type.mono
                            font.pixelSize: Type.sizeM
                            Layout.fillWidth: true
                            onTextEdited: root.draftSshPassword = text
                        }
                        FlatButton {
                            text: root.revealSshPass ? qsTr("Hide") : qsTr("Show")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: root.revealSshPass = !root.revealSshPass
                        }
                    }

                    // Key path row (visible for key auth)
                    Text {
                        visible: root.draftSshAuthMode === 1
                        text: qsTr("Key file")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    RowLayout {
                        visible: root.draftSshAuthMode === 1
                        spacing: 6
                        Layout.fillWidth: true
                        TextField {
                            placeholderText: "~/.ssh/id_ed25519"
                            text: root.draftSshKeyPath
                            font.family: Type.mono
                            font.pixelSize: Type.sizeM
                            Layout.fillWidth: true
                            onTextEdited: root.draftSshKeyPath = text
                        }
                        FlatButton {
                            text: qsTr("Browse…")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: keyDialog.open()
                        }
                    }

                    // Key passphrase (visible for key auth)
                    Text {
                        visible: root.draftSshAuthMode === 1
                        text: qsTr("Passphrase")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 100
                    }
                    RowLayout {
                        visible: root.draftSshAuthMode === 1
                        spacing: 6
                        Layout.fillWidth: true
                        TextField {
                            echoMode: root.revealSshKeyPass ? TextInput.Normal : TextInput.Password
                            placeholderText: qsTr("Optional")
                            text: root.draftSshKeyPassphrase
                            font.family: Type.mono
                            font.pixelSize: Type.sizeM
                            Layout.fillWidth: true
                            onTextEdited: root.draftSshKeyPassphrase = text
                        }
                        FlatButton {
                            text: root.revealSshKeyPass ? qsTr("Hide") : qsTr("Show")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: root.revealSshKeyPass = !root.revealSshKeyPass
                        }
                    }
                }
            }
        }

        FileDialog {
            id: keyDialog
            title: qsTr("Select SSH private key")
            onAccepted: root.draftSshKeyPath = Paths.urlToLocalFile(keyDialog.selectedFile)
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
                text: root.isNew ? qsTr("Add machine") : qsTr("Save changes")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
enabled: root.draftName.length > 0 && root.draftHost.length > 0
                onClicked: {
                    let savedRow = root.row;
                    if (root.isNew) {
                        savedRow = ComputerModel.addComputer(root.draftName,
                            root.draftHost,
                            root.draftUser, root.draftPass, root.draftTls);
                        if (savedRow < 0) return;
                    } else if (root.row >= 0) {
                        const idx = ComputerModel.index(root.row, 0);
                        ComputerModel.setData(idx, root.draftName, ComputerModel.NameRole);
                        ComputerModel.setData(idx, root.draftHost, ComputerModel.HostRole);
                        ComputerModel.setData(idx, root.draftUser, ComputerModel.UserRole);
                        ComputerModel.setData(idx, root.draftPass, ComputerModel.PassRole);
                        ComputerModel.setData(idx, root.draftTls, ComputerModel.TlsRole);
                    }
                    ComputerModel.setSshConfig(savedRow, {
                        "enabled": root.draftSshEnabled,
                        "host": root.draftSshHost,
                        "port": root.draftSshPort,
                        "user": root.draftSshUser,
                        "authMode": root.draftSshAuthMode,
                        "password": root.draftSshPassword,
                        "keyPath": root.draftSshKeyPath,
                        "keyPassphrase": root.draftSshKeyPassphrase,
                    });
                    root.computerSaved(savedRow);
                    root.accept();
                }
            }
        }
    }
}
