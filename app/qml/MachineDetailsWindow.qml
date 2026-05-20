// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Per-machine "open" window. The main app never connects on its own;
/// double-clicking a machine here is the explicit connect. The sidebar
/// lists components (Overview, Network, ...) and each pane fetches its
/// data on-demand via the MachineDetailsController.
AppWindow {
    id: root

    // Inputs from the main-window Loader.
    property int targetRow: -1
    property string machineName
    property string machineHost
    property string machineUser
    property string machinePass
    property bool   machineTls: false
    property var    machineTrustedFingerprints: []
    property var    machineSshConfig: ({})

    signal trustedFingerprintPersistRequested(string fingerprint)
    signal trustedSshHostKeyPersistRequested(string fingerprint)

    width: 1100
    height: 760
    minimumWidth: 880
    minimumHeight: 540
    title: machineName.length > 0
        ? qsTr("QuMesh — %1").arg(machineName)
        : qsTr("QuMesh — %1").arg(machineHost)

    // Apply the saved SSH tunnel config whenever Main.qml updates
    // `machineSshConfig`. `Component.onCompleted` is too early — the
    // Loader populates `machineSshConfig` only after `Loader.Ready`,
    // and at construction time it's still the default empty object,
    // so setting up the controller's SSH state from inside onCompleted
    // installs an empty (disabled) config and the WSMAN requests then
    // bypass the tunnel entirely.
    onMachineSshConfigChanged: controller.setSshConfig(root.machineSshConfig || ({}))

    MachineDetailsController {
        id: controller
        host: root.machineHost
        user: root.machineUser
        password: root.machinePass
        tls: root.machineTls
        trustedFingerprints: root.machineTrustedFingerprints
        onTrustedFingerprintAdded: function(fp) {
            // Pass up to Main.qml so it lands in ComputerModel — SOL /
            // KVM / IDE-R will then inherit the same pinned trust.
            root.trustedFingerprintPersistRequested(fp);
        }
        onTrustedSshHostKeyAdded: function(fp) {
            root.trustedSshHostKeyPersistRequested(fp);
        }
        onCloseRequested: root.close()
        onPeerCertVerifiedByPin: function(fp) { certPinFlash.flash(fp) }

        // Each refresh / fetch that arrives without `lastError` set is
        // a success pulse; failures (any non-empty `lastError`) flash
        // the bar red and surface the message in the tooltip.
        onPowerChangeCompleted: function(state, ok, error) {
            if (ok) ActivityHeartbeat.reportSuccess();
            else    ActivityHeartbeat.reportFailure(error);
        }
        onIdentifyChanged:        ActivityHeartbeat.reportSuccess()
        onPowerStateChanged:      ActivityHeartbeat.reportSuccess()
        onGeneralSettingsChanged: ActivityHeartbeat.reportSuccess()
        onComputerSystemChanged:  ActivityHeartbeat.reportSuccess()
        onEthernetChanged:        ActivityHeartbeat.reportSuccess()
        onTimeChanged:            ActivityHeartbeat.reportSuccess()
        onEventLogChanged:        ActivityHeartbeat.reportSuccess()
        onUserAccountsChanged:    ActivityHeartbeat.reportSuccess()
        onLastErrorChanged: {
            if (lastError.length > 0)
                ActivityHeartbeat.reportFailure(lastError);
        }
        onSshTunnelStateChanged: {
            if (sshTunnelActive) ActivityHeartbeat.reportSuccess();
            else if (sshTunnelStatus.indexOf("failed") >= 0)
                ActivityHeartbeat.reportFailure(sshTunnelStatus);
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

    /// Trust-on-first-use prompt. The dialog is wired generically — it
    /// reads `controller.awaitingTrust` and the `pendingCert*` props,
    /// and calls `controller.trustPendingCert(persist)` / `close()`.
    CertTrustDialog {
        controller: controller
    }

    /// SSH host-key trust prompt (#270). The dialog binds to the
    /// controller's `awaitingSshHostKeyTrust` and the
    /// `pendingSshHostKey*` properties; Accept calls
    /// `trustPendingSshHostKey(persist)`, Cancel tears the tunnel
    /// down via `setSshConfig({})`.
    SshHostKeyTrustDialog {
        controller: controller
        onCancelled: controller.setSshConfig({})
    }

    /// Shared confirmation prompt for the disruptive Power section
    /// buttons (Reset / Power off / Reset-to-PXE / ...). The pending
    /// action is captured as a JS function and invoked on accept. See
    /// #278.
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

    SecureErasePrompt {
        id: secureErasePrompt
        tlsActive: root.machineTls
        onConfirmed: function(password, reset) {
            controller.bootToSecureErase(reset, password);
        }
    }

    PlatformErasePrompt {
        id: platformErasePrompt
        tlsActive: root.machineTls
        capabilitiesMask: controller.capPlatformEraseMask
        onConfirmed: function(flags, psid, ssdPassword, reset) {
            controller.bootToPlatformErase(reset, flags, psid, ssdPassword);
        }
    }

    HttpsBootPrompt {
        id: httpsBootPrompt
        tlsActive: root.machineTls
        onConfirmed: function(url, reset) {
            controller.bootToHttpsBootUrl(reset, url);
        }
    }

    OcrPrompt {
        id: ocrPrompt
        canWinRE: controller.capForceWinReBoot
        canLocalPBA: controller.capForceUefiLocalPbaBoot
        canHttps: controller.capForceUefiHttpsBoot
        onConfirmWinRE: function(reset) {
            controller.bootToWinRE(reset);
        }
        onConfirmLocalPBA: function(reset, idx) {
            controller.bootToLocalPBA(reset, idx);
        }
        onConfirmHttps: function(reset, url, hashAlg, hashHex,
                                  pinAlg, pinHex, user, pass) {
            controller.bootToOcrHttpsUrl(reset, url, hashAlg, hashHex,
                                          pinAlg, pinHex, user, pass);
        }
    }

    PowerPolicyDialog {
        id: powerPolicyDialog
        controller: controller
        onConfirmed: function(instanceId) {
            controller.setPowerScheme(instanceId);
        }
    }

    KvmSettingsDialog {
        id: kvmSettingsDialog
        controller: controller
    }

    HttpProxyDialog {
        id: httpProxyDialog
        onConfirmed: function(accessInfo, port, networkDnsSuffix) {
            controller.addHttpProxy(accessInfo, port, networkDnsSuffix);
        }
    }

    EnvDetectionDialog {
        id: envDetectionDialog
        controller: controller
    }

    UserInitiatedDialog {
        id: userInitiatedDialog
        controller: controller
    }

    MpsServerDialog {
        id: mpsServerDialog
        controller: controller
    }

    ConfirmDialog {
        id: mpsConfirmDialog
        property string pendingName: ""
        onProceed: {
            if (pendingName.length > 0)
                controller.removeMpServer(pendingName);
            pendingName = "";
        }
    }

    CiraPolicyDialog {
        id: ciraPolicyDialog
        controller: controller
    }

    ConfirmDialog {
        id: ciraPolicyConfirmDialog
        property string pendingName: ""
        onProceed: {
            if (pendingName.length > 0)
                controller.removeCiraPolicyRule(pendingName);
            pendingName = "";
        }
    }

    UserAccountDialog {
        id: userAccountDialog
        controller: controller
    }

    AdminPasswordDialog {
        id: adminPasswordDialog
        controller: controller
    }

    AddCertificateDialog {
        id: addCertificateDialog
        controller: controller
    }

    IssueCertificateDialog {
        id: issueCertificateDialog
        controller: controller
    }

    TlsModeDialog {
        id: tlsModeDialog
        controller: controller
    }

    CertDetailsDialog {
        id: certDetailsDialog
    }

    WiFiProfileDialog {
        id: wifiProfileDialog
        controller: controller
    }

    Wired8021xDialog {
        id: wired8021xDialog
        controller: controller
    }

    // Lightweight inline editor for the two AMT_WiFiPortConfigurationService
    // sync toggles. Built here rather than in a separate file because it's
    // two CheckBoxes + Apply — a full Dialog component would be overkill.
    Dialog {
        id: wifiSyncDialog
        title: qsTr("WiFi sync settings")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        implicitWidth: 460

        property int initialLocal: 0
        property int initialUefi: 0
        property bool draftLocal: false
        property bool draftUefi: false

        function openForPort(port) {
            initialLocal = port.localProfileSyncEnabled || 0;
            initialUefi  = port.uefiProfileShareEnabled || 0;
            draftLocal = initialLocal === 1;
            draftUefi  = initialUefi === 1;
            open();
        }

        contentItem: ColumnLayout {
            spacing: 14
            CheckBox {
                text: qsTr("Local profile synchronization — push OS-side WiFi profiles to AMT")
                checked: wifiSyncDialog.draftLocal
                onToggled: wifiSyncDialog.draftLocal = checked
            }
            CheckBox {
                text: qsTr("UEFI WiFi profile sharing — let UEFI / pre-boot use these profiles")
                checked: wifiSyncDialog.draftUefi
                onToggled: wifiSyncDialog.draftUefi = checked
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Cancel")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: wifiSyncDialog.reject()
                }
                AccentButton {
                    text: qsTr("Apply")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: {
                        controller.setWifiSyncSettings(
                            wifiSyncDialog.draftLocal ? 1 : 0,
                            wifiSyncDialog.draftUefi  ? 1 : 0);
                        wifiSyncDialog.accept();
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: wifiProfileConfirmDialog
        property string pendingName: ""
        property string pendingMode: "" // single | bulkIT | bulkUser
        onProceed: {
            if (pendingMode === "single")
                controller.deleteWiFiProfile(pendingName);
            else if (pendingMode === "bulkIT")
                controller.deleteAllITWiFiProfiles();
            else if (pendingMode === "bulkUser")
                controller.deleteAllUserWiFiProfiles();
            pendingName = "";
            pendingMode = "";
        }
    }

    ConfirmDialog {
        id: certConfirmDialog
        property string pendingInstance: ""
        property string pendingKind: "" // "cert" or "key"
        onProceed: {
            if (pendingKind === "cert")
                controller.deleteDeviceCertificate(pendingInstance);
            else if (pendingKind === "key")
                controller.deleteDeviceKeyPair(pendingInstance);
            pendingInstance = "";
            pendingKind = "";
        }
    }

    ConfirmDialog {
        id: confirmDialog
        // The user-accounts pane stashes the row + action here before
        // opening; the proceed handler reads them back. Two-property
        // pattern lets one ConfirmDialog instance serve every delete
        // / disable callsite without per-action component instances.
        property int pendingHandle: -1
        property string pendingAction: ""
        onProceed: {
            if (pendingAction === "delete")
                controller.removeUserAccount(pendingHandle);
            else if (pendingAction === "disable")
                controller.setAccountEnabled(pendingHandle, false);
            pendingHandle = -1;
            pendingAction = "";
        }
    }

    // Surfaces a Put-rejected message when the AMT login can't modify
    // the consent policy. Cleared on the next successful refresh.
    Connections {
        target: controller
        function onOptInPolicyChangeFailed(error) {
            ActivityHeartbeat.reportFailure(qsTr("User consent: %1").arg(error));
        }
    }

    // Sidebar items. The `id` keys must match the value used by each
    // Loader/StackLayout currentIndex below.
    readonly property var sections: [
        { key: "overview",  label: qsTr("Overview"),       icon: "layout-dashboard" },
        { key: "hardware",  label: qsTr("Hardware"),       icon: "cpu" },
        { key: "power",     label: qsTr("Power"),          icon: "power" },
        { key: "network",   label: qsTr("Network"),        icon: "network" },
        { key: "wireless",  label: qsTr("Wireless"),       icon: "wifi" },
        { key: "time",      label: qsTr("Time"),           icon: "clock" },
        { key: "remote",    label: qsTr("Remote access"),  icon: "monitor" },
        { key: "cira",      label: qsTr("CIRA"),           icon: "cloud-cog" },
        { key: "certs",     label: qsTr("Pinned trust"),   icon: "lock" },
        { key: "devcerts",  label: qsTr("Device certs"),   icon: "badge-check" },
        { key: "events",    label: qsTr("Event log"),      icon: "scroll-text" },
        { key: "audit",     label: qsTr("Audit log"),      icon: "clipboard-list" },
        { key: "users",     label: qsTr("User accounts"),  icon: "users" },
        { key: "watchdogs", label: qsTr("Watchdogs"),      icon: "dog" },
        { key: "subs",      label: qsTr("Subscriptions"),  icon: "bell" },
        { key: "alarms",    label: qsTr("Wake alarms"),    icon: "alarm-clock" },
        { key: "wsman",     label: qsTr("WSMAN"),          icon: "terminal" },
        { key: "sysdef",    label: qsTr("System Defense"), icon: "shield" },
        { key: "sessions",  label: qsTr("Sessions"),       icon: "arrow-left-right" },
    ]
    property int currentSection: 0

    /// Pull whatever data the active section needs. Skipped silently
    /// while `machineHost` is still empty (the Loader populates the
    /// props *after* the window is constructed, so initial bindings
    /// fire with an empty host before `applyMachine()` runs).
    function refreshCurrent() {
        if (machineHost.length === 0) return;
        switch (currentSection) {
        case 0:  controller.refreshOverview();     break;
        case 1:  controller.refreshHardware();     break;
        case 2:  controller.refreshPower();        break;
        case 3:  controller.refreshNetwork();      break;
        case 4:  controller.refreshWireless();     break;
        case 5:  controller.refreshTime();         break;
        // 6 = Remote access — no fetch needed.
        case 7:  controller.refreshRemoteAccess(); break;
        // 8 = Pinned trust (local pins) — comes from the saved machine.
        case 9:  controller.refreshDeviceCerts();  break;
        case 10: controller.refreshEventLog();     break;
        case 11: controller.refreshAuditLog();     break;
        case 12: controller.refreshUserAccounts(); break;
        case 13: controller.refreshAgentPresence(); break;
        case 14: controller.refreshEventSubscriptions(); break;
        case 15: controller.refreshWakeAlarms(); break;
        // 16 = WSMAN browser — operator-triggered, no auto-refresh.
        case 17: controller.refreshSystemDefense(); break;
        case 18: controller.refreshActiveSessions(); break;
        }
    }

    // Auto-refresh whenever the user moves between sections, or the
    // host becomes non-empty for the first time (i.e. once the parent
    // Loader has finished populating us). On the very first host
    // assignment the `host: root.machineHost` binding on the
    // MachineDetailsController has not necessarily propagated into
    // C++ yet — fire `refreshCurrent` via `Qt.callLater` so we run
    // after the binding engine catches up, otherwise the controller
    // reads `m_host == ""` and surfaces "Host is empty — cannot
    // refresh" until the user clicks a sidebar entry.
    onCurrentSectionChanged: refreshCurrent()
    onMachineHostChanged: if (machineHost.length > 0) Qt.callLater(refreshCurrent)

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // -- Sidebar ---------------------------------------------------
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Colors.surface
            border.width: 0

            Rectangle {
                color: Colors.border
                implicitWidth: 1
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: 18
                    spacing: 4

                    Text {
                        text: root.machineName.length > 0 ? root.machineName : qsTr("Unnamed")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeL
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.machineHost
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    // SSH tunnel info, directly under the IP so the link
                    // between "this connection goes through X" is obvious
                    // at a glance. Issue #245. Coloured by state: accent
                    // when the tunnel is live, muted when configured but
                    // not connected (e.g. authenticating, awaiting trust).
                    Text {
                        visible: controller.sshTunnelStatus.length > 0
                        text: controller.sshTunnelStatus
                        color: controller.sshTunnelActive
                            ? Colors.accent
                            : Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Row {
                        spacing: 6
                        Layout.topMargin: 6
                        StatusLed {
                            ledState: controller.powerState === 2 ? "on"
                                    : controller.powerState === 6 ? "off"
                                    : controller.powerState === 8 ? "off"
                                    : controller.powerState === 0 ? "unknown"
                                    : "standby"
                            implicitWidth: 8
                            implicitHeight: 8
                            pulse: !controller.busy
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: controller.busy
                                  ? qsTr("Working…")
                                  : controller.powerStateLabel
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Rectangle {
                    color: Colors.borderMuted
                    implicitHeight: 1
                    Layout.fillWidth: true
                }

                ListView {
                    id: nav
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: root.sections
                    clip: true
                    currentIndex: root.currentSection
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    // Keep the active row in view when the section
                    // changes programmatically (e.g. host reload sets
                    // currentSection back to 0 while the user has
                    // scrolled further down).
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        width: nav.width
                        height: 36
                        color: nav.currentIndex === index
                            ? Colors.accentSoft
                            : (hover.hovered ? Colors.elevated : "transparent")
                        Behavior on color { ColorAnimation { duration: Motion.fast } }

                        HoverHandler { id: hover }
                        TapHandler {
                            // currentIndex is bound to root.currentSection
                            // above, so setting that alone keeps the binding
                            // alive and lets programmatic section changes
                            // (e.g. on host reload) keep moving the highlight.
                            onTapped: root.currentSection = index
                        }

                        Rectangle {
                            color: Colors.accent
                            implicitWidth: 2
                            visible: nav.currentIndex === parent.index
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 12

                            Icon {
                                name: parent.parent.modelData.icon
                                size: 16
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }
                            Text {
                                text: parent.parent.modelData.label
                                color: nav.currentIndex === parent.parent.index
                                    ? Colors.text : Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                Rectangle {
                    color: Colors.borderMuted
                    implicitHeight: 1
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 10
                    spacing: 6

                    FlatButton {
                        text: qsTr("Refresh")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        enabled: !controller.busy
                        onClicked: root.refreshCurrent()
                    }
                    Item { Layout.fillWidth: true }
                    FlatButton {
                        text: qsTr("Close")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.close()
                    }
                }
            }
        }

        // -- Detail pane ----------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            /// Section-level loading indicator (#282). A 2 px
            /// indeterminate progress bar pinned to the pane's top
            /// while any WSMAN fetch is in flight. `controller.busy`
            /// was already used to disable buttons; this surfaces
            /// the same state visually so the user knows something's
            /// happening on a first-open fetch (where the section
            /// body has no rows yet to flash "loading" against).
            ProgressBar {
                id: sectionBusyBar
                visible: controller.busy
                indeterminate: true
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                implicitHeight: 2
                z: 11
                Accessible.role: Accessible.ProgressBar
                Accessible.name: qsTr("Fetching")
            }

            // Error banner across all sections (#283). Replaces the
            // hand-rolled red Rectangle that pinned its own height to
            // the inner Text. Adds a Dismiss button so the operator
            // can clear stale errors without switching sections.
            // Sits below the busy bar.
            ResultBanner {
                id: errBanner
                kind: "error"
                text: controller.lastError
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: sectionBusyBar.visible ? sectionBusyBar.height : 0
                z: 10
                onDismissed: controller.clearLastError()
            }

            StackLayout {
                anchors.fill: parent
                anchors.topMargin: (sectionBusyBar.visible ? sectionBusyBar.height : 0)
                                 + (errBanner.visible ? errBanner.implicitHeight : 0)
                currentIndex: root.currentSection

                // 0 — Overview (body in MachineDetails/OverviewSection.qml).
                Loader {
                    active: root.currentSection === 0
                    sourceComponent: OverviewSection {
                        anchors.fill: parent
                        controller: controller
                        machineName: root.machineName
                        machineHost: root.machineHost
                    }
                }

                // 1 — Hardware (body in MachineDetails/HardwareSection.qml).
                Loader {
                    active: root.currentSection === 1
                    sourceComponent: HardwareSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 2 — Power
                Loader {
                    active: root.currentSection === 2
                    sourceComponent: ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("POWER")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: controller.powerStateLabel
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: 24
                                font.weight: Font.Medium
                            }
                        }

                        Section {
                            title: qsTr("ACTIONS")
                            accent: Colors.accent
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Flow {
                                spacing: 8
                                Layout.fillWidth: true

                                Button {
                                    text: qsTr("Power on")
                                    enabled: !controller.busy
                                    onClicked: controller.powerOn()
                                }
                                Button {
                                    text: qsTr("Reset")
                                    enabled: !controller.busy
                                    onClicked: confirmPower.askFor(
                                        qsTr("Reset \"%1\"?").arg(root.machineHost),
                                        qsTr("This hard-resets the target — "
                                             + "unsaved work on the OS will be lost."),
                                        qsTr("Reset"),
                                        () => controller.powerReset())
                                }
                                Button {
                                    text: qsTr("Reset (graceful)")
                                    enabled: !controller.busy
                                    onClicked: confirmPower.askFor(
                                        qsTr("Reset \"%1\" gracefully?").arg(root.machineHost),
                                        qsTr("Asks the OS to reboot. The OS may "
                                             + "still decline."),
                                        qsTr("Reset"),
                                        () => controller.powerResetGraceful())
                                }
                                Button {
                                    text: qsTr("Power off (soft)")
                                    enabled: !controller.busy
                                    onClicked: confirmPower.askFor(
                                        qsTr("Power off \"%1\"?").arg(root.machineHost),
                                        qsTr("Asks the OS to shut down."),
                                        qsTr("Power off"),
                                        () => controller.powerOffSoft())
                                }
                                Button {
                                    text: qsTr("Power off (hard)")
                                    enabled: !controller.busy
                                    onClicked: confirmPower.askFor(
                                        qsTr("Hard power off \"%1\"?").arg(root.machineHost),
                                        qsTr("Cuts power without asking the OS — "
                                             + "unsaved work will be lost."),
                                        qsTr("Power off"),
                                        () => controller.powerOffHard())
                                }

                                Button {
                                    // OS-level wake/sleep is AMT 10+ only.
                                    visible: controller.amtVersionMajor >= 10
                                    text: qsTr("Wake OS")
                                    enabled: !controller.busy
                                    onClicked: controller.osWakeFromSleep()
                                }
                                Button {
                                    visible: controller.amtVersionMajor >= 10
                                    text: qsTr("Sleep OS")
                                    enabled: !controller.busy
                                    onClicked: controller.osPutToSleep()
                                }

                                Button {
                                    text: qsTr("Boot to… ▾")
                                    enabled: !controller.busy
                                    onClicked: bootMenu.popup()

                                    // BootMenu.qml owns the full item list
                                    // so this window and SessionWindow can't
                                    // drift (#299). Both pass `controller` +
                                    // `confirmDialog`; we additionally hand
                                    // over the four cap-gated prompts that
                                    // only exist in this window.
                                    BootMenu {
                                        id: bootMenu
                                        controller: controller
                                        targetHost: root.machineHost
                                        confirmDialog: confirmPower
                                        secureErasePrompt: secureErasePrompt
                                        platformErasePrompt: platformErasePrompt
                                        httpsBootPrompt: httpsBootPrompt
                                        ocrPrompt: ocrPrompt
                                    }
                                }

                                FlatButton {
                                    text: qsTr("Power Policy…")
                                    enabled: !controller.busy
                                        && controller.powerSchemes.length > 0
                                    onClicked: powerPolicyDialog.open()
                                }

                                FlatButton {
                                    text: qsTr("Refresh")
                                    enabled: !controller.busy
                                    onClicked: controller.refreshPower()
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // 3 — Network
                Loader {
                    active: root.currentSection === 3
                    sourceComponent: Flickable {
                        anchors.fill: parent
                        contentWidth: width
                        contentHeight: networkCol.implicitHeight + 48
                        clip: true

                        ColumnLayout {
                            id: networkCol
                            spacing: 18
                            width: parent.width

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                Layout.topMargin: 24
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Text {
                                    text: qsTr("NETWORK")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.letterSpacing: 2
                                    font.weight: Font.Medium
                                }
                                Text {
                                    text: controller.networkInterfaces.length === 0
                                        ? qsTr("Not yet fetched")
                                        : qsTr("%1 interface(s)")
                                            .arg(controller.networkInterfaces.length)
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: 20
                                }
                            }

                            Repeater {
                                model: controller.networkInterfaces
                                delegate: ColumnLayout {
                                    id: nicDelegate
                                    required property var modelData
                                    required property int index
                                    Layout.fillWidth: true
                                    spacing: 12

                                    // Cached IPv6 sub-map. Bindings in a hidden Section
                                    // still evaluate, so default to {} when absent.
                                    readonly property var ipv6: nicDelegate.modelData.ipv6 || ({})

                                    ColumnLayout {
                                        spacing: 2
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 24
                                        Layout.rightMargin: 24
                                        Layout.topMargin: 6
                                        Text {
                                            text: qsTr("INTERFACE %1").arg(nicDelegate.index)
                                            color: Colors.textMuted
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.letterSpacing: 2
                                            font.weight: Font.Medium
                                        }
                                        Text {
                                            text: nicDelegate.modelData.macAddress
                                                || nicDelegate.modelData.instanceId
                                            color: Colors.text
                                            font.family: Type.mono
                                            font.pixelSize: 18
                                        }
                                    }

                                    Section {
                                        title: qsTr("IPV4")
                                        accent: Colors.accent
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 24
                                        Layout.rightMargin: 24

                                        GridLayout {
                                            columns: 2
                                            columnSpacing: 16
                                            rowSpacing: 6
                                            Layout.fillWidth: true

                                            Text { text: qsTr("Addressing"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.dhcpEnabled ? qsTr("DHCP") : qsTr("Static"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("IP sync with OS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.ipSyncEnabled ? qsTr("Yes") : qsTr("No"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("IP address"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.ipAddress || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Subnet mask"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.subnetMask || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Gateway"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.defaultGateway || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Primary DNS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.primaryDns || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Secondary DNS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.secondaryDns || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Link policy"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.modelData.linkPolicyLabel || qsTr("(not set)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                        }
                                    }

                                    Section {
                                        title: qsTr("IPV6")
                                        visible: nicDelegate.ipv6.present === true
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 24
                                        Layout.rightMargin: 24

                                        GridLayout {
                                            columns: 2
                                            columnSpacing: 16
                                            rowSpacing: 6
                                            Layout.fillWidth: true

                                            Text { text: qsTr("Addresses"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.ipv6.addressesLabel || qsTr("(none)")
                                                   color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }

                                            Text { text: qsTr("Default router"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.ipv6.defaultRouter || qsTr("(none)")
                                                   color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Primary DNS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.ipv6.primaryDns || qsTr("(none)")
                                                   color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                            Text { text: qsTr("Secondary DNS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                            Text { text: nicDelegate.ipv6.secondaryDns || qsTr("(none)")
                                                   color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 4 — Wireless (WiFi + 802.1x)
                Loader {
                    active: root.currentSection === 4
                    sourceComponent: Flickable {
                        anchors.fill: parent
                        contentWidth: width
                        contentHeight: wirelessCol.implicitHeight + 48
                        clip: true

                        ColumnLayout {
                            id: wirelessCol
                            spacing: 18
                            width: parent.width

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                Layout.topMargin: 24
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Text {
                                    text: qsTr("WIRELESS")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.letterSpacing: 2
                                    font.weight: Font.Medium
                                }
                                Text {
                                    text: {
                                        const w = controller.wireless;
                                        if (!w || !w.ok)
                                            return qsTr("Not yet fetched");
                                        if (w.port && w.port.present)
                                            return w.port.currentSsid
                                                ? w.port.currentSsid
                                                : qsTr("Wireless interface");
                                        return qsTr("No wireless interface");
                                    }
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: 20
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    visible: !controller.wireless
                                          || !controller.wireless.ok
                                    text: qsTr("Click Refresh to fetch wireless state.")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                            }

                            // --- WiFi port + radio --------------------------
                            Section {
                                id: wifiPortSection
                                title: qsTr("WIFI PORT")
                                visible: (controller.wireless
                                           && controller.wireless.port
                                           && controller.wireless.port.present) === true
                                accent: Colors.accent
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                // `port` is referenced by every binding below; bindings in
                                // hidden Sections still evaluate, so cache it (or {}) here
                                // so the property reads never throw.
                                readonly property var wifiPort: (controller.wireless
                                                                  && controller.wireless.port) || ({})

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 16
                                    rowSpacing: 6
                                    Layout.fillWidth: true

                                    Text { text: qsTr("Port state"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: wifiPortSection.wifiPort.portStateLabel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Radio"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: wifiPortSection.wifiPort.radioStateLabel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Current SSID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: wifiPortSection.wifiPort.currentSsid || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text {
                                        text: qsTr("Local profile sync")
                                        color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS
                                        visible: wifiPortSection.wifiPort.localProfileSyncEnabled !== undefined
                                              && wifiPortSection.wifiPort.localProfileSyncEnabled !== -1
                                    }
                                    Text {
                                        text: wifiPortSection.wifiPort.localProfileSyncEnabled === 1
                                            ? qsTr("Enabled") : qsTr("Disabled")
                                        color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true
                                        visible: wifiPortSection.wifiPort.localProfileSyncEnabled !== undefined
                                              && wifiPortSection.wifiPort.localProfileSyncEnabled !== -1
                                    }
                                    Text {
                                        text: qsTr("UEFI profile sharing")
                                        color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS
                                        visible: wifiPortSection.wifiPort.uefiProfileShareEnabled !== undefined
                                              && wifiPortSection.wifiPort.uefiProfileShareEnabled !== -1
                                    }
                                    Text {
                                        text: wifiPortSection.wifiPort.uefiProfileShareEnabled === 1
                                            ? qsTr("Enabled") : qsTr("Disabled")
                                        color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true
                                        visible: wifiPortSection.wifiPort.uefiProfileShareEnabled !== undefined
                                              && wifiPortSection.wifiPort.uefiProfileShareEnabled !== -1
                                    }
                                }

                                // Actions row — radio state toggle + sync toggles.
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 10
                                    spacing: 8

                                    FlatButton {
                                        // portState 3 = Disabled; anything else (32768/32769) = enabled
                                        readonly property bool isOn: wifiPortSection.wifiPort.portState !== 3
                                        text: isOn ? qsTr("Disable WiFi") : qsTr("Enable WiFi")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        // Confirm on disable only (#278) — enable
                                        // is a benign re-arm, disable hard-stops
                                        // an active WiFi link.
                                        onClicked: {
                                            if (isOn) confirmPower.askFor(
                                                qsTr("Disable WiFi on \"%1\"?").arg(root.machineHost),
                                                qsTr("Stops AMT from using WiFi. "
                                                     + "If the device only has a "
                                                     + "WiFi link, you lose access."),
                                                qsTr("Disable"),
                                                () => controller.setWifiPortEnabled(false));
                                            else controller.setWifiPortEnabled(true);
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    FlatButton {
                                        text: qsTr("Sync settings…")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        onClicked: wifiSyncDialog.openForPort(wifiPortSection.wifiPort)
                                    }
                                }
                            }

                            // --- WiFi profiles ------------------------------
                            Section {
                                title: qsTr("WIFI PROFILES")
                                visible: (controller.wireless && controller.wireless.ok) === true
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Text {
                                        visible: ((controller.wireless && controller.wireless.profiles) || []).length === 0
                                        text: qsTr("No wireless profiles configured.")
                                        color: Colors.textFaint
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                    }
                                    Repeater {
                                        model: (controller.wireless && controller.wireless.profiles) || []
                                        delegate: ColumnLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 2
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8
                                                Text {
                                                    text: modelData.elementName
                                                       || qsTr("(unnamed)")
                                                    color: Colors.text
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeM
                                                    Layout.fillWidth: true
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    text: qsTr("priority %1").arg(modelData.priority)
                                                    color: Colors.textFaint
                                                    font.family: Type.mono
                                                    font.pixelSize: Type.sizeXs
                                                    visible: modelData.priority >= 0
                                                }
                                                FlatButton {
                                                    // EAP-bound profiles can't be edited here yet —
                                                    // the dialog is PSK-only. Hide the button
                                                    // until Phase C lands.
                                                    visible: (modelData.eap8021xProtocol === undefined
                                                           || modelData.eap8021xProtocol === -1)
                                                    text: qsTr("Edit")
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeXs
                                                    onClicked: wifiProfileDialog.openForEdit(modelData)
                                                }
                                                FlatButton {
                                                    text: qsTr("Delete")
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeXs
                                                    onClicked: {
                                                        wifiProfileConfirmDialog.ask(
                                                            qsTr("Delete WiFi profile?"),
                                                            qsTr("Removes %1 from the AMT firmware. The OS-side profile (if any) is unaffected.")
                                                                .arg(modelData.elementName),
                                                            qsTr("Delete"), true);
                                                        wifiProfileConfirmDialog.pendingName = modelData.elementName;
                                                        wifiProfileConfirmDialog.pendingMode = "single";
                                                    }
                                                }
                                            }
                                            Text {
                                                text: {
                                                    let s = "SSID " + (modelData.ssid || "(none)");
                                                    if (modelData.authMethodLabel)
                                                        s += " · " + modelData.authMethodLabel;
                                                    if (modelData.encryptionLabel)
                                                        s += " · " + modelData.encryptionLabel;
                                                    if ((modelData.eap8021xProtocolLabel || "").length > 0)
                                                        s += " · " + modelData.eap8021xProtocolLabel;
                                                    return s;
                                                }
                                                color: Colors.textMuted
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }

                                    // Add + bulk delete actions.
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 6
                                        spacing: 8

                                        FlatButton {
                                            text: qsTr("Delete all IT profiles")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: {
                                                wifiProfileConfirmDialog.ask(
                                                    qsTr("Wipe all IT-channel WiFi profiles?"),
                                                    qsTr("Removes every profile this management channel installed. OS-side / user profiles are kept. Cannot be undone."),
                                                    qsTr("Wipe IT"), true);
                                                wifiProfileConfirmDialog.pendingMode = "bulkIT";
                                            }
                                        }
                                        FlatButton {
                                            text: qsTr("Delete all user profiles")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: {
                                                wifiProfileConfirmDialog.ask(
                                                    qsTr("Wipe all user WiFi profiles?"),
                                                    qsTr("Removes every profile the OS pushed up via Local Profile Sync. The IT-channel profiles are kept."),
                                                    qsTr("Wipe user"), true);
                                                wifiProfileConfirmDialog.pendingMode = "bulkUser";
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        AccentButton {
                                            text: qsTr("Add profile…")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: wifiProfileDialog.openForAdd()
                                        }
                                    }
                                }
                            }

                            // --- Wired 802.1x ------------------------------
                            Section {
                                id: wiredSection
                                title: qsTr("WIRED 802.1X")
                                visible: (controller.wireless
                                           && controller.wireless.wired
                                           && controller.wireless.wired.present) === true
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Layout.bottomMargin: 24

                                readonly property var wired: (controller.wireless
                                                               && controller.wireless.wired) || ({})

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 16
                                    rowSpacing: 6
                                    Layout.fillWidth: true

                                    Text { text: qsTr("State"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: wiredSection.wired.enabled ? qsTr("Enabled") : qsTr("Disabled"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Protocol"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: wiredSection.wired.authProtocolLabel || qsTr("(none)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 8
                                    Item { Layout.fillWidth: true }
                                    FlatButton {
                                        text: qsTr("Edit…")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        onClicked: wired8021xDialog.openForEdit(wiredSection.wired)
                                    }
                                }
                            }
                        }
                    }
                }

                // 5 — Time (body in MachineDetails/TimeSection.qml).
                Loader {
                    active: root.currentSection === 5
                    sourceComponent: TimeSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 6 — Remote access (SOL / KVM / IDE-R launchers)
                Loader {
                    active: root.currentSection === 6
                    sourceComponent: ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Text {
                                text: qsTr("REMOTE ACCESS")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: qsTr("In-band channels to the AMT firmware on this machine.")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                            }
                        }

                        Section {
                            title: qsTr("USER CONSENT")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: controller.kvmOptInPolicy
                                        ? qsTr("KVM requires a 6-digit code displayed on the remote machine before the framebuffer unlocks.")
                                        : qsTr("Disabled — KVM, SOL, and IDE-R start without prompting for a code on the remote machine.")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    spacing: 10
                                    Layout.fillWidth: true

                                    Rectangle {
                                        implicitWidth: 10
                                        implicitHeight: 10
                                        radius: 5
                                        color: controller.kvmOptInPolicy ? Colors.standby : Colors.off
                                    }
                                    Text {
                                        text: controller.kvmOptInPolicy
                                            ? qsTr("Consent required")
                                            : qsTr("Not required")
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeS
                                        Layout.fillWidth: true
                                    }
                                    FlatButton {
                                        text: controller.kvmOptInPolicy
                                            ? qsTr("Disable")
                                            : qsTr("Enable")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        enabled: controller.canModifyOptInPolicy && !controller.busy
                                        // Disabling consent removes the security
                                        // requirement that the target's local
                                        // operator approve each KVM/SOL/IDE-R
                                        // session — significant security change.
                                        // Confirm on disable only (#278).
                                        onClicked: {
                                            if (controller.kvmOptInPolicy) confirmPower.askFor(
                                                qsTr("Disable consent policy on \"%1\"?").arg(root.machineHost),
                                                qsTr("After this, remote KVM / SOL "
                                                     + "/ IDE-R sessions no longer "
                                                     + "require a person at the "
                                                     + "target to approve each connect."),
                                                qsTr("Disable"),
                                                () => controller.setKvmOptInPolicyEnabled(false));
                                            else controller.setKvmOptInPolicyEnabled(true);
                                        }
                                    }
                                }
                                Text {
                                    visible: !controller.canModifyOptInPolicy
                                    text: qsTr("Note: the current AMT login lacks the privilege to change this policy. Log in with an administrator account to enable / disable consent.")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Section {
                            title: qsTr("SERIAL OVER LAN")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: qsTr("Console redirect over TCP. Useful for BIOS / bootloader access.")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                AccentButton {
                                    text: qsTr("Open SOL")
    enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                                    onClicked: sessionLoader.launchAt(0)
                                }
                            }
                        }

                        Section {
                            title: qsTr("REMOTE DESKTOP (KVM)")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: qsTr("Hardware-level keyboard/video/mouse for full OS-independent control.")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                AccentButton {
                                    text: qsTr("Open KVM")
    enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                                    onClicked: sessionLoader.launchAt(1)
                                }

                                // Device-level KVM settings (#175). These
                                // persist on the firmware and apply to every
                                // KVM session regardless of which console
                                // initiates it.
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 4
                                    spacing: 8
                                    FlatButton {
                                        text: qsTr("KVM settings…")
                                        enabled: !controller.busy
                                            && controller.canModifyOptInPolicy
                                        onClicked: kvmSettingsDialog.open()
                                    }
                                    FlatButton {
                                        text: controller.kvmEnabled
                                            ? qsTr("Disable KVM")
                                            : qsTr("Enable KVM")
                                        enabled: !controller.busy
                                            && controller.kvmAvailable
                                        onClicked: controller.setKvmServiceEnabled(!controller.kvmEnabled)
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("IDE REDIRECTION")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: qsTr("Mount a local .iso as a virtual CD/floppy on the target.")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                AccentButton {
                                    text: qsTr("Mount ISO…")
    enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                                    onClicked: sessionLoader.launchAt(2)
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // 7 — Remote access (CIRA)
                Loader {
                    active: root.currentSection === 7
                    sourceComponent: Flickable {
                        anchors.fill: parent
                        contentWidth: width
                        contentHeight: ciraCol.implicitHeight + 48
                        clip: true

                        ColumnLayout {
                            id: ciraCol
                            spacing: 18
                            width: parent.width

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                Layout.topMargin: 24
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Text {
                                    text: qsTr("REMOTE ACCESS (CIRA)")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.letterSpacing: 2
                                    font.weight: Font.Medium
                                }
                                Text {
                                    text: {
                                        const r = controller.remoteAccess;
                                        if (!r || !r.ok)
                                            return qsTr("Not yet fetched");
                                        const n = (r.servers || []).length;
                                        return n === 0
                                            ? qsTr("No MPS servers configured")
                                            : qsTr("%1 management server(s)").arg(n);
                                    }
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: 20
                                }
                                Text {
                                    visible: !controller.remoteAccess
                                          || !controller.remoteAccess.ok
                                    text: qsTr("Click Refresh to fetch the CIRA configuration.")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                            }

                            // --- Environment detection -------------------
                            Section {
                                title: qsTr("ENVIRONMENT DETECTION")
                                visible: (controller.remoteAccess && controller.remoteAccess.ok) === true
                                accent: Colors.accent
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 16
                                    rowSpacing: 6
                                    Layout.fillWidth: true

                                    Text { text: qsTr("State"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text {
                                        text: (controller.remoteAccess.envDetection
                                                && controller.remoteAccess.envDetection.enabled)
                                                ? qsTr("Enabled") : qsTr("Disabled")
                                        color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true
                                    }
                                    Text { text: qsTr("Domains"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text {
                                        text: (controller.remoteAccess.envDetection
                                                && controller.remoteAccess.envDetection.domainsLabel)
                                                || qsTr("(none)")
                                        color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; wrapMode: Text.WordWrap
                                    }
                                    Text { text: qsTr("User initiation"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text {
                                        text: (controller.remoteAccess.userInitiated
                                                && controller.remoteAccess.userInitiated.label)
                                                || qsTr("(unknown)")
                                        color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 8
                                    Item { Layout.fillWidth: true }
                                    FlatButton {
                                        text: qsTr("User initiation…")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        onClicked: userInitiatedDialog.openForEdit(
                                            controller.remoteAccess.userInitiated)
                                    }
                                    FlatButton {
                                        text: qsTr("Domains…")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        onClicked: envDetectionDialog.openForEdit(
                                            controller.remoteAccess.envDetection)
                                    }
                                }
                            }

                            // --- Policies (User Initiated / Alert / Periodic) ---
                            Section {
                                title: qsTr("CONNECTION POLICIES")
                                visible: !!(controller.remoteAccess
                                             && controller.remoteAccess.ok)
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Text {
                                        visible: ((controller.remoteAccess && controller.remoteAccess.policies) || []).length === 0
                                        text: qsTr("(no policies configured)")
                                        color: Colors.textFaint
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                    }
                                    Repeater {
                                        model: (controller.remoteAccess && controller.remoteAccess.policies) || []
                                        delegate: ColumnLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 2
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8
                                                Text {
                                                    text: modelData.name || "—"
                                                    color: Colors.text
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeM
                                                    Layout.preferredWidth: 140
                                                }
                                                Text {
                                                    text: (modelData.mpsNamesLabel || "").length > 0
                                                        ? modelData.mpsNamesLabel
                                                        : qsTr("(no servers)")
                                                    color: Colors.textMuted
                                                    font.family: Type.mono
                                                    font.pixelSize: Type.sizeXs
                                                    Layout.fillWidth: true
                                                    elide: Text.ElideRight
                                                }
                                                FlatButton {
                                                    text: qsTr("Edit")
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeXs
                                                    onClicked: ciraPolicyDialog.openForEdit(modelData)
                                                }
                                                FlatButton {
                                                    text: qsTr("Delete")
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeXs
                                                    onClicked: {
                                                        ciraPolicyConfirmDialog.ask(
                                                            qsTr("Delete CIRA policy?"),
                                                            qsTr("Removes the %1 policy. AMT cascades its MPS bindings.").arg(modelData.name),
                                                            qsTr("Delete"), true);
                                                        ciraPolicyConfirmDialog.pendingName = modelData.name;
                                                    }
                                                }
                                            }
                                            Text {
                                                visible: (modelData.scheduleLabel || "").length > 0
                                                      || modelData.tunnelLifeTime > 0
                                                text: {
                                                    let s = "";
                                                    if ((modelData.scheduleLabel || "").length > 0)
                                                        s += modelData.scheduleLabel;
                                                    if (modelData.tunnelLifeTime > 0) {
                                                        if (s.length > 0) s += " · ";
                                                        s += qsTr("tunnel %1 s").arg(modelData.tunnelLifeTime);
                                                    }
                                                    return s;
                                                }
                                                color: Colors.textFaint
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 6
                                        spacing: 8
                                        Item { Layout.fillWidth: true }
                                        AccentButton {
                                            text: qsTr("Add policy…")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            // No point letting the user open the
                                            // dialog when there are no MPS rows
                                            // to bind — AMT requires at least one.
                                            enabled: ((controller.remoteAccess
                                                        && controller.remoteAccess.servers) || []).length > 0
                                            onClicked: ciraPolicyDialog.openForAdd()
                                        }
                                    }
                                }
                            }

                            // --- MPS servers ----------------------------
                            Section {
                                title: qsTr("MANAGEMENT PRESENCE SERVERS")
                                visible: (controller.remoteAccess
                                           && controller.remoteAccess.ok) === true
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Text {
                                        visible: ((controller.remoteAccess && controller.remoteAccess.servers) || []).length === 0
                                        text: qsTr("(none configured)")
                                        color: Colors.textFaint
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                    }
                                    Repeater {
                                        model: (controller.remoteAccess && controller.remoteAccess.servers) || []
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Text {
                                                text: qsTr("%1:%2")
                                                    .arg(modelData.accessInfo || "")
                                                    .arg(modelData.port)
                                                color: Colors.text
                                                font.family: Type.mono
                                                font.pixelSize: Type.sizeS
                                                Layout.preferredWidth: 240
                                            }
                                            Text {
                                                visible: modelData.cila === true
                                                text: qsTr("CILA")
                                                color: Colors.standby
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                font.weight: Font.Medium
                                                font.letterSpacing: 1
                                            }
                                            Text {
                                                text: (modelData.cn && modelData.cn.length > 0)
                                                    ? qsTr("CN: %1").arg(modelData.cn)
                                                    : ""
                                                color: Colors.textMuted
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }
                                            FlatButton {
                                                text: qsTr("Edit")
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                onClicked: mpsServerDialog.openForEdit(modelData)
                                            }
                                            FlatButton {
                                                text: qsTr("Delete")
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                onClicked: {
                                                    mpsConfirmDialog.ask(
                                                        qsTr("Delete MPS server?"),
                                                        qsTr("Removes %1 from the AMT CIRA stack. Linked auth credentials cascade.").arg(modelData.name),
                                                        qsTr("Delete"), true);
                                                    mpsConfirmDialog.pendingName = modelData.name;
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 6
                                        spacing: 8
                                        Item { Layout.fillWidth: true }
                                        AccentButton {
                                            text: qsTr("Add server…")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: mpsServerDialog.openForAdd()
                                        }
                                    }
                                }
                            }

                            // --- HTTP proxies (AMT 11+) -----------------
                            Section {
                                title: qsTr("HTTP PROXIES")
                                visible: controller.remoteAccess
                                      && controller.remoteAccess.httpProxySupported === true
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Layout.bottomMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Text {
                                        visible: (controller.remoteAccess.httpProxies || []).length === 0
                                        text: qsTr("(none configured)")
                                        color: Colors.textFaint
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                    }
                                    Repeater {
                                        model: controller.remoteAccess.httpProxies || []
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Text {
                                                text: qsTr("%1:%2")
                                                    .arg(modelData.accessInfo || "")
                                                    .arg(modelData.port)
                                                color: Colors.text
                                                font.family: Type.mono
                                                font.pixelSize: Type.sizeS
                                                Layout.preferredWidth: 240
                                            }
                                            Text {
                                                text: modelData.networkDnsSuffix || ""
                                                color: Colors.textMuted
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }
                                            FlatButton {
                                                text: qsTr("Delete")
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                onClicked: controller.deleteHttpProxy(modelData.name || "")
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 6
                                        spacing: 8
                                        Text {
                                            text: qsTr("%1 / 15")
                                                .arg((controller.remoteAccess.httpProxies || []).length)
                                            color: Colors.textFaint
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                        }
                                        Item { Layout.fillWidth: true }
                                        AccentButton {
                                            text: qsTr("Add proxy…")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            enabled: (controller.remoteAccess.httpProxies || []).length < 15
                                            onClicked: httpProxyDialog.openForAdd()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 8 — Certificates (locally pinned)
                Loader {
                    active: root.currentSection === 8
                    sourceComponent: ColumnLayout {
                        anchors.fill: parent
                        spacing: 18

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Text {
                                text: qsTr("PINNED CERTIFICATES")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: root.machineTrustedFingerprints.length === 0
                                    ? qsTr("No certificate pinned yet.")
                                    : qsTr("%1 SHA-256 fingerprint(s) trusted on first use.")
                                        .arg(root.machineTrustedFingerprints.length)
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        Section {
                            title: qsTr("FINGERPRINTS")
                            visible: root.machineTrustedFingerprints.length > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                spacing: 6
                                Layout.fillWidth: true
                                Repeater {
                                    model: root.machineTrustedFingerprints
                                    delegate: Text {
                                        required property string modelData
                                        text: modelData
                                        color: Colors.textMuted
                                        font.family: Type.mono
                                        font.pixelSize: Type.sizeXs
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // 9 — Device certificate store
                Loader {
                    active: root.currentSection === 9
                    sourceComponent: Flickable {
                        anchors.fill: parent
                        contentWidth: width
                        contentHeight: devCertCol.implicitHeight + 48
                        clip: true

                        ColumnLayout {
                            id: devCertCol
                            spacing: 18
                            width: parent.width

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                Layout.topMargin: 24
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Text {
                                    text: qsTr("DEVICE CERTIFICATES")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.letterSpacing: 2
                                    font.weight: Font.Medium
                                }
                                Text {
                                    text: {
                                        const s = controller.deviceCertStore;
                                        if (!s || !s.certificates)
                                            return qsTr("Not yet fetched");
                                        return qsTr("%1 certificate(s)")
                                            .arg(s.certificates.length);
                                    }
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: 20
                                }
                                Text {
                                    visible: !controller.deviceCertStore
                                          || !controller.deviceCertStore.certificates
                                    text: qsTr("Click Refresh to fetch the AMT device's certificate store.")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                            }

                            // --- TLS modes ----------------------------------
                            Section {
                                title: qsTr("TLS MODES")
                                visible: !!(controller.deviceCertStore
                                             && controller.deviceCertStore.tlsSettings
                                             && controller.deviceCertStore.tlsSettings.length > 0)
                                accent: Colors.accent
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Repeater {
                                        model: (controller.deviceCertStore && controller.deviceCertStore.tlsSettings) || []
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 12

                                            Text {
                                                text: modelData.isLocal
                                                    ? qsTr("Local (LMS)")
                                                    : qsTr("Remote (16993)")
                                                color: Colors.textMuted
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeS
                                                Layout.preferredWidth: 160
                                            }
                                            Text {
                                                text: {
                                                    let s = modelData.label;
                                                    if (modelData.mutualAuthentication
                                                        && modelData.trustedCnLabel
                                                           && modelData.trustedCnLabel.length > 0)
                                                        s += " · " + qsTr("Trusted: ")
                                                          + modelData.trustedCnLabel;
                                                    return s;
                                                }
                                                color: Colors.text
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeS
                                                Layout.fillWidth: true
                                                wrapMode: Text.WordWrap
                                            }
                                            FlatButton {
                                                text: qsTr("Edit…")
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                onClicked: tlsModeDialog.openForRow(modelData)
                                            }
                                        }
                                    }
                                }
                            }

                            // --- Certificates ------------------------------
                            Section {
                                title: qsTr("CERTIFICATES")
                                visible: !!(controller.deviceCertStore
                                             && controller.deviceCertStore.certificates
                                             && controller.deviceCertStore.certificates.length > 0)
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Item { Layout.fillWidth: true }
                                        FlatButton {
                                            text: qsTr("Issue certificate…")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            ToolTip.text: qsTr("Generate a fresh key pair on the device and obtain a CA-signed cert")
                                            ToolTip.visible: hovered
                                            onClicked: issueCertificateDialog.openForIssue()
                                        }
                                        AccentButton {
                                            text: qsTr("Add certificate…")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: addCertificateDialog.openForAdd()
                                        }
                                    }
                                    Repeater {
                                        model: (controller.deviceCertStore && controller.deviceCertStore.certificates) || []
                                        delegate: Rectangle {
                                            required property var modelData
                                            required property int index
                                            Layout.fillWidth: true
                                            implicitHeight: certCol.implicitHeight + 12
                                            color: index % 2 === 0
                                                ? "transparent" : Colors.elevated
                                            radius: 4

                                            ColumnLayout {
                                                id: certCol
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                anchors.topMargin: 6
                                                spacing: 2

                                                RowLayout {
                                                    spacing: 8
                                                    Layout.fillWidth: true

                                                    Text {
                                                        text: modelData.subjectCn
                                                           || modelData.subjectRaw
                                                           || qsTr("(unnamed cert)")
                                                        color: Colors.text
                                                        font.family: Type.sans
                                                        font.pixelSize: Type.sizeM
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }
                                                    Text {
                                                        visible: modelData.trustedRoot === true
                                                        text: qsTr("TRUSTED ROOT")
                                                        color: Colors.accent
                                                        font.family: Type.sans
                                                        font.pixelSize: Type.sizeXs
                                                        font.weight: Font.Medium
                                                        font.letterSpacing: 1
                                                    }
                                                    Text {
                                                        visible: modelData.hasPrivateKey === true
                                                        text: qsTr("KEY")
                                                        color: Colors.standby
                                                        font.family: Type.sans
                                                        font.pixelSize: Type.sizeXs
                                                        font.weight: Font.Medium
                                                        font.letterSpacing: 1
                                                    }
                                                    Text {
                                                        visible: modelData.active === true
                                                        text: qsTr("ACTIVE")
                                                        color: Colors.text
                                                        font.family: Type.sans
                                                        font.pixelSize: Type.sizeXs
                                                        font.weight: Font.Medium
                                                        font.letterSpacing: 1
                                                    }
                                                }
                                                Text {
                                                    text: qsTr("Issued by %1")
                                                        .arg(modelData.issuerCn
                                                             || modelData.issuerRaw
                                                             || qsTr("(unknown)"))
                                                    color: Colors.textMuted
                                                    font.family: Type.sans
                                                    font.pixelSize: Type.sizeXs
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8

                                                    Text {
                                                        text: qsTr("%1 bytes").arg(modelData.derSizeBytes || 0)
                                                        color: Colors.textFaint
                                                        font.family: Type.mono
                                                        font.pixelSize: Type.sizeXs
                                                        Layout.fillWidth: true
                                                    }
                                                    FlatButton {
                                                        text: qsTr("Details…")
                                                        font.family: Type.sans
                                                        font.pixelSize: Type.sizeXs
                                                        onClicked: certDetailsDialog.openForCert(modelData)
                                                    }
                                                    FlatButton {
                                                        text: qsTr("Delete")
                                                        font.family: Type.sans
                                                        font.pixelSize: Type.sizeXs
                                                        onClicked: {
                                                            certConfirmDialog.ask(
                                                                modelData.active === true
                                                                    ? qsTr("Delete the ACTIVE TLS certificate?")
                                                                    : qsTr("Delete certificate?"),
                                                                modelData.active === true
                                                                    ? qsTr("This cert is currently bound by AMT_TLSCredentialContext. Removing it breaks TLS until a new cert is bound — the device may become unreachable on its TLS port.")
                                                                    : qsTr("This removes %1 from the device cert store. The AMT handle is freed and cannot be undeleted.").arg(modelData.subjectCn || modelData.subjectRaw || qsTr("the certificate")),
                                                                qsTr("Delete"),
                                                                true);
                                                            certConfirmDialog.pendingInstance = modelData.instanceId;
                                                            certConfirmDialog.pendingKind = "cert";
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // --- Orphan key pairs -------------------------
                            Section {
                                title: qsTr("UNASSIGNED PRIVATE KEYS")
                                visible: !!(controller.deviceCertStore
                                             && controller.deviceCertStore.orphanKeys
                                             && controller.deviceCertStore.orphanKeys.length > 0)
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Layout.bottomMargin: 24

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Repeater {
                                        model: (controller.deviceCertStore && controller.deviceCertStore.orphanKeys) || []
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Text {
                                                text: qsTr("%1 — %2 bytes")
                                                    .arg(modelData.instanceId)
                                                    .arg(modelData.derSizeBytes)
                                                color: Colors.textMuted
                                                font.family: Type.mono
                                                font.pixelSize: Type.sizeXs
                                                elide: Text.ElideMiddle
                                                Layout.fillWidth: true
                                            }
                                            FlatButton {
                                                text: qsTr("Delete")
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                onClicked: {
                                                    certConfirmDialog.ask(
                                                        qsTr("Delete orphan private key?"),
                                                        qsTr("Removes %1 from the AMT key store. Orphan keys have no matching cert, so this is usually safe — but cannot be undone.").arg(modelData.instanceId),
                                                        qsTr("Delete"),
                                                        true);
                                                    certConfirmDialog.pendingInstance = modelData.instanceId;
                                                    certConfirmDialog.pendingKind = "key";
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 10 — EventLog (body in MachineDetails/EventLogSection.qml).
                Loader {
                    active: root.currentSection === 10
                    sourceComponent: EventLogSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 11 — AuditLog (body in MachineDetails/AuditLogSection.qml).
                Loader {
                    active: root.currentSection === 11
                    sourceComponent: AuditLogSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 12 — User accounts
                Loader {
                    active: root.currentSection === 12
                    sourceComponent: ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        // Hidden-account toggle drives the model filter
                        // below. AMT marks internal accounts with names
                        // starting in `$$` (e.g. `$$OsAdmin`); operators
                        // rarely want to see them.
                        property bool showHidden: false
                        function filteredAccounts() {
                            if (showHidden) return controller.userAccounts;
                            return (controller.userAccounts || []).filter(a => !a.hidden);
                        }

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("USER ACCOUNTS")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Text {
                                    text: controller.userAccounts.length === 0
                                        ? qsTr("No accounts.")
                                        : qsTr("%1 accounts")
                                            .arg(parent.parent.parent.filteredAccounts().length)
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: 20
                                    Layout.fillWidth: true
                                }
                                CheckBox {
                                    text: qsTr("Show hidden ($$)")
                                    checked: parent.parent.parent.showHidden
                                    onToggled: parent.parent.parent.showHidden = checked
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Item { Layout.fillWidth: true }
                                FlatButton {
                                    text: qsTr("Rotate admin…")
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    onClicked: {
                                        // Default the dialog's username to the
                                        // admin row we already enumerated, if
                                        // any — otherwise it stays blank.
                                        let adminUser = "";
                                        for (let i = 0; i < (controller.userAccounts || []).length; ++i) {
                                            const a = controller.userAccounts[i];
                                            if (a.handle === -1) {
                                                adminUser = a.digestUsername || "";
                                                break;
                                            }
                                        }
                                        adminPasswordDialog.openForRotate(adminUser);
                                    }
                                }
                                AccentButton {
                                    text: qsTr("Add account…")
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    onClicked: userAccountDialog.openForAdd()
                                }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24
                            clip: true
                            model: parent.filteredAccounts()
                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: ListView.view.width
                                implicitHeight: 56
                                color: index % 2 === 0 ? "transparent" : Colors.elevated
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    anchors.topMargin: 6
                                    anchors.bottomMargin: 6
                                    spacing: 2

                                    RowLayout {
                                        spacing: 8
                                        Layout.fillWidth: true

                                        Text {
                                            text: modelData.name || qsTr("(unnamed)")
                                            color: Colors.text
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeM
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            visible: modelData.handle === -1
                                            text: qsTr("ADMIN")
                                            color: Colors.accent
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.weight: Font.Medium
                                            font.letterSpacing: 1
                                        }
                                        Text {
                                            visible: modelData.isKerberos === true
                                            text: qsTr("KERBEROS")
                                            color: Colors.textMuted
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.weight: Font.Medium
                                            font.letterSpacing: 1
                                        }
                                        Text {
                                            visible: !modelData.enabled
                                            text: qsTr("DISABLED")
                                            color: Colors.standby
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.weight: Font.Medium
                                            font.letterSpacing: 1
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: {
                                                if (modelData.handle === -1)
                                                    return qsTr("Administrator (full access)");
                                                if (modelData.isAdmin)
                                                    return modelData.accessPermissionLabel
                                                        + " · " + qsTr("Administrator");
                                                const r = modelData.realmsLabel || "";
                                                return r.length > 0
                                                    ? modelData.accessPermissionLabel + " · " + r
                                                    : modelData.accessPermissionLabel;
                                            }
                                            color: Colors.textMuted
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // Per-row actions. Admin entry only
                                        // supports rotation via the top-bar
                                        // button so the row actions are hidden.
                                        FlatButton {
                                            visible: modelData.handle !== -1
                                                     && !modelData.isKerberos
                                            text: qsTr("Edit")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: userAccountDialog.openForEdit(modelData)
                                        }
                                        FlatButton {
                                            visible: modelData.handle !== -1
                                            text: modelData.enabled
                                                ? qsTr("Disable")
                                                : qsTr("Enable")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: {
                                                const isOwnRow = modelData.digestUsername
                                                    === controller.user;
                                                if (modelData.enabled && isOwnRow) {
                                                    confirmDialog.ask(
                                                        qsTr("Disable your own account?"),
                                                        qsTr("This will lock your current AMT session out. You'll need another account to reconnect."),
                                                        qsTr("Disable anyway"),
                                                        true);
                                                    confirmDialog.pendingHandle = modelData.handle;
                                                    confirmDialog.pendingAction = "disable";
                                                } else {
                                                    controller.setAccountEnabled(
                                                        modelData.handle, !modelData.enabled);
                                                }
                                            }
                                        }
                                        FlatButton {
                                            visible: modelData.handle !== -1
                                            text: qsTr("Delete")
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            onClicked: {
                                                const isOwnRow = modelData.digestUsername
                                                    === controller.user;
                                                confirmDialog.ask(
                                                    isOwnRow
                                                        ? qsTr("Delete your own account?")
                                                        : qsTr("Delete user account?"),
                                                    isOwnRow
                                                        ? qsTr("Removing %1 will lock this AMT session out immediately. You'll need another account to reconnect.").arg(modelData.name)
                                                        : qsTr("Removing %1 cannot be undone — the AMT handle is freed and any clients using it will fail to authenticate.").arg(modelData.name),
                                                    qsTr("Delete"),
                                                    true);
                                                confirmDialog.pendingHandle = modelData.handle;
                                                confirmDialog.pendingAction = "delete";
                                            }
                                        }
                                    }
                                }
                            }

                            Text {
                                visible: controller.userAccounts.length === 0 && !controller.busy
                                anchors.centerIn: parent
                                text: qsTr("No user accounts.")
                                color: Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                            }
                        }
                    }
                }

                // 13 — Watchdogs (body in MachineDetails/WatchdogsSection.qml).
                Loader {
                    active: root.currentSection === 13
                    sourceComponent: WatchdogsSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 14 — Event subscriptions (body in MachineDetails/EventSubscriptionsSection.qml).
                Loader {
                    active: root.currentSection === 14
                    sourceComponent: EventSubscriptionsSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 15 — Wake alarms (body in MachineDetails/WakeAlarmsSection.qml).
                Loader {
                    active: root.currentSection === 15
                    sourceComponent: WakeAlarmsSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 16 — WSMAN browser (dev tool, #167) (body in MachineDetails/WsmanBrowserSection.qml).
                Loader {
                    active: root.currentSection === 16
                    sourceComponent: WsmanBrowserSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 17 — System Defense (ACM only, #165) (body in MachineDetails/SystemDefenseSection.qml).
                Loader {
                    active: root.currentSection === 17
                    sourceComponent: SystemDefenseSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 18 — Active sessions (body in MachineDetails/ActiveSessionsSection.qml).
                Loader {
                    active: root.currentSection === 18
                    sourceComponent: ActiveSessionsSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }
            }
        }
    }

    // -- Embedded session windows ------------------------------------
    /// Single tabbed session window. Each "Open SOL / KVM / IDE-R"
    /// button on the Remote-access page funnels into this Loader and
    /// sets the initial tab before launching, so the existing UX of
    /// "click the verb you want" still picks the right pane while
    /// keeping all three sessions in one OS window per machine.
    Loader {
        id: sessionLoader
        active: false
        asynchronous: true
        property int pendingTab: 0
        function launchAt(tab) {
            if (active && item !== null && item.visible) {
                item.openTab(tab);
                item.raise();
                return;
            }
            pendingTab = tab;
            active = false;
            active = true;
        }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
            item.user = root.machineUser;
            item.password = root.machinePass;
            item.tls = root.machineTls;
            item.trustedFingerprints = root.machineTrustedFingerprints;
            item.machineSshConfig = root.machineSshConfig || ({});
            item.label = root.machineName.length > 0 ? root.machineName : root.machineHost;
            // initialTab is read once by Component.onCompleted; by the
            // time we get here it has already run with the default 0, so
            // also drive openTab() to actually switch the visible tab.
            item.initialTab = pendingTab;
            item.visible = true;
            item.openTab(pendingTab);
        }
        onStatusChanged: if (status === Loader.Ready) openWindow()
        sourceComponent: SessionWindow {
            onClosing: sessionLoader.active = false
            onTrustedFingerprintPersistRequested: function(fp) {
                root.trustedFingerprintPersistRequested(fp);
            }
            onTrustedSshHostKeyPersistRequested: function(fp) {
                root.trustedSshHostKeyPersistRequested(fp);
            }
        }
    }
}
