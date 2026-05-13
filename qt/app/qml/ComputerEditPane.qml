// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

Rectangle {
    id: root

    property int row: -1

    readonly property bool hasSelection: row >= 0 && row < ComputerModel.rowCount()
    property bool isNew: false

    property string draftName
    property string draftHost
    property int draftPort: 16992
    property string draftUser
    property string draftPass
    property bool draftTls: false
    property bool revealPass: false

    color: Colors.bg

    function startNewComputer() {
        root.isNew = true;
        root.draftName = "";
        root.draftHost = "";
        root.draftPort = 16992;
        root.draftUser = "";
        root.draftPass = "";
        root.draftTls = false;
        root.revealPass = false;
    }

    function loadFromRow() {
        if (!root.hasSelection) return;
        const idx = ComputerModel.index(root.row, 0);
        root.isNew = false;
        root.revealPass = false;
        root.draftName = ComputerModel.data(idx, ComputerModel.NameRole) || "";
        root.draftHost = ComputerModel.data(idx, ComputerModel.HostRole) || "";
        root.draftPort = ComputerModel.data(idx, ComputerModel.PortRole) || 16992;
        root.draftUser = ComputerModel.data(idx, ComputerModel.UserRole) || "";
        root.draftPass = ComputerModel.data(idx, ComputerModel.PassRole) || "";
        root.draftTls = ComputerModel.data(idx, ComputerModel.TlsRole) || false;
    }

    onRowChanged: if (root.hasSelection) root.loadFromRow()

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        Flickable {
            id: flick
            clip: true
            contentWidth: width
            contentHeight: form.implicitHeight + 32
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                id: form
                spacing: 24
                enabled: root.hasSelection || root.isNew
                width: flick.width

                Section {
                    title: qsTr("IDENTITY")
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

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
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

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
                            text: qsTr("Port")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.preferredWidth: 80
                        }
                        SpinBox {
                            from: 1
                            to: 65535
                            value: root.draftPort
                            editable: true
                            font.family: Type.mono
                            font.pixelSize: Type.sizeM
                            onValueModified: root.draftPort = value
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
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 24

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

                            Button {
                                text: root.revealPass ? qsTr("Hide") : qsTr("Show")
                                flat: true
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                onClicked: root.revealPass = !root.revealPass
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            color: Colors.border
            implicitHeight: 1
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.margins: 12

            Item { Layout.fillWidth: true }

            Button {
                visible: root.isNew
                text: qsTr("Cancel")
                flat: true
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    root.isNew = false;
                    if (root.hasSelection) root.loadFromRow();
                }
            }

            Button {
                text: root.isNew ? qsTr("Add machine") : qsTr("Save changes")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.draftName.length > 0 && root.draftHost.length > 0
                         && (root.hasSelection || root.isNew)
                onClicked: {
                    if (root.isNew) {
                        const newRow = ComputerModel.addComputer(root.draftName, root.draftHost,
                            root.draftPort, root.draftUser, root.draftPass, root.draftTls);
                        if (newRow >= 0) {
                            root.isNew = false;
                            root.row = newRow;
                        }
                    } else if (root.hasSelection) {
                        const idx = ComputerModel.index(root.row, 0);
                        ComputerModel.setData(idx, root.draftName, ComputerModel.NameRole);
                        ComputerModel.setData(idx, root.draftHost, ComputerModel.HostRole);
                        ComputerModel.setData(idx, root.draftPort, ComputerModel.PortRole);
                        ComputerModel.setData(idx, root.draftUser, ComputerModel.UserRole);
                        ComputerModel.setData(idx, root.draftPass, ComputerModel.PassRole);
                        ComputerModel.setData(idx, root.draftTls, ComputerModel.TlsRole);
                    }
                }
            }
        }
    }

    ColumnLayout {
        visible: !root.hasSelection && !root.isNew
        spacing: 6
        anchors.centerIn: parent

        Text {
            text: "◇"
            color: Colors.textFaint
            font.pixelSize: 48
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: qsTr("QUMESH")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 3
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: qsTr("Select a machine, or click + to add one.")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
        }
    }
}
