// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Single per-machine session window. Hosts SOL / KVM / IDE-R as tabs
/// instead of three independent windows so the operator only juggles
/// one OS window per managed machine. The shared `MachineDetailsController`
/// in the title bar drives the Power ▾ menu via one WSMAN connection
/// instead of one-per-panel.
AppWindow {
    id: root

    property string targetHost
    property string user
    property string password
    property bool tls: false
    property list<string> trustedFingerprints: []
    // `machineSshConfig` is a QVariantMap from `ComputerModel.sshConfigFor()`;
    // QML's only typed declaration for that shape is still `var`. A `Q_GADGET`
    // would help qmlsc but is out of scope for #387.
    property var machineSshConfig: ({})
    property string label: qsTr("Session")

    /// Initial tab to show: 0 = SOL, 1 = KVM, 2 = IDE-R. Panels do
    /// not auto-connect — the operator clicks Connect on the panel.
    property int initialTab: 0

    signal trustedFingerprintPersistRequested(string fingerprint)
    signal trustedSshHostKeyPersistRequested(string fingerprint)

    function openTab(idx) {
        bar.currentIndex = idx;
        if (root.visible) root.raise();
    }

    width: 1024
    height: 720
    minimumWidth: 640
    minimumHeight: 420
    title: qsTr("QuMesh — %1 — %2 — %3")
        .arg(root.label)
        .arg(root.targetHost)
        .arg(bar.currentIndex === 0 ? qsTr("Serial Console")
            : bar.currentIndex === 1 ? qsTr("Remote Desktop")
            : qsTr("IDE Redirection"))

    MachineDetailsController {
        id: powerController
        host: root.targetHost
        user: root.user
        password: root.password
        tls: root.tls
        trustedFingerprints: root.trustedFingerprints
        Component.onCompleted: powerController.setSshConfig(root.machineSshConfig || ({}))
        onTrustedFingerprintAdded: function(fp) {
            root.trustedFingerprintPersistRequested(fp);
        }
        onTrustedSshHostKeyAdded: function(fp) {
            root.trustedSshHostKeyPersistRequested(fp);
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
        // Detect that KVM/SOL/IDE-R will be gated by consent and walk
        // the operator through the StartOptIn / SendOptInCode dance.
        onOptInStatusChanged: {
            if (powerController.optInRequired
                && powerController.optInState !== 4 /* InSession */
                && !optInPrompt.opened) {
                powerController.startOptIn();
            }
        }
        onOptInStarted: function(ok, error) {
            if (ok) optInPrompt.openFor(powerController.optInPolicyTimeoutSec);
            else ActivityHeartbeat.reportFailure(qsTr("User consent: %1").arg(error));
        }
        onOptInCodeResult: function(ok, error) {
            if (ok) {
                optInPrompt.close();
                ActivityHeartbeat.reportSuccess();
            } else {
                optInPrompt.errorText = qsTr("Code rejected: %1").arg(error);
                optInPrompt.openFor(powerController.optInPolicyTimeoutSec);
            }
        }
        // Polling signals from #171.
        onOptInGranted: {
            optInPrompt.close();
            ActivityHeartbeat.reportSuccess();
        }
        onOptInExpiredOrDenied: {
            optInPrompt.close();
            ActivityHeartbeat.reportFailure(
                qsTr("User consent expired or denied at the target."));
        }
    }

    CertTrustDialog {
        controller: powerController
    }

    /// SSH host-key trust for the WSMAN-side tunnel managed by the
    /// SessionWindow's MachineDetailsController. Sol/Kvm/Ider tabs
    /// each have their own SshTunnelHost and surface their own
    /// prompts via the per-panel SshHostKeyTrustDialog instances.
    SshHostKeyTrustDialog {
        controller: powerController
        onCancelled: powerController.setSshConfig({})
    }

    /// Shared confirmation prompt for the disruptive Power menu items
    /// (Reset / Power off / Reset-to-PXE / ...). The pending action is
    /// captured as a JS function and invoked on accept (#278).
    ConfirmDialog {
        id: confirmPower
        property var pendingAction: null
        function askFor(t, b, verb, fn) {
            pendingAction = fn;
            ask(t, b, verb, true);
        }
        onProceed: {
            if (pendingAction) pendingAction();
            pendingAction = null;
        }
        onRejected: pendingAction = null
    }

    OptInPrompt {
        id: optInPrompt
        onSubmitted: function(code) { powerController.sendOptInCode(code); }
        onCancelled: powerController.cancelOptIn()
    }

    CertPinFlash {
        id: certPinFlash
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.rightMargin: 12
        z: 1000
    }

    onClosing: {
        solPanel.stop();
        kvmPanel.stop();
        iderPanel.stop();
    }

    Component.onCompleted: {
        bar.currentIndex = root.initialTab;
        // Once the controller has the SSH config + host, ask AMT whether
        // consent is required for redirection. The result lands in
        // `onOptInStatusChanged` which kicks off `startOptIn` if needed.
        powerController.refreshOptInStatus();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // -- Title bar with Power ▾ ------------------------------------
        Rectangle {
            color: Colors.surface
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: root.label
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Power ▾")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: !powerController.busy
                    onClicked: powerMenu.popup()

                    Menu {
                        id: powerMenu
                        // Reset/Off are disruptive — interrupt the OS,
                        // can lose unsaved work. Gate with ConfirmDialog
                        // (#278). "Power on …" variants only apply when
                        // the machine is already off, so they're benign.
                        MenuItem { text: qsTr("Power on");           onTriggered: powerController.powerOn() }
                        MenuItem {
                            text: qsTr("Reset")
                            onTriggered: confirmPower.askFor(
                                qsTr("Reset \"%1\"?").arg(root.targetHost),
                                qsTr("This hard-resets the target — unsaved "
                                     + "work on the OS will be lost."),
                                qsTr("Reset"),
                                () => powerController.powerReset())
                        }
                        MenuItem {
                            text: qsTr("Reset (graceful)")
                            onTriggered: confirmPower.askFor(
                                qsTr("Reset \"%1\" gracefully?").arg(root.targetHost),
                                qsTr("Asks the OS to reboot. The OS may still "
                                     + "decline (e.g. if a save prompt is open)."),
                                qsTr("Reset"),
                                () => powerController.powerResetGraceful())
                        }
                        MenuItem {
                            text: qsTr("Power off (soft)")
                            onTriggered: confirmPower.askFor(
                                qsTr("Power off \"%1\"?").arg(root.targetHost),
                                qsTr("Asks the OS to shut down."),
                                qsTr("Power off"),
                                () => powerController.powerOffSoft())
                        }
                        MenuItem {
                            text: qsTr("Power off (hard)")
                            onTriggered: confirmPower.askFor(
                                qsTr("Hard power off \"%1\"?").arg(root.targetHost),
                                qsTr("Cuts power without asking the OS — "
                                     + "unsaved work will be lost."),
                                qsTr("Power off"),
                                () => powerController.powerOffHard())
                        }
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
                        // Boot variants live in BootMenu.qml so this
                        // window and MachineDetailsWindow can't drift
                        // (see #299). As a submenu it also de-clutters
                        // the top-level Power ▾ menu. The four cap-
                        // gated items (Secure Erase / Platform Erase /
                        // HTTPS / OCR) hide here because we don't pass
                        // the matching prompt instances — those live
                        // in MachineDetailsWindow and aren't carried
                        // by SessionWindow.
                        BootMenu {
                            controller: powerController
                            targetHost: root.targetHost
                            confirmDialog: confirmPower
                        }
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                implicitHeight: 1
                color: Colors.border
            }
        }

        TabBar {
            id: bar
            Layout.fillWidth: true
            TabButton {
                text: qsTr("Serial Console")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }
            TabButton {
                text: qsTr("Remote Desktop")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }
            TabButton {
                text: qsTr("IDE Redirection")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }
        }

        StackLayout {
            id: stack
            currentIndex: bar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            SolPanel {
                id: solPanel
                targetHost: root.targetHost
                machineName: root.label
                user: root.user
                password: root.password
                tls: root.tls
                trustedFingerprints: root.trustedFingerprints
                sshConfig: root.machineSshConfig
                onTrustedFingerprintPersistRequested: function(fp) {
                    root.trustedFingerprintPersistRequested(fp);
                }
                onTrustedSshHostKeyPersistRequested: function(fp) {
                    root.trustedSshHostKeyPersistRequested(fp);
                }
                onPeerCertVerifiedByPin: function(fp) { certPinFlash.flash(fp) }
            }
            KvmPanel {
                id: kvmPanel
                targetHost: root.targetHost
                machineName: root.label
                user: root.user
                password: root.password
                tls: root.tls
                trustedFingerprints: root.trustedFingerprints
                sshConfig: root.machineSshConfig
                onTrustedFingerprintPersistRequested: function(fp) {
                    root.trustedFingerprintPersistRequested(fp);
                }
                onTrustedSshHostKeyPersistRequested: function(fp) {
                    root.trustedSshHostKeyPersistRequested(fp);
                }
                onPeerCertVerifiedByPin: function(fp) { certPinFlash.flash(fp) }
            }
            IderPanel {
                id: iderPanel
                targetHost: root.targetHost
                user: root.user
                password: root.password
                tls: root.tls
                trustedFingerprints: root.trustedFingerprints
                sshConfig: root.machineSshConfig
                onTrustedFingerprintPersistRequested: function(fp) {
                    root.trustedFingerprintPersistRequested(fp);
                }
                onTrustedSshHostKeyPersistRequested: function(fp) {
                    root.trustedSshHostKeyPersistRequested(fp);
                }
                onPeerCertVerifiedByPin: function(fp) { certPinFlash.flash(fp) }
            }
        }
    }
}
