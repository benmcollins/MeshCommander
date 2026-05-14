// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Right-side pane: shows the live status and metadata for the
/// currently-selected machine plus action buttons (Open SOL, Mount
/// ISO, Open KVM) and an Edit settings action.
Rectangle {
    id: root

    property int row: -1
    readonly property bool hasSelection: row >= 0 && row < ComputerModel.rowCount()

    function _read(role) {
        return ComputerModel.data(ComputerModel.index(root.row, 0), role);
    }

    readonly property string machineName: hasSelection ? (_read(ComputerModel.NameRole) || "") : ""
    readonly property string machineHost: hasSelection ? (_read(ComputerModel.HostRole) || "") : ""
    readonly property int machinePort: hasSelection ? (_read(ComputerModel.PortRole) || 16992) : 16992
    readonly property string machineUser: hasSelection ? (_read(ComputerModel.UserRole) || "") : ""
    readonly property string machinePass: hasSelection ? (_read(ComputerModel.PassRole) || "") : ""
    readonly property bool machineTls: hasSelection ? (_read(ComputerModel.TlsRole) || false) : false
    readonly property var machineTrustedFingerprints: hasSelection
        ? (_read(ComputerModel.TrustedFingerprintsRole) || [])
        : []
    readonly property int machinePowerState: hasSelection
        ? (_read(ComputerModel.PowerStateRole) || 0) : 0

    readonly property string powerLabel: {
        switch (machinePowerState) {
        case 1: return qsTr("On");
        case 2: return qsTr("Off");
        case 3: return qsTr("Standby");
        case 4: return qsTr("Hibernate");
        case 5: return qsTr("Unreachable");
        default: return qsTr("Unknown");
        }
    }
    readonly property string powerLed: {
        switch (machinePowerState) {
        case 1: return "on";
        case 2: return "off";
        case 3:
        case 4: return "standby";
        case 5: return "error";
        default: return "unknown";
        }
    }

    signal addRequested

    color: Colors.bg

    EditComputerDialog {
        id: editDialog
        onComputerSaved: function(savedRow) {
            if (root.row !== savedRow) root.row = savedRow;
        }
    }

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
                visible: root.hasSelection
                width: flick.width

                ColumnLayout {
                    spacing: 6
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

                    Text {
                        text: root.machineName.length > 0
                              ? root.machineName
                              : qsTr("Unnamed")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: 22
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "%1:%2".arg(root.machineHost).arg(root.machinePort)
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeS
                    }
                }

                Section {
                    title: qsTr("POWER")
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

                    RowLayout {
                        spacing: 10
                        Layout.fillWidth: true

                        StatusLed {
                            ledState: root.powerLed
                            implicitWidth: 14
                            implicitHeight: 14
                        }
                        Text {
                            text: root.powerLabel
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeM
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("Polled every 10 s")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
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
                        rowSpacing: 6
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Endpoint")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                        Text {
                            text: "%1://%2:%3"
                                .arg(root.machineTls ? "https" : "http")
                                .arg(root.machineHost).arg(root.machinePort)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("TLS")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                        Text {
                            text: root.machineTls
                                  ? qsTr("Enabled (port 16995 for redirection)")
                                  : qsTr("Disabled (plaintext)")
                            color: root.machineTls ? Colors.on : Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("User")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                        Text {
                            text: root.machineUser.length > 0
                                  ? root.machineUser
                                  : qsTr("(not set)")
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("Trusted certs")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            visible: root.machineTls
                        }
                        Text {
                            text: root.machineTrustedFingerprints.length === 0
                                  ? qsTr("None pinned")
                                  : qsTr("%1 fingerprint(s) pinned")
                                      .arg(root.machineTrustedFingerprints.length)
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            visible: root.machineTls
                            Layout.fillWidth: true
                        }
                    }
                }

                Section {
                    title: qsTr("ACTIONS")
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 24

                    Flow {
                        spacing: 8
                        Layout.fillWidth: true

                        Button {
                            text: qsTr("Open SOL")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                            onClicked: solLoader.launch()
                        }
                        Button {
                            text: qsTr("Mount ISO")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                            onClicked: iderLoader.launch()
                        }
                        Button {
                            text: qsTr("Open KVM")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                            onClicked: kvmLoader.launch()
                        }
                        Button {
                            text: qsTr("Edit settings…")
                            flat: true
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            onClicked: editDialog.openFor(root.row)
                        }
                        Button {
                            text: qsTr("Delete machine")
                            flat: true
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            onClicked: confirmDelete.open()
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmDelete
        title: qsTr("Delete machine")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel | Dialog.Yes
        implicitWidth: 420
        contentItem: Text {
            text: qsTr("Remove %1 from the list? This does not change anything on the AMT device itself.")
                  .arg(root.machineName.length > 0 ? root.machineName : root.machineHost)
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
        }
        onAccepted: if (root.hasSelection) ComputerModel.removeAt(root.row)
    }

    ColumnLayout {
        visible: !root.hasSelection
        spacing: 6
        anchors.centerIn: parent

        AppMark {
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            Layout.alignment: Qt.AlignHCenter
            opacity: 0.5
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
        Button {
            text: qsTr("Add machine")
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 12
            onClicked: root.addRequested()
        }
    }

    Loader {
        id: solLoader
        active: false
        asynchronous: true
        property int targetRow: -1
        function launch() { targetRow = root.row; active = false; active = true; }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
            item.targetPort = root.machineTls ? 16995 : 16994;
            item.user = root.machineUser;
            item.password = root.machinePass;
            item.tls = root.machineTls;
            item.trustedFingerprints = root.machineTrustedFingerprints;
            item.label = root.machineName.length > 0 ? root.machineName : root.machineHost;
            item.visible = true;
            item.start();
        }
        onStatusChanged: if (status === Loader.Ready) openWindow()
        sourceComponent: SolWindow {
            onClosing: solLoader.active = false
            onTrustedFingerprintPersistRequested: function(fp) {
                if (solLoader.targetRow >= 0)
                    ComputerModel.addTrustedFingerprint(solLoader.targetRow, fp);
            }
        }
    }

    Loader {
        id: iderLoader
        active: false
        asynchronous: true
        property int targetRow: -1
        function launch() { targetRow = root.row; active = false; active = true; }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
            item.targetPort = root.machineTls ? 16995 : 16994;
            item.user = root.machineUser;
            item.password = root.machinePass;
            item.tls = root.machineTls;
            item.trustedFingerprints = root.machineTrustedFingerprints;
            item.label = root.machineName.length > 0 ? root.machineName : root.machineHost;
            item.visible = true;
        }
        onStatusChanged: if (status === Loader.Ready) openWindow()
        sourceComponent: IderWindow {
            onClosing: iderLoader.active = false
            onTrustedFingerprintPersistRequested: function(fp) {
                if (iderLoader.targetRow >= 0)
                    ComputerModel.addTrustedFingerprint(iderLoader.targetRow, fp);
            }
        }
    }

    Loader {
        id: kvmLoader
        active: false
        asynchronous: true
        property int targetRow: -1
        function launch() { targetRow = root.row; active = false; active = true; }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
            item.targetPort = root.machineTls ? 16995 : 16994;
            item.user = root.machineUser;
            item.password = root.machinePass;
            item.tls = root.machineTls;
            item.trustedFingerprints = root.machineTrustedFingerprints;
            item.label = root.machineName.length > 0 ? root.machineName : root.machineHost;
            item.visible = true;
            item.start();
        }
        onStatusChanged: if (status === Loader.Ready) openWindow()
        sourceComponent: KvmWindow {
            onClosing: kvmLoader.active = false
            onTrustedFingerprintPersistRequested: function(fp) {
                if (kvmLoader.targetRow >= 0)
                    ComputerModel.addTrustedFingerprint(kvmLoader.targetRow, fp);
            }
        }
    }
}
