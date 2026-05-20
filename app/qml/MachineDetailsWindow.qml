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

    WakeAlarmDialog {
        id: wakeAlarmDialog
        onConfirmed: function(editingInstanceId, fields) {
            if (editingInstanceId.length > 0)
                controller.replaceWakeAlarm(editingInstanceId, fields);
            else
                controller.addWakeAlarm(fields);
        }
    }

    ConfirmDialog {
        id: wakeAlarmConfirmDialog
        property string pendingInstanceId: ""
        onProceed: {
            if (pendingInstanceId.length > 0)
                controller.deleteWakeAlarm(pendingInstanceId);
            pendingInstanceId = "";
        }
        onRejected: pendingInstanceId = ""
    }

    WatchdogDialog {
        id: watchdogDialog
        onConfirmed: function(editingDeviceIdGuid, fields) {
            if (editingDeviceIdGuid.length > 0)
                controller.replaceAgentPresenceWatchdog(editingDeviceIdGuid, fields);
            else
                controller.addAgentPresenceWatchdog(fields);
        }
    }

    ConfirmDialog {
        id: watchdogConfirmDialog
        property string pendingDeviceIdGuid: ""
        onProceed: {
            if (pendingDeviceIdGuid.length > 0)
                controller.deleteAgentPresenceWatchdog(pendingDeviceIdGuid);
            pendingDeviceIdGuid = "";
        }
        onRejected: pendingDeviceIdGuid = ""
    }

    EventSubscriptionDialog {
        id: eventSubscriptionDialog
        controller: controller
        onConfirmed: function(filterInstanceId, deliveryMode,
                              notifyUrl, user, pass) {
            controller.subscribeToEventFilter(filterInstanceId, deliveryMode,
                                                notifyUrl, user, pass);
        }
    }

    Hdr8021FilterDialog {
        id: hdr8021FilterDialog
        onConfirmed: function(fields) {
            controller.addSystemDefenseHdrFilter(fields);
        }
    }

    IpHeadersFilterDialog {
        id: ipHeadersFilterDialog
        onConfirmed: function(fields) {
            controller.addSystemDefenseIpFilter(fields);
        }
    }

    SystemDefensePolicyDialog {
        id: systemDefensePolicyDialog
        controller: controller
        onConfirmed: function(fields) {
            controller.addSystemDefensePolicyAction(fields);
        }
    }

    ConfirmDialog {
        id: systemDefenseConfirmDialog
        property string pendingInstanceId: ""
        property string pendingKind: "" // "policy" | "hdr" | "ip"
        onProceed: {
            if (pendingInstanceId.length > 0) {
                if (pendingKind === "policy")
                    controller.deleteSystemDefensePolicy(pendingInstanceId);
                else if (pendingKind === "hdr")
                    controller.deleteSystemDefenseHdrFilter(pendingInstanceId);
                else if (pendingKind === "ip")
                    controller.deleteSystemDefenseIpFilter(pendingInstanceId);
            }
            pendingInstanceId = "";
            pendingKind = "";
        }
        onRejected: {
            pendingInstanceId = "";
            pendingKind = "";
        }
    }

    ConfirmDialog {
        id: eventSubscriptionConfirmDialog
        property string pendingFilterInstanceId: ""
        property string pendingListenerName: ""
        onProceed: {
            if (pendingFilterInstanceId.length > 0
                && pendingListenerName.length > 0) {
                controller.unsubscribeFromEventFilter(
                    pendingFilterInstanceId, pendingListenerName);
            }
            pendingFilterInstanceId = "";
            pendingListenerName = "";
        }
        onRejected: {
            pendingFilterInstanceId = "";
            pendingListenerName = "";
        }
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

                // 2 — Power (body in MachineDetails/PowerSection.qml).
                Loader {
                    active: root.currentSection === 2
                    sourceComponent: PowerSection {
                        anchors.fill: parent
                        controller: controller
                        machineHost: root.machineHost
                        confirmPower: confirmPower
                        powerPolicyDialog: powerPolicyDialog
                        secureErasePrompt: secureErasePrompt
                        platformErasePrompt: platformErasePrompt
                        httpsBootPrompt: httpsBootPrompt
                        ocrPrompt: ocrPrompt
                    }
                }

                // 3 — Network (body in MachineDetails/NetworkSection.qml).
                Loader {
                    active: root.currentSection === 3
                    sourceComponent: NetworkSection {
                        anchors.fill: parent
                        controller: controller
                    }
                }

                // 4 — Wireless (WiFi + 802.1x) (body in MachineDetails/WirelessSection.qml).
                Loader {
                    active: root.currentSection === 4
                    sourceComponent: WirelessSection {
                        anchors.fill: parent
                        controller: controller
                        machineHost: root.machineHost
                        confirmPower: confirmPower
                        wifiProfileDialog: wifiProfileDialog
                        wifiSyncDialog: wifiSyncDialog
                        wired8021xDialog: wired8021xDialog
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

                // 6 — Remote access (SOL / KVM / IDE-R launchers) (body in MachineDetails/SessionLaunchersSection.qml).
                Loader {
                    active: root.currentSection === 6
                    sourceComponent: SessionLaunchersSection {
                        anchors.fill: parent
                        controller: controller
                        machineHost: root.machineHost
                        machineUser: root.machineUser
                        confirmPower: confirmPower
                        sessionLoader: sessionLoader
                    }
                }

                // 7 — Remote access (CIRA) (body in MachineDetails/CiraSection.qml).
                Loader {
                    active: root.currentSection === 7
                    sourceComponent: CiraSection {
                        anchors.fill: parent
                        controller: controller
                        envDetectionDialog: envDetectionDialog
                        mpsServerDialog: mpsServerDialog
                        ciraPolicyDialog: ciraPolicyDialog
                        userInitiatedDialog: userInitiatedDialog
                    }
                }

                // 8 — Certificates (locally pinned) (body in MachineDetails/PinnedCertificatesSection.qml).
                Loader {
                    active: root.currentSection === 8
                    sourceComponent: PinnedCertificatesSection {
                        anchors.fill: parent
                        machineTrustedFingerprints: root.machineTrustedFingerprints
                    }
                }

                // 9 — Device certificate store (body in MachineDetails/DeviceCertStoreSection.qml).
                Loader {
                    active: root.currentSection === 9
                    sourceComponent: DeviceCertStoreSection {
                        anchors.fill: parent
                        controller: controller
                        addCertificateDialog: addCertificateDialog
                        issueCertificateDialog: issueCertificateDialog
                        certDetailsDialog: certDetailsDialog
                        tlsModeDialog: tlsModeDialog
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

                // 12 — User accounts (body in MachineDetails/UserAccountsSection.qml).
                Loader {
                    active: root.currentSection === 12
                    sourceComponent: UserAccountsSection {
                        anchors.fill: parent
                        controller: controller
                        userAccountDialog: userAccountDialog
                        adminPasswordDialog: adminPasswordDialog
                    }
                }

                // 13 — Watchdogs (body in MachineDetails/WatchdogsSection.qml).
                Loader {
                    active: root.currentSection === 13
                    sourceComponent: WatchdogsSection {
                        anchors.fill: parent
                        controller: controller
                        watchdogDialog: watchdogDialog
                    }
                }

                // 14 — Event subscriptions (body in MachineDetails/EventSubscriptionsSection.qml).
                Loader {
                    active: root.currentSection === 14
                    sourceComponent: EventSubscriptionsSection {
                        anchors.fill: parent
                        controller: controller
                        eventSubscriptionDialog: eventSubscriptionDialog
                    }
                }

                // 15 — Wake alarms (body in MachineDetails/WakeAlarmsSection.qml).
                Loader {
                    active: root.currentSection === 15
                    sourceComponent: WakeAlarmsSection {
                        anchors.fill: parent
                        controller: controller
                        wakeAlarmDialog: wakeAlarmDialog
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
                        hdr8021FilterDialog: hdr8021FilterDialog
                        ipHeadersFilterDialog: ipHeadersFilterDialog
                        systemDefensePolicyDialog: systemDefensePolicyDialog
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
