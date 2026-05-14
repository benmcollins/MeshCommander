// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Detached window hosting one KVM session.
AppWindow {
    id: root

    property string targetHost
    property string user
    property string password
    property bool tls: false
    property var trustedFingerprints: []
    property string label: qsTr("Remote Desktop")

    signal trustedFingerprintPersistRequested(string fingerprint)

    function start() {
        controller.host = root.targetHost;
        controller.user = root.user;
        controller.password = root.password;
        controller.tls = root.tls;
        controller.trustedFingerprints = root.trustedFingerprints;
        controller.open();
    }

    // X11 keysyms used by the Send-keys menu. Mirror the constants in
    // kvmviewer.cpp; the menu reaches them via `root.kXk*`.
    readonly property int kXkLAlt: 0xFFE9
    readonly property int kXkLCtrl: 0xFFE3
    readonly property int kXkLMeta: 0xFFE7   // Windows / Super_L
    readonly property int kXkLShift: 0xFFE1
    readonly property int kXkEscape: 0xFF1B
    readonly property int kXkTab: 0xFF09
    readonly property int kXkF1Base: 0xFFBE

    function chord(keys) {
        // Press each key in order, release in reverse — modifiers wrap
        // the inner key the way AMT expects.
        for (let i = 0; i < keys.length; ++i)
            controller.sendKey(keys[i], true);
        for (let i = keys.length - 1; i >= 0; --i)
            controller.sendKey(keys[i], false);
    }

    width: 1024
    height: 720
    minimumWidth: 640
    minimumHeight: 400
    title: qsTr("QuMesh — %1 — %2").arg(root.label).arg(root.targetHost)

    KvmController {
        id: controller
        onTrustedFingerprintAdded: function(fp) {
            root.trustedFingerprintPersistRequested(fp);
        }
        onPeerCertVerifiedByPin: function(fp) { certPinFlash.flash(fp) }
        onStateChanged: {
            if (state === KvmController.Connected) ActivityHeartbeat.reportSuccess();
            else if (state === KvmController.Failed)
                ActivityHeartbeat.reportFailure(qsTr("KVM: %1").arg(lastError));
        }
    }

    /// Per-window WSMAN controller for the Power ▾ menu — same auth /
    /// pinned cert, separate TCP session against the WSMAN port.
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
        onPeerCertVerifiedByPin: function(fp) { certPinFlash.flash(fp) }
        onPowerChangeCompleted: function(state, ok, error) {
            if (ok) ActivityHeartbeat.reportSuccess();
            else    ActivityHeartbeat.reportFailure(error);
        }
        onLastErrorChanged: {
            if (lastError.length > 0)
                ActivityHeartbeat.reportFailure(lastError);
        }
    }

    CertPinFlash {
        id: certPinFlash
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.rightMargin: 12
        z: 1000
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
        anchors.margins: 8
        spacing: 8

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: controller.state === KvmController.Connected ? Colors.on
                     : controller.state === KvmController.Failed ? Colors.error
                     : controller.state === KvmController.Disconnected ? Colors.off
                     : Colors.standby
            }

            Text {
                text: {
                    switch (controller.state) {
                    case KvmController.Disconnected:   return qsTr("Disconnected");
                    case KvmController.Connecting:     return qsTr("Connecting…");
                    case KvmController.Authenticating: return qsTr("Authenticating…");
                    case KvmController.Negotiating:    return qsTr("Negotiating display…");
                    case KvmController.Connected:      return qsTr("%1×%2")
                                                                .arg(controller.desktopWidth)
                                                                .arg(controller.desktopHeight);
                    case KvmController.Failed:         return qsTr("Failed: %1").arg(controller.lastError);
                    }
                    return "";
                }
                color: controller.state === KvmController.Failed ? Colors.error : Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                font.features: ({ "tnum": 1 })
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Power ▾")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
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
                text: qsTr("Send keys ▾")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                enabled: controller.state === KvmController.Connected
                onClicked: sendMenu.popup()

                Menu {
                    id: sendMenu
                    MenuItem {
                        text: qsTr("Ctrl+Alt+Del")
                        onTriggered: controller.sendCtrlAltDel()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Alt+Tab")
                        onTriggered: root.chord([root.kXkLAlt, root.kXkTab])
                    }
                    MenuItem {
                        text: qsTr("Alt+F4")
                        onTriggered: root.chord([root.kXkLAlt, root.kXkF1Base + 3])
                    }
                    MenuItem {
                        text: qsTr("Esc")
                        onTriggered: controller.sendKeyTap(root.kXkEscape)
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Win key")
                        onTriggered: controller.sendKeyTap(root.kXkLMeta)
                    }
                    MenuItem {
                        text: qsTr("Win+L (lock)")
                        onTriggered: root.chord([root.kXkLMeta, 0x6C])  // 'l'
                    }
                    MenuItem {
                        text: qsTr("Win+R (run)")
                        onTriggered: root.chord([root.kXkLMeta, 0x72])  // 'r'
                    }
                    MenuItem {
                        text: qsTr("Win+E (explorer)")
                        onTriggered: root.chord([root.kXkLMeta, 0x65])  // 'e'
                    }
                    MenuItem {
                        text: qsTr("Win+D (desktop)")
                        onTriggered: root.chord([root.kXkLMeta, 0x64])  // 'd'
                    }
                    MenuSeparator {}
                    Menu {
                        title: qsTr("Function keys")
                        // F1..F12 — useful when macOS intercepts the
                        // physical F-keys for brightness / volume.
                        Repeater {
                            model: 12
                            delegate: MenuItem {
                                required property int index
                                text: qsTr("F%1").arg(index + 1)
                                onTriggered: controller.sendKeyTap(root.kXkF1Base + index)
                            }
                        }
                    }
                }
            }

            Button {
                text: controller.state === KvmController.Disconnected
                      || controller.state === KvmController.Failed
                    ? qsTr("Reconnect") : qsTr("Disconnect")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    if (controller.state === KvmController.Disconnected
                        || controller.state === KvmController.Failed) {
                        root.start();
                    } else {
                        controller.close();
                    }
                }
            }
        }

        KvmViewer {
            id: viewer
            controller: controller
            focus: true
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Component.onCompleted: viewer.forceActiveFocus()
}
