// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Detached window hosting one SOL session. Created on demand by the
/// edit pane; owns its `SolController`. Closing the window closes the
/// underlying TCP session.
AppWindow {
    id: root

    property string targetHost
    property string user
    property string password
    property bool tls: false
    property var trustedFingerprints: []
    property string label: qsTr("Serial Console")

    signal trustedFingerprintPersistRequested(string fingerprint)

    function start() {
        controller.host = root.targetHost;
        controller.user = root.user;
        controller.password = root.password;
        controller.tls = root.tls;
        controller.trustedFingerprints = root.trustedFingerprints;
        controller.open();
    }

    width: 900
    height: 620
    minimumWidth: 560
    minimumHeight: 360
    title: qsTr("QuMesh — %1 — %2").arg(root.label).arg(root.targetHost)

    SolController {
        id: controller
        onTrustedFingerprintAdded: function(fp) {
            root.trustedFingerprintPersistRequested(fp);
        }
    }

    /// Per-window WSMAN controller, used only for the Power ▾ menu.
    /// Same credentials / pinned-cert list as the SOL controller; opens
    /// a separate TCP session against the WSMAN port (16992/16993).
    MachineDetailsController {
        id: powerController
        host: root.targetHost
        user: root.user
        password: root.password
        tls: root.tls
        trustedFingerprints: root.trustedFingerprints
        onTrustedFingerprintAdded: function(fp) {
            root.trustedFingerprintPersistRequested(fp);
        }
    }

    CertTrustDialog {
        controller: controller
    }
    CertTrustDialog {
        controller: powerController
    }

    onClosing: controller.close()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 10
            Layout.fillWidth: true

            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: controller.state === SolController.Connected ? Colors.on
                     : controller.state === SolController.Failed ? Colors.error
                     : controller.state === SolController.Disconnected ? Colors.off
                     : Colors.standby
            }

            Text {
                text: {
                    switch (controller.state) {
                    case SolController.Disconnected:   return qsTr("Disconnected");
                    case SolController.Connecting:     return qsTr("Connecting…");
                    case SolController.Authenticating: return qsTr("Authenticating…");
                    case SolController.Opening:        return qsTr("Opening session…");
                    case SolController.Connected:      return qsTr("Connected");
                    case SolController.Failed:         return qsTr("Failed: %1").arg(controller.lastError);
                    }
                    return "";
                }
                color: controller.state === SolController.Failed ? Colors.error : Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Power ▾")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: !powerController.busy
                onClicked: powerMenu.popup()

                Menu {
                    id: powerMenu
                    MenuItem { text: qsTr("Power on");           onTriggered: powerController.powerOn() }
                    MenuItem { text: qsTr("Reset");              onTriggered: powerController.powerReset() }
                    MenuItem { text: qsTr("Reset (graceful)");   onTriggered: powerController.powerResetGraceful() }
                    MenuItem { text: qsTr("Power off (soft)");   onTriggered: powerController.powerOffSoft() }
                    MenuItem { text: qsTr("Power off (hard)");   onTriggered: powerController.powerOffHard() }
                    MenuSeparator {}
                    MenuItem {
                        visible: powerController.amtVersionMajor >= 10
                        height: visible ? implicitHeight : 0
                        text: qsTr("Wake OS")
                        onTriggered: powerController.osWakeFromSleep()
                    }
                    MenuItem {
                        visible: powerController.amtVersionMajor >= 10
                        height: visible ? implicitHeight : 0
                        text: qsTr("Sleep OS")
                        onTriggered: powerController.osPutToSleep()
                    }
                    MenuSeparator { visible: powerController.amtVersionMajor >= 10; height: visible ? implicitHeight : 0 }
                    MenuItem { text: qsTr("Power on to BIOS Setup"); onTriggered: powerController.bootToBios(false) }
                    MenuItem { text: qsTr("Reset to BIOS Setup");    onTriggered: powerController.bootToBios(true) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Power on to PXE");        onTriggered: powerController.bootToPxe(false) }
                    MenuItem { text: qsTr("Reset to PXE");           onTriggered: powerController.bootToPxe(true) }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Power on to IDE-R CDROM"); onTriggered: powerController.bootToIderCdrom(false) }
                    MenuItem { text: qsTr("Reset to IDE-R CDROM");    onTriggered: powerController.bootToIderCdrom(true) }
                    MenuItem { text: qsTr("Power on to IDE-R Floppy"); onTriggered: powerController.bootToIderFloppy(false) }
                    MenuItem { text: qsTr("Reset to IDE-R Floppy");    onTriggered: powerController.bootToIderFloppy(true) }
                }
            }

            Button {
                text: controller.state === SolController.Connected
                      || controller.state === SolController.Connecting
                      || controller.state === SolController.Authenticating
                      || controller.state === SolController.Opening
                    ? qsTr("Disconnect")
                    : qsTr("Reconnect")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    if (controller.state === SolController.Disconnected
                        || controller.state === SolController.Failed) {
                        root.start();
                    } else {
                        controller.close();
                    }
                }
            }
        }

        Terminal {
            id: term
            screen: controller.screen
            Layout.fillWidth: true
            Layout.fillHeight: true
            onKeyInput: function(text) { controller.sendText(text); }
            onControlSequence: function(seq) { controller.sendText(seq); }
        }
    }

    Component.onCompleted: term.forceActiveFocus()
}
