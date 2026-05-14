// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
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

    function openFor(targetRow) {
        row = targetRow;
        if (targetRow >= 0) {
            const idx = ComputerModel.index(targetRow, 0);
            draftName = ComputerModel.data(idx, ComputerModel.NameRole) || "";
            draftHost = ComputerModel.data(idx, ComputerModel.HostRole) || "";
            draftUser = ComputerModel.data(idx, ComputerModel.UserRole) || "";
            draftPass = ComputerModel.data(idx, ComputerModel.PassRole) || "";
            draftTls  = ComputerModel.data(idx, ComputerModel.TlsRole) || false;
        } else {
            draftName = "";
            draftHost = "";
            draftUser = "";
            draftPass = "";
            draftTls = false;
        }
        revealPass = false;
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
                    root.computerSaved(savedRow);
                    root.accept();
                }
            }
        }
    }
}
