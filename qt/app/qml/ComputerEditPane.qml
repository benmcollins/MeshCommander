pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshCommander

Item {
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

    function startNewComputer() {
        root.isNew = true;
        root.draftName = "";
        root.draftHost = "";
        root.draftPort = 16992;
        root.draftUser = "";
        root.draftPass = "";
        root.draftTls = false;
    }

    function loadFromRow() {
        if (!root.hasSelection) return;
        const idx = ComputerModel.index(root.row, 0);
        root.isNew = false;
        root.draftName = ComputerModel.data(idx, ComputerModel.NameRole) || "";
        root.draftHost = ComputerModel.data(idx, ComputerModel.HostRole) || "";
        root.draftPort = ComputerModel.data(idx, ComputerModel.PortRole) || 16992;
        root.draftUser = ComputerModel.data(idx, ComputerModel.UserRole) || "";
        root.draftPass = ComputerModel.data(idx, ComputerModel.PassRole) || "";
        root.draftTls = ComputerModel.data(idx, ComputerModel.TlsRole) || false;
    }

    onRowChanged: if (root.hasSelection) root.loadFromRow()

    GridLayout {
        columns: 2
        columnSpacing: 8
        rowSpacing: 8
        enabled: root.hasSelection || root.isNew
        anchors.fill: parent
        anchors.margins: 16

        Label { text: qsTr("Name") }
        TextField {
            placeholderText: qsTr("My AMT machine")
            text: root.draftName
            Layout.fillWidth: true
            onTextEdited: root.draftName = text
        }

        Label { text: qsTr("Host") }
        TextField {
            placeholderText: qsTr("10.0.0.5 or amt-01.example")
            text: root.draftHost
            Layout.fillWidth: true
            onTextEdited: root.draftHost = text
        }

        Label { text: qsTr("Port") }
        SpinBox {
            from: 1
            to: 65535
            value: root.draftPort
            editable: true
            onValueModified: root.draftPort = value
        }

        Label { text: qsTr("User") }
        TextField {
            placeholderText: qsTr("admin")
            text: root.draftUser
            Layout.fillWidth: true
            onTextEdited: root.draftUser = text
        }

        Label { text: qsTr("Password") }
        TextField {
            echoMode: TextInput.Password
            text: root.draftPass
            Layout.fillWidth: true
            onTextEdited: root.draftPass = text
        }

        Label { text: qsTr("TLS") }
        Switch {
            checked: root.draftTls
            onCheckedChanged: root.draftTls = checked
        }

        Item {
            Layout.columnSpan: 2
            Layout.fillHeight: true
        }

        RowLayout {
            spacing: 8
            Layout.columnSpan: 2
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            Button {
                visible: root.isNew
                text: qsTr("Cancel")
                onClicked: {
                    root.isNew = false;
                    if (root.hasSelection) root.loadFromRow();
                }
            }

            Button {
                text: root.isNew ? qsTr("Add") : qsTr("Save")
                enabled: root.draftName.length > 0 && root.draftHost.length > 0
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

    Label {
        visible: !root.hasSelection && !root.isNew
        opacity: 0.5
        text: qsTr("Select a computer or add a new one.")
        anchors.centerIn: parent
    }
}
