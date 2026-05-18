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
        { key: "overview",  label: qsTr("Overview"),       icon: "■" },
        { key: "hardware",  label: qsTr("Hardware"),       icon: "▦" },
        { key: "power",     label: qsTr("Power"),          icon: "⏻" },
        { key: "network",   label: qsTr("Network"),        icon: "≋" },
        { key: "wireless",  label: qsTr("Wireless"),       icon: "📶" },
        { key: "time",      label: qsTr("Time"),           icon: "◷" },
        { key: "remote",    label: qsTr("Remote access"),  icon: "▶" },
        { key: "cira",      label: qsTr("CIRA"),           icon: "⤴" },
        { key: "certs",     label: qsTr("Pinned trust"),   icon: "🔒" },
        { key: "devcerts",  label: qsTr("Device certs"),   icon: "✦" },
        { key: "events",    label: qsTr("Event log"),      icon: "≡" },
        { key: "audit",     label: qsTr("Audit log"),      icon: "✓" },
        { key: "users",     label: qsTr("User accounts"),  icon: "⌥" },
        { key: "watchdogs", label: qsTr("Watchdogs"),      icon: "🐕" },
        { key: "subs",      label: qsTr("Subscriptions"),  icon: "🔔" },
        { key: "alarms",    label: qsTr("Wake alarms"),    icon: "⏰" },
        { key: "wsman",     label: qsTr("WSMAN"),          icon: "⌬" },
        { key: "sysdef",    label: qsTr("System Defense"), icon: "🛡" },
        { key: "sessions",  label: qsTr("Sessions"),       icon: "⇄" },
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
                    interactive: false

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
                            onTapped: {
                                root.currentSection = index;
                                nav.currentIndex = index;
                            }
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

                            Text {
                                text: parent.parent.modelData.icon
                                color: Colors.textMuted
                                font.pixelSize: Type.sizeS
                                Layout.preferredWidth: 14
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

            // Error banner across all sections.
            Rectangle {
                visible: controller.lastError.length > 0
                color: Colors.bannerFailed
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                implicitHeight: errText.implicitHeight + 12
                z: 10

                Text {
                    id: errText
                    text: controller.lastError
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    wrapMode: Text.WordWrap
                    anchors.fill: parent
                    anchors.margins: 6
                }
            }

            StackLayout {
                anchors.fill: parent
                anchors.topMargin: controller.lastError.length > 0 ? errText.implicitHeight + 12 : 0
                currentIndex: root.currentSection

                // 0 — Overview
                Flickable {
                    contentWidth: width
                    contentHeight: overviewCol.implicitHeight + 48
                    clip: true

                    ColumnLayout {
                        id: overviewCol
                        spacing: 18
                        width: parent.width

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("OVERVIEW")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: root.machineName.length > 0 ? root.machineName : root.machineHost
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: 24
                                font.weight: Font.Medium
                            }

                            Rectangle {
                                visible: controller.sshTunnelStatus.length > 0
                                Layout.topMargin: 6
                                Layout.preferredHeight: tunnelBadgeText.implicitHeight + 6
                                Layout.preferredWidth: tunnelBadgeText.implicitWidth + 16
                                color: controller.sshTunnelActive
                                    ? Colors.accent
                                    : Colors.borderMuted
                                radius: 4
                                Text {
                                    id: tunnelBadgeText
                                    anchors.centerIn: parent
                                    text: controller.sshTunnelStatus
                                    color: controller.sshTunnelActive
                                        ? Colors.surface
                                        : Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.weight: Font.Medium
                                }
                            }
                        }

                        Section {
                            title: qsTr("SYSTEM")
                            accent: Colors.accent
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Power"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.powerStateLabel; color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Vendor"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.amtVendor || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("AMT version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.amtVersion || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Intel ME version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.meVersionString.length > 0 ? ("v" + controller.meVersionString) : qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Activation"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.provisioningModeLabel; color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Power source"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.powerSourceLabel; color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("WSMAN protocol"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.amtProtocolVersion || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Element name"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.systemElementName || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("UUID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.systemUuid || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; elide: Text.ElideMiddle }
                            }
                        }

                        // --- AMT INFO (firmware/SKU fingerprint, #174) ---
                        // Surfaces the secondary CIM_SoftwareIdentity
                        // rows so operators can tell ISM-vs-AMT and an
                        // old build apart at a glance. Pairs with the
                        // Boot Capabilities pane in Hardware to answer
                        // "why doesn't action X work."
                        Section {
                            id: amtInfoSection
                            title: qsTr("AMT INFO")
                            visible: {
                                const fp = controller.amtFingerprint;
                                if (!fp) return false;
                                return (fp.sku || fp.buildNumber || fp.recoveryVersion
                                        || fp.vendorId || fp.flash || fp.skuLabel || "").length > 0;
                            }
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("SKU"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text {
                                    text: controller.amtFingerprint.skuLabel
                                          || controller.amtFingerprint.sku
                                          || qsTr("(unknown)")
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                Text { text: qsTr("Vendor ID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text {
                                    text: controller.amtFingerprint.vendorId || qsTr("(unknown)")
                                    color: Colors.text
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                }

                                Text { text: qsTr("Main FW build"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text {
                                    text: controller.amtFingerprint.buildNumber || qsTr("(unknown)")
                                    color: Colors.text
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                }

                                Text { text: qsTr("Recovery FW"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text {
                                    text: controller.amtFingerprint.recoveryVersion || qsTr("(unknown)")
                                    color: Colors.text
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                }

                                Text { text: qsTr("Flash"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text {
                                    text: controller.amtFingerprint.flash || qsTr("(unknown)")
                                    color: Colors.text
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        Section {
                            title: qsTr("ACTIVE FEATURES")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            // Repeater + chip delegate keeps the four
                            // chips declarative — each binds straight to
                            // the controller, no Loader/Connections juggle.
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6

                                Repeater {
                                    model: [
                                        { label: qsTr("Redirection port"),
                                          active: controller.redirectionListenerEnabled,
                                          available: true },
                                        { label: qsTr("SOL"),
                                          active: controller.solEnabled,
                                          available: true },
                                        { label: qsTr("IDE-R"),
                                          active: controller.iderEnabled,
                                          available: true },
                                        { label: qsTr("KVM"),
                                          active: controller.kvmEnabled,
                                          available: controller.kvmAvailable },
                                    ]
                                    delegate: Rectangle {
                                        required property var modelData
                                        radius: 4
                                        implicitHeight: chipText.implicitHeight + 6
                                        implicitWidth: chipText.implicitWidth + 14
                                        color: modelData.active
                                            ? Colors.accentSoft
                                            : (modelData.available ? Colors.borderMuted
                                                                    : "transparent")
                                        border.width: modelData.available ? 0 : 1
                                        border.color: Colors.borderMuted
                                        opacity: modelData.available ? 1.0 : 0.55
                                        Text {
                                            id: chipText
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: modelData.active
                                                ? Colors.text : Colors.textMuted
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.weight: Font.Medium
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("HOSTNAME / DOMAIN")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Hostname"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hostName || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Domain"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.domainName || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Realm"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.digestRealm || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }

                                Text { text: qsTr("Network if enabled"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.networkInterfaceEnabled ? qsTr("Yes") : qsTr("No"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("RMCP ping"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.rmcpPingResponseEnabled ? qsTr("Enabled") : qsTr("Disabled"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                            }
                        }
                    }
                }

                // 1 — Hardware
                Flickable {
                    contentWidth: width
                    contentHeight: hardwareCol.implicitHeight + 48
                    clip: true

                    ColumnLayout {
                        id: hardwareCol
                        spacing: 18
                        width: parent.width

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("HARDWARE")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: {
                                    const inv = controller.hardwareInventory;
                                    if (!inv || Object.keys(inv).length === 0)
                                        return qsTr("Not yet fetched");
                                    return (inv.platformManufacturer || "") + " "
                                        + (inv.platformModel || "");
                                }
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: 20
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // Empty-state placeholder until the user hits
                        // Refresh / lands on the section for the first time.
                        Text {
                            visible: Object.keys(controller.hardwareInventory).length === 0
                                && !controller.busy
                            text: qsTr("Click Refresh to fetch hardware inventory.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.leftMargin: 24
                        }

                        // --- Platform ---------------------------------
                        Section {
                            title: qsTr("PLATFORM")
                            visible: (controller.hardwareInventory.platformModel || "").length > 0
                                || (controller.hardwareInventory.platformManufacturer || "").length > 0
                            accent: Colors.accent
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Model"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.platformModel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Manufacturer"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.platformManufacturer || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.platformVersion || qsTr("(none)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Serial number"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.platformSerialNumber || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("System ID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.platformSystemId || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; elide: Text.ElideMiddle }
                            }
                        }

                        // --- Baseboard --------------------------------
                        Section {
                            title: qsTr("BASEBOARD")
                            visible: (controller.hardwareInventory.baseboardManufacturer || "").length > 0
                                || (controller.hardwareInventory.baseboardModel || "").length > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Manufacturer"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.baseboardManufacturer || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Product"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.baseboardModel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.baseboardVersion || qsTr("(none)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Serial number"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.baseboardSerialNumber || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Asset tag"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.baseboardAssetTag || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Replaceable"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS; visible: controller.hardwareInventory.baseboardCanBeFRUedKnown === true }
                                Text { text: controller.hardwareInventory.baseboardReplaceable ? qsTr("Yes") : qsTr("No"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true; visible: controller.hardwareInventory.baseboardCanBeFRUedKnown === true }
                            }
                        }

                        // --- BIOS -------------------------------------
                        Section {
                            title: qsTr("BIOS")
                            visible: (controller.hardwareInventory.biosVendor || "").length > 0
                                || (controller.hardwareInventory.biosVersion || "").length > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Vendor"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.biosVendor || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.biosVersion || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Release date"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.biosReleaseDate || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                            }
                        }

                        // --- Processors -------------------------------
                        Repeater {
                            model: controller.hardwareInventory.processors || []
                            delegate: Section {
                                id: cpuSection
                                required property var modelData
                                required property int index
                                title: qsTr("PROCESSOR %1").arg(cpuSection.index + 1)
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 16
                                    rowSpacing: 6
                                    Layout.fillWidth: true

                                    Text { text: qsTr("Manufacturer"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: cpuSection.modelData.manufacturer || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Family"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: cpuSection.modelData.familyLabel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                    Text { text: qsTr("Version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: cpuSection.modelData.version || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Max socket speed"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: cpuSection.modelData.maxClockSpeedMhz > 0
                                        ? qsTr("%1 MHz").arg(cpuSection.modelData.maxClockSpeedMhz)
                                        : qsTr("(unknown)")
                                        color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Status"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: cpuSection.modelData.cpuStatusLabel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                }
                            }
                        }

                        // --- Memory -----------------------------------
                        Repeater {
                            model: controller.hardwareInventory.memoryModules || []
                            delegate: Section {
                                id: memSection
                                required property var modelData
                                required property int index
                                title: qsTr("MEMORY MODULE %1").arg(memSection.index + 1)
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 16
                                    rowSpacing: 6
                                    Layout.fillWidth: true

                                    Text { text: qsTr("Bank label"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.bankLabel || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Size"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.capacityBytes > 0
                                        ? qsTr("%1 MB").arg(Math.round(memSection.modelData.capacityBytes / (1024 * 1024)))
                                        : qsTr("(unknown)")
                                        color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Type"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.memoryTypeLabel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Form factor"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.formFactorLabel || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Manufacturer"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.manufacturer || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Part number"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.partNumber || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Serial number"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: memSection.modelData.serialNumber || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                }
                            }
                        }

                        // --- Storage ----------------------------------
                        Repeater {
                            model: controller.hardwareInventory.storageDevices || []
                            delegate: Section {
                                id: storageSection
                                required property var modelData
                                required property int index
                                title: qsTr("STORAGE %1").arg(storageSection.index + 1)
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 16
                                    rowSpacing: 6
                                    Layout.fillWidth: true

                                    Text { text: qsTr("Model"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: storageSection.modelData.model || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Serial number"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    Text { text: storageSection.modelData.serialNumber || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                    Text { text: qsTr("Capacity"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                    // Schema MaxMediaSize is in units of 1000 bytes (kB);
                                    // convert to MB.
                                    Text { text: storageSection.modelData.maxMediaSizeKb > 0
                                        ? qsTr("%1 MB").arg(Math.round(storageSection.modelData.maxMediaSizeKb * 1000 / (1024 * 1024)))
                                        : qsTr("(unknown)")
                                        color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                }
                            }
                        }

                        // --- Battery ----------------------------------
                        Section {
                            title: qsTr("BATTERY")
                            visible: (controller.hardwareInventory.battery
                                       && controller.hardwareInventory.battery.present) === true
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Device"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: (controller.hardwareInventory.battery
                                              && controller.hardwareInventory.battery.deviceId)
                                              || qsTr("(unknown)")
                                       color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Chemistry"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: (controller.hardwareInventory.battery
                                              && controller.hardwareInventory.battery.chemistryLabel)
                                              || qsTr("(unknown)")
                                       color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Design capacity"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.battery
                                              && controller.hardwareInventory.battery.designCapacityMwh > 0
                                              ? qsTr("%1 mWh").arg(controller.hardwareInventory.battery.designCapacityMwh)
                                              : qsTr("(unknown)")
                                       color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Design voltage"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.hardwareInventory.battery
                                              && controller.hardwareInventory.battery.designVoltageMv > 0
                                              ? qsTr("%1 mV").arg(controller.hardwareInventory.battery.designVoltageMv)
                                              : qsTr("(unknown)")
                                       color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Manufacturer"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: (controller.hardwareInventory.battery
                                              && controller.hardwareInventory.battery.manufacturer)
                                              || qsTr("(unknown)")
                                       color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                                Text { text: qsTr("Serial number"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: (controller.hardwareInventory.battery
                                              && controller.hardwareInventory.battery.serialNumber)
                                              || qsTr("(unknown)")
                                       color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                            }
                        }

                        // --- Boot capabilities (read-only, #172) ------
                        // Surface AMT_BootCapabilities so the operator
                        // can tell SKU-vs-user-error apart at a glance.
                        // Power-menu gating already consults a subset of
                        // these flags via the cap* booleans on the
                        // controller; this pane shows the full picture.
                        Section {
                            id: bootCapsSection
                            title: qsTr("BOOT CAPABILITIES")
                            visible: Object.keys(controller.bootCapabilities).length > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24

                            // Stable ordering and friendly labels for
                            // the AMT_BootCapabilities flags. Keys map
                            // 1:1 to keys in `controller.bootCapabilities`.
                            readonly property var capEntries: [
                                { key: "IDER",                   label: qsTr("IDE-R (storage redirection)") },
                                { key: "SOL",                    label: qsTr("Serial-over-LAN") },
                                { key: "ForcePXEBoot",           label: qsTr("Force PXE boot") },
                                { key: "ForceHDDBoot",           label: qsTr("Force HDD boot") },
                                { key: "ForceCDorDVDBoot",       label: qsTr("Force CD/DVD boot") },
                                { key: "ForceWinREBoot",         label: qsTr("Force WinRE boot") },
                                { key: "ForceUEFILocalPBABoot",  label: qsTr("Force UEFI local PBA boot") },
                                { key: "ForceUEFIHTTPSBoot",     label: qsTr("Force UEFI HTTPS boot") },
                                { key: "BIOSSetup",              label: qsTr("Enter BIOS setup") },
                                { key: "BIOSPause",              label: qsTr("Pause BIOS POST") },
                                { key: "BIOSReflash",            label: qsTr("BIOS reflash") },
                                { key: "BIOSSecureBoot",         label: qsTr("BIOS Secure Boot control") },
                                { key: "AMTSecureBootControl",   label: qsTr("AMT Secure Boot override") },
                                { key: "SecureErase",            label: qsTr("Secure Erase") },
                                { key: "PlatformErase",          label: qsTr("Platform Erase") },
                                { key: "ConfigurationDataReset", label: qsTr("Configuration data reset") },
                                { key: "UserPasswordBypass",     label: qsTr("User-password bypass") },
                                { key: "ForcedProgressEvents",   label: qsTr("Forced progress events") },
                                { key: "VerbosityScreenBlank",   label: qsTr("Screen-blank verbosity") },
                                { key: "VerbosityVerbose",       label: qsTr("Verbose verbosity") },
                                { key: "VerbosityQuiet",         label: qsTr("Quiet verbosity") },
                                { key: "PowerButtonLock",        label: qsTr("Power button lock") },
                                { key: "ResetButtonLock",        label: qsTr("Reset button lock") },
                                { key: "KeyboardLock",           label: qsTr("Keyboard lock") },
                                { key: "SleepButtonLock",        label: qsTr("Sleep button lock") },
                            ]

                            // Decode of the PlatformErase sub-bitmask.
                            // Order and bit indices come from the
                            // BootCapabilitiesResult struct.
                            readonly property var platformEraseBits: [
                                { bit: 1,  label: qsTr("Pyrite revert") },
                                { bit: 2,  label: qsTr("Secure erase all SSDs") },
                                { bit: 6,  label: qsTr("TPM clear") },
                                { bit: 16, label: qsTr("OEM custom action") },
                                { bit: 25, label: qsTr("Clear BIOS NVM variables") },
                                { bit: 26, label: qsTr("BIOS reload of golden configuration") },
                                { bit: 31, label: qsTr("CSME unconfigure") },
                            ]

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Repeater {
                                    model: bootCapsSection.capEntries

                                    delegate: RowLayout {
                                        id: capRow
                                        required property var modelData
                                        spacing: 8
                                        Layout.fillWidth: true
                                        Layout.columnSpan: 2

                                        readonly property bool supported:
                                            controller.bootCapabilities[capRow.modelData.key] === true

                                        Rectangle {
                                            implicitWidth: bootCapChipText.implicitWidth + 16
                                            implicitHeight: bootCapChipText.implicitHeight + 6
                                            radius: 4
                                            color: capRow.supported
                                                ? Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.18)
                                                : Qt.rgba(Colors.textFaint.r, Colors.textFaint.g, Colors.textFaint.b, 0.10)
                                            border.color: capRow.supported
                                                ? Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.40)
                                                : Colors.borderMuted
                                            border.width: 1

                                            Text {
                                                id: bootCapChipText
                                                anchors.centerIn: parent
                                                text: capRow.supported ? qsTr("Yes") : qsTr("No")
                                                color: capRow.supported ? Colors.accent : Colors.textFaint
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                                font.weight: Font.Medium
                                                font.letterSpacing: 1
                                            }
                                        }

                                        Text {
                                            text: capRow.modelData.label
                                            color: capRow.supported ? Colors.text : Colors.textFaint
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeS
                                            font.strikeout: !capRow.supported
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }

                            // Platform-erase sub-bitmask. Only shown
                            // when Platform Erase is supported — the
                            // bits are meaningless on firmware that
                            // doesn't advertise the action at all.
                            ColumnLayout {
                                visible: controller.bootCapabilities.PlatformErase === true
                                spacing: 4
                                Layout.fillWidth: true
                                Layout.topMargin: 8

                                Text {
                                    text: qsTr("PLATFORM ERASE SUB-ACTIONS")
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.letterSpacing: 2
                                    font.weight: Font.Medium
                                }

                                Repeater {
                                    model: bootCapsSection.platformEraseBits

                                    delegate: RowLayout {
                                        id: peRow
                                        required property var modelData
                                        spacing: 8
                                        Layout.fillWidth: true

                                        readonly property bool supported:
                                            ((controller.bootCapabilities.PlatformEraseMask || 0)
                                             & (1 << peRow.modelData.bit)) !== 0

                                        Rectangle {
                                            implicitWidth: 10
                                            implicitHeight: 10
                                            radius: 5
                                            color: peRow.supported ? Colors.accent : Colors.borderMuted
                                        }
                                        Text {
                                            text: peRow.modelData.label
                                            color: peRow.supported ? Colors.text : Colors.textFaint
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeS
                                            font.strikeout: !peRow.supported
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 2 — Power
                ColumnLayout {
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
                                onClicked: controller.powerReset()
                            }
                            Button {
                                text: qsTr("Reset (graceful)")
                                enabled: !controller.busy
                                onClicked: controller.powerResetGraceful()
                            }
                            Button {
                                text: qsTr("Power off (soft)")
                                enabled: !controller.busy
                                onClicked: controller.powerOffSoft()
                            }
                            Button {
                                text: qsTr("Power off (hard)")
                                enabled: !controller.busy
                                onClicked: controller.powerOffHard()
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

                                Menu {
                                    id: bootMenu
                                    MenuItem { text: qsTr("Power on to BIOS Setup"); onTriggered: controller.bootToBios(false) }
                                    MenuItem { text: qsTr("Reset to BIOS Setup");    onTriggered: controller.bootToBios(true) }
                                    MenuSeparator {}
                                    MenuItem { text: qsTr("Power on to PXE");        onTriggered: controller.bootToPxe(false) }
                                    MenuItem { text: qsTr("Reset to PXE");           onTriggered: controller.bootToPxe(true) }
                                    MenuSeparator {}
                                    MenuItem { text: qsTr("Power on to IDE-R CDROM"); onTriggered: controller.bootToIderCdrom(false) }
                                    MenuItem { text: qsTr("Reset to IDE-R CDROM");    onTriggered: controller.bootToIderCdrom(true) }
                                    MenuItem { text: qsTr("Power on to IDE-R Floppy"); onTriggered: controller.bootToIderFloppy(false) }
                                    MenuItem { text: qsTr("Reset to IDE-R Floppy");    onTriggered: controller.bootToIderFloppy(true) }

                                    MenuSeparator { visible: controller.capSecureErase; height: visible ? implicitHeight : 0 }
                                    MenuItem {
                                        visible: controller.capSecureErase
                                        height: visible ? implicitHeight : 0
                                        text: qsTr("Power on to Secure Erase…")
                                        onTriggered: secureErasePrompt.openFor(false)
                                    }
                                    MenuItem {
                                        visible: controller.capSecureErase
                                        height: visible ? implicitHeight : 0
                                        text: qsTr("Reset to Secure Erase…")
                                        onTriggered: secureErasePrompt.openFor(true)
                                    }

                                    MenuSeparator { visible: controller.capPlatformErase; height: visible ? implicitHeight : 0 }
                                    MenuItem {
                                        visible: controller.capPlatformErase
                                        height: visible ? implicitHeight : 0
                                        text: qsTr("Power on to Platform Erase…")
                                        onTriggered: platformErasePrompt.openFor(false)
                                    }
                                    MenuItem {
                                        visible: controller.capPlatformErase
                                        height: visible ? implicitHeight : 0
                                        text: qsTr("Reset to Platform Erase…")
                                        onTriggered: platformErasePrompt.openFor(true)
                                    }

                                    MenuSeparator { visible: controller.capForceUefiHttpsBoot; height: visible ? implicitHeight : 0 }
                                    MenuItem {
                                        visible: controller.capForceUefiHttpsBoot
                                        height: visible ? implicitHeight : 0
                                        text: qsTr("Power on to HTTPS URL…")
                                        onTriggered: httpsBootPrompt.openFor(false)
                                    }
                                    MenuItem {
                                        visible: controller.capForceUefiHttpsBoot
                                        height: visible ? implicitHeight : 0
                                        text: qsTr("Reset to HTTPS URL…")
                                        onTriggered: httpsBootPrompt.openFor(true)
                                    }
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

                // 3 — Network
                Flickable {
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

                // 4 — Wireless (WiFi + 802.1x)
                Flickable {
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
                                    onClicked: controller.setWifiPortEnabled(!isOn)
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

                // 5 — Time
                ColumnLayout {
                    spacing: 18

                    // Time data is fetched centrally via
                    // `root.refreshCurrent()` when this section becomes
                    // active.

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Text {
                            text: qsTr("TIME")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }
                        Text {
                            text: controller.amtEpoch === 0
                                ? qsTr("Not yet fetched")
                                : Qt.formatDateTime(new Date(controller.amtEpoch * 1000),
                                                     "yyyy-MM-dd  HH:mm:ss  t")
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: 20
                        }
                    }

                    Section {
                        title: qsTr("DETAILS")
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        GridLayout {
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 6
                            Layout.fillWidth: true

                            Text { text: qsTr("Unix epoch"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                            Text { text: String(controller.amtEpoch); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                            Text { text: qsTr("Local skew"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                            Text {
                                text: controller.amtEpoch === 0
                                    ? qsTr("—")
                                    : qsTr("%1 s")
                                        .arg(Math.round(controller.amtEpoch -
                                              (new Date().getTime() / 1000)))
                                color: Colors.text
                                font.family: Type.mono
                                font.pixelSize: Type.sizeS
                                Layout.fillWidth: true
                            }
                        }
                    }

                    RowLayout {
                        Layout.leftMargin: 24
                        spacing: 8

                        FlatButton {
                            text: qsTr("Refresh")
                            enabled: !controller.busy
                            onClicked: controller.refreshTime()
                        }
                        AccentButton {
                            text: qsTr("Sync now")
                            // No live skew until the first read — disable
                            // the action so the operator doesn't push
                            // garbage (host_now ≈ 0 device epoch) at the
                            // firmware.
                            enabled: !controller.busy && controller.amtEpoch !== 0
                            onClicked: controller.syncDeviceTime()
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // 6 — Remote access (SOL / KVM / IDE-R launchers)
                ColumnLayout {
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
                                    onClicked: controller.setKvmOptInPolicyEnabled(!controller.kvmOptInPolicy)
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

                // 7 — Remote access (CIRA)
                Flickable {
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
                                         && controller.remoteAccess.policies
                                         && controller.remoteAccess.policies.length > 0)
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
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
                                            font.pixelSize: 9
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

                // 8 — Certificates (locally pinned)
                ColumnLayout {
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

                // 9 — Device certificate store
                Flickable {
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
                                                    font.pixelSize: 9
                                                    font.weight: Font.Medium
                                                    font.letterSpacing: 1
                                                }
                                                Text {
                                                    visible: modelData.hasPrivateKey === true
                                                    text: qsTr("KEY")
                                                    color: Colors.standby
                                                    font.family: Type.sans
                                                    font.pixelSize: 9
                                                    font.weight: Font.Medium
                                                    font.letterSpacing: 1
                                                }
                                                Text {
                                                    visible: modelData.active === true
                                                    text: qsTr("ACTIVE")
                                                    color: Colors.text
                                                    font.family: Type.sans
                                                    font.pixelSize: 9
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

                // 10 — Event log
                ColumnLayout {
                    spacing: 8

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        Text {
                            text: qsTr("EVENT LOG")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }
                        Text {
                            text: controller.eventLog.length === 0
                                ? qsTr("No entries.")
                                : qsTr("%1 entries").arg(controller.eventLog.length)
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: 20
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: 24
                        clip: true
                        model: controller.eventLog
                        ScrollBar.vertical: ScrollBar {}

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            implicitHeight: 36
                            color: index % 2 === 0 ? "transparent" : Colors.elevated
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 12

                                Text {
                                    text: modelData.severity || "—"
                                    color: {
                                        // CIM Severity: 0/1 OK, 2 Degraded, 3 Minor,
                                        // 4 Major, 5 Critical, 6 Fatal. Bucket into
                                        // our three on/standby/error colours.
                                        const s = parseInt(modelData.severity);
                                        if (s >= 5) return Colors.error;
                                        if (s >= 3) return Colors.standby;
                                        return Colors.textMuted;
                                    }
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    Layout.preferredWidth: 32
                                }
                                Text {
                                    text: modelData.timestamp || "—"
                                    color: Colors.textMuted
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    Layout.preferredWidth: 160
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.message || ""
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Text {
                            visible: controller.eventLog.length === 0 && !controller.busy
                            anchors.centerIn: parent
                            text: qsTr("No event log entries.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                    }
                }

                // 11 — Audit log
                ColumnLayout {
                    spacing: 8

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        Text {
                            text: qsTr("AUDIT LOG")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }
                        Text {
                            text: {
                                const s = controller.auditLogState;
                                if (!s || !s.ok)
                                    return controller.auditLogEntries.length === 0
                                        ? qsTr("Not yet fetched")
                                        : qsTr("%1 entries").arg(controller.auditLogEntries.length);
                                let parts = [];
                                parts.push(s.enabled ? qsTr("Enabled") : qsTr("Disabled"));
                                if (s.locked)     parts.push(qsTr("Locked"));
                                if (s.full)       parts.push(qsTr("Full"));
                                else if (s.almostFull) parts.push(qsTr("Almost full"));
                                if (s.noSigningKey) parts.push(qsTr("No signing key"));
                                return parts.join(" · ");
                            }
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: 20
                        }
                        Text {
                            visible: controller.auditLogState
                                  && controller.auditLogState.ok === true
                            text: qsTr("%1 records · %2%% free")
                                .arg(controller.auditLogState.currentNumberOfRecords || 0)
                                .arg(controller.auditLogState.percentageFree || 0)
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                        Text {
                            visible: controller.auditLogEntries.length === 0
                                && !controller.busy
                            text: qsTr("Click Refresh to fetch the audit log.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: 24
                        clip: true
                        model: controller.auditLogEntries
                        ScrollBar.vertical: ScrollBar {}

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            implicitHeight: 36
                            color: index % 2 === 0 ? "transparent" : Colors.elevated

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 12

                                Text {
                                    text: modelData.unixSeconds > 0
                                        ? Qt.formatDateTime(
                                            new Date(modelData.unixSeconds * 1000),
                                            "yyyy-MM-dd  HH:mm:ss")
                                        : "—"
                                    color: Colors.textMuted
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    Layout.preferredWidth: 160
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.initiator || "—"
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    Layout.preferredWidth: 140
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.eventLabel || "—"
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.netAddress || ""
                                    color: Colors.textFaint
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    Layout.preferredWidth: 110
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                // 12 — User accounts
                ColumnLayout {
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
                                        font.pixelSize: 9
                                        font.weight: Font.Medium
                                        font.letterSpacing: 1
                                    }
                                    Text {
                                        visible: modelData.isKerberos === true
                                        text: qsTr("KERBEROS")
                                        color: Colors.textMuted
                                        font.family: Type.sans
                                        font.pixelSize: 9
                                        font.weight: Font.Medium
                                        font.letterSpacing: 1
                                    }
                                    Text {
                                        visible: !modelData.enabled
                                        text: qsTr("DISABLED")
                                        color: Colors.standby
                                        font.family: Type.sans
                                        font.pixelSize: 9
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

                // 13 — Watchdogs (agent presence)
                ColumnLayout {
                    spacing: 8

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        Text {
                            text: qsTr("WATCHDOGS")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }
                        Text {
                            text: {
                                const ap = controller.agentPresence || {};
                                const w = ap.watchdogs || [];
                                if (Object.keys(ap).length === 0)
                                    return qsTr("Not yet fetched");
                                if (w.length === 0)
                                    return qsTr("No agent presence watchdog configured.");
                                return qsTr("%1 watchdog%2 configured")
                                    .arg(w.length)
                                    .arg(w.length === 1 ? "" : "s");
                            }
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: 20
                        }
                        Text {
                            visible: (controller.agentPresence
                                       && controller.agentPresence.maxTotalAgents > 0)
                            text: qsTr("Firmware ceiling: %1 watchdogs, %2 actions total.")
                                .arg(controller.agentPresence.maxTotalAgents)
                                .arg(controller.agentPresence.maxTotalActions)
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                        Text {
                            text: qsTr("Read-only — add / edit / delete arrives in Phase B.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: 24
                        clip: true
                        model: (controller.agentPresence
                                 && controller.agentPresence.watchdogs) || []
                        ScrollBar.vertical: ScrollBar {}
                        spacing: 6

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            implicitHeight: watchdogCol.implicitHeight + 16
                            color: Colors.surface
                            border.color: Colors.borderMuted
                            border.width: 1
                            radius: 8

                            ColumnLayout {
                                id: watchdogCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                anchors.topMargin: 8
                                spacing: 4

                                RowLayout {
                                    spacing: 10
                                    Layout.fillWidth: true

                                    Text {
                                        text: parent.parent.modelData.description
                                              && parent.parent.modelData.description.length > 0
                                            ? parent.parent.modelData.description
                                            : parent.parent.modelData.deviceIdGuid
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeM
                                        font.weight: Font.Medium
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }

                                    Rectangle {
                                        readonly property bool running:
                                            parent.parent.parent.modelData.currentState === 4
                                        implicitWidth: stateChip.implicitWidth + 14
                                        implicitHeight: stateChip.implicitHeight + 6
                                        radius: 4
                                        color: running
                                            ? Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.18)
                                            : Qt.rgba(Colors.textFaint.r, Colors.textFaint.g, Colors.textFaint.b, 0.10)
                                        border.color: running
                                            ? Qt.rgba(Colors.accent.r, Colors.accent.g, Colors.accent.b, 0.40)
                                            : Colors.borderMuted
                                        border.width: 1

                                        Text {
                                            id: stateChip
                                            anchors.centerIn: parent
                                            text: parent.parent.parent.parent.modelData.currentStateLabel
                                            color: parent.running ? Colors.accent : Colors.textFaint
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.weight: Font.Medium
                                            font.letterSpacing: 1
                                        }
                                    }
                                }

                                GridLayout {
                                    columns: 4
                                    columnSpacing: 16
                                    rowSpacing: 2
                                    Layout.fillWidth: true
                                    Layout.topMargin: 4

                                    Text { text: qsTr("Monitored"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                                    Text {
                                        text: watchdogCol.parent.modelData.monitoredEntityLabel
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        Layout.fillWidth: true
                                    }
                                    Text { text: qsTr("Enabled"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                                    Text {
                                        text: watchdogCol.parent.modelData.enabledStateLabel
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        Layout.fillWidth: true
                                    }

                                    Text { text: qsTr("Startup"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                                    Text {
                                        text: qsTr("%1 s").arg(watchdogCol.parent.modelData.startupIntervalSec)
                                        color: Colors.text
                                        font.family: Type.mono
                                        font.pixelSize: Type.sizeXs
                                        Layout.fillWidth: true
                                    }
                                    Text { text: qsTr("Timeout"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                                    Text {
                                        text: qsTr("%1 s").arg(watchdogCol.parent.modelData.timeoutIntervalSec)
                                        color: Colors.text
                                        font.family: Type.mono
                                        font.pixelSize: Type.sizeXs
                                        Layout.fillWidth: true
                                    }
                                }

                                Text {
                                    text: watchdogCol.parent.modelData.deviceIdGuid
                                    color: Colors.textFaint
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                }

                // 14 — Event subscriptions
                Flickable {
                    contentWidth: width
                    contentHeight: subsCol.implicitHeight + 48
                    clip: true

                    ColumnLayout {
                        id: subsCol
                        spacing: 18
                        width: parent.width

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("EVENT SUBSCRIPTIONS")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: {
                                    const es = controller.eventSubscriptions || {};
                                    if (Object.keys(es).length === 0)
                                        return qsTr("Not yet fetched");
                                    const s = es.subscriptions || [];
                                    return s.length === 0
                                        ? qsTr("No subscriptions configured")
                                        : qsTr("%1 subscription%2")
                                              .arg(s.length)
                                              .arg(s.length === 1 ? "" : "s");
                                }
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: 20
                            }
                            Text {
                                text: qsTr("Read-only — Subscribe / UnSubscribe arrives in Phase B.")
                                color: Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                            }
                        }

                        Section {
                            title: qsTr("ACTIVE SUBSCRIPTIONS")
                            visible: !!(controller.eventSubscriptions
                                  && controller.eventSubscriptions.subscriptions
                                  && controller.eventSubscriptions.subscriptions.length > 0)
                            accent: Colors.accent
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                spacing: 6
                                Layout.fillWidth: true

                                Repeater {
                                    model: (controller.eventSubscriptions
                                             && controller.eventSubscriptions.subscriptions) || []

                                    delegate: ColumnLayout {
                                        id: subRow
                                        required property var modelData
                                        spacing: 2
                                        Layout.fillWidth: true

                                        RowLayout {
                                            spacing: 10
                                            Layout.fillWidth: true

                                            Text {
                                                text: subRow.modelData.filterInstanceId || qsTr("(unnamed filter)")
                                                color: Colors.text
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeS
                                                font.weight: Font.Medium
                                                Layout.fillWidth: true
                                                elide: Text.ElideMiddle
                                            }
                                            Text {
                                                text: subRow.modelData.deliveryModeLabel || ""
                                                color: Colors.textMuted
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeXs
                                            }
                                        }
                                        Text {
                                            text: subRow.modelData.destination
                                                || subRow.modelData.listenerName
                                            color: Colors.textMuted
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("LISTENER DESTINATIONS")
                            visible: !!(controller.eventSubscriptions
                                  && controller.eventSubscriptions.listeners
                                  && controller.eventSubscriptions.listeners.length > 0)
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true

                                Repeater {
                                    model: (controller.eventSubscriptions
                                             && controller.eventSubscriptions.listeners) || []

                                    delegate: ColumnLayout {
                                        id: listenerRow
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            text: listenerRow.modelData.name
                                                  + "  ·  "
                                                  + listenerRow.modelData.deliveryModeLabel
                                            color: Colors.text
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeS
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: listenerRow.modelData.destination
                                            color: Colors.textMuted
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("AVAILABLE FILTERS")
                            visible: !!(controller.eventSubscriptions
                                  && controller.eventSubscriptions.filters
                                  && controller.eventSubscriptions.filters.length > 0)
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24

                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true

                                Repeater {
                                    model: (controller.eventSubscriptions
                                             && controller.eventSubscriptions.filters) || []

                                    delegate: Text {
                                        required property var modelData
                                        text: modelData.collectionName
                                              || modelData.instanceId
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeS
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }
                        }
                    }
                }

                // 15 — Wake alarms
                ColumnLayout {
                    spacing: 8

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        Text {
                            text: qsTr("WAKE ALARMS")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }
                        Text {
                            text: controller.wakeAlarms.length === 0
                                ? qsTr("No wake alarms registered.")
                                : qsTr("%1 alarm%2 scheduled")
                                      .arg(controller.wakeAlarms.length)
                                      .arg(controller.wakeAlarms.length === 1 ? "" : "s")
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: 20
                        }
                        Text {
                            text: qsTr("Read-only — Add / Edit / Delete arrives in Phase B.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: 24
                        clip: true
                        model: controller.wakeAlarms
                        ScrollBar.vertical: ScrollBar {}
                        spacing: 6

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            implicitHeight: alarmCol.implicitHeight + 16
                            color: Colors.surface
                            border.color: Colors.borderMuted
                            border.width: 1
                            radius: 8

                            ColumnLayout {
                                id: alarmCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                anchors.topMargin: 8
                                spacing: 2

                                RowLayout {
                                    spacing: 10
                                    Layout.fillWidth: true

                                    Text {
                                        text: alarmCol.parent.modelData.elementName
                                              || alarmCol.parent.modelData.instanceId
                                              || qsTr("(unnamed)")
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeM
                                        font.weight: Font.Medium
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }

                                    Rectangle {
                                        visible: alarmCol.parent.modelData.deleteOnCompletion
                                        implicitWidth: dotcText.implicitWidth + 12
                                        implicitHeight: dotcText.implicitHeight + 6
                                        radius: 4
                                        color: Qt.rgba(Colors.textFaint.r, Colors.textFaint.g, Colors.textFaint.b, 0.10)
                                        border.color: Colors.borderMuted
                                        border.width: 1

                                        Text {
                                            id: dotcText
                                            anchors.centerIn: parent
                                            text: qsTr("Delete after fire")
                                            color: Colors.textMuted
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.letterSpacing: 1
                                        }
                                    }
                                }

                                Text {
                                    text: qsTr("Wakes at %1").arg(alarmCol.parent.modelData.startTimeLocal)
                                    color: Colors.textMuted
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                }
                                Text {
                                    visible: (alarmCol.parent.modelData.intervalLabel || "").length > 0
                                    text: qsTr("and recurs every %1").arg(alarmCol.parent.modelData.intervalLabel)
                                    color: Colors.textMuted
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                // 16 — WSMAN browser (dev tool, #167)
                ColumnLayout {
                    id: wsmanPane
                    spacing: 8

                    // Pretty-print the raw response so the operator sees
                    // indented XML rather than a single line. The QtCore
                    // QXmlStreamReader/Writer combo handles namespace
                    // declarations correctly.
                    function prettyPrint(s) {
                        if (!s || s.length === 0) return "";
                        // Don't try to format an empty / non-XML payload.
                        if (s.indexOf("<") < 0) return s;
                        const reader = Qt.createQmlObject(
                            'import QtQuick; QtObject {}', wsmanPane);
                        // Use the built-in DOMParser equivalent — QML
                        // doesn't expose QXmlStreamReader, so a simple
                        // bracket-aware reflow is "good enough" for the
                        // dev-tool use case. Insert a newline after each
                        // `>` and indent based on nesting depth.
                        let out = "";
                        let depth = 0;
                        let i = 0;
                        while (i < s.length) {
                            if (s[i] === "<") {
                                const end = s.indexOf(">", i);
                                if (end < 0) { out += s.substring(i); break; }
                                const tag = s.substring(i, end + 1);
                                const isClose = tag.startsWith("</");
                                const isSelf  = tag.endsWith("/>") || tag.startsWith("<?");
                                if (isClose && depth > 0) depth--;
                                out += "  ".repeat(depth) + tag + "\n";
                                if (!isClose && !isSelf) depth++;
                                i = end + 1;
                            } else {
                                const next = s.indexOf("<", i);
                                const text = (next < 0 ? s.substring(i)
                                                       : s.substring(i, next)).trim();
                                if (text.length > 0) out += "  ".repeat(depth) + text + "\n";
                                i = next < 0 ? s.length : next;
                            }
                        }
                        return out;
                    }

                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        Text {
                            text: qsTr("WSMAN BROWSER")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }
                        Text {
                            text: qsTr("Developer tool — raw Get / Enumerate against any AMT WSMAN class.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                    }

                    Section {
                        title: qsTr("REQUEST")
                        accent: Colors.accent
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24

                        ColumnLayout {
                            spacing: 8
                            Layout.fillWidth: true

                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true

                                ComboBox {
                                    id: wsmanKind
                                    model: [ qsTr("Get"), qsTr("Enumerate") ]
                                    Layout.preferredWidth: 130
                                }
                                TextField {
                                    id: wsmanClass
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("e.g. AMT_GeneralSettings")
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                }
                                AccentButton {
                                    text: qsTr("Submit")
                                    enabled: !controller.busy && wsmanClass.text.trim().length > 0
                                    onClicked: {
                                        const selectors = {};
                                        const lines = (wsmanSelectors.text || "").split("\n");
                                        for (let i = 0; i < lines.length; i++) {
                                            const eq = lines[i].indexOf("=");
                                            if (eq <= 0) continue;
                                            const k = lines[i].substring(0, eq).trim();
                                            const v = lines[i].substring(eq + 1).trim();
                                            if (k.length > 0) selectors[k] = v;
                                        }
                                        controller.wsmanBrowse(
                                            wsmanClass.text.trim(),
                                            wsmanKind.currentIndex === 1 ? "enumerate" : "get",
                                            selectors);
                                    }
                                }
                            }

                            Text {
                                text: qsTr("Selectors (Get only) — one Name=Value per line")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                            }
                            TextArea {
                                id: wsmanSelectors
                                Layout.fillWidth: true
                                Layout.preferredHeight: 60
                                placeholderText: qsTr("InstanceID=Intel(r) AMT:Whatever")
                                font.family: Type.mono
                                font.pixelSize: Type.sizeS
                                wrapMode: TextArea.NoWrap
                                enabled: wsmanKind.currentIndex === 0
                            }
                        }
                    }

                    Section {
                        title: {
                            const r = controller.wsmanBrowseResult || {};
                            if (Object.keys(r).length === 0) return qsTr("RESPONSE");
                            if (!r.ok) return qsTr("RESPONSE — error");
                            if (r.kind === "enumerate")
                                return qsTr("RESPONSE — %1 item%2")
                                    .arg(r.itemCount)
                                    .arg(r.itemCount === 1 ? "" : "s");
                            return qsTr("RESPONSE");
                        }
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: 24

                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Text {
                                visible: !!(controller.wsmanBrowseResult
                                      && !controller.wsmanBrowseResult.ok
                                      && (controller.wsmanBrowseResult.error || "").length > 0)
                                text: controller.wsmanBrowseResult
                                    ? (controller.wsmanBrowseResult.error || "")
                                    : ""
                                color: Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                TextArea {
                                    readOnly: true
                                    text: wsmanPane.prettyPrint(
                                        controller.wsmanBrowseResult
                                            ? (controller.wsmanBrowseResult.xml || "")
                                            : "")
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    wrapMode: TextArea.NoWrap
                                    selectByMouse: true
                                }
                            }
                        }
                    }
                }

                // 17 — System Defense (ACM only, #165)
                Flickable {
                    contentWidth: width
                    contentHeight: sysDefCol.implicitHeight + 48
                    clip: true

                    ColumnLayout {
                        id: sysDefCol
                        spacing: 18
                        width: parent.width

                        readonly property bool isAcm:
                            controller.provisioningMode !== 4
                        readonly property bool supported:
                            controller.systemDefense
                                ? controller.systemDefense.supported !== false
                                : true

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("SYSTEM DEFENSE")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: {
                                    if (!sysDefCol.isAcm)
                                        return qsTr("ACM only — this device is provisioned in Client Control Mode.");
                                    if (!sysDefCol.supported)
                                        return qsTr("Not supported by this firmware.");
                                    const sd = controller.systemDefense || {};
                                    const n = ((sd.policies || []).length)
                                            + ((sd.hdrFilters || []).length)
                                            + ((sd.ipFilters || []).length);
                                    return n === 0
                                        ? qsTr("No policies or filters configured.")
                                        : qsTr("%1 entr%2 across policies / filters")
                                              .arg(n)
                                              .arg(n === 1 ? "y" : "ies");
                                }
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: 20
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Text {
                                visible: sysDefCol.isAcm && sysDefCol.supported
                                text: qsTr("Read-only — statistics + add/edit defer to Phase B.")
                                color: Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                            }
                        }

                        Section {
                            title: qsTr("POLICIES")
                            visible: sysDefCol.isAcm
                                  && sysDefCol.supported
                                  && controller.systemDefense
                                  && (controller.systemDefense.policies || []).length > 0
                            accent: Colors.accent
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true

                                Repeater {
                                    model: (controller.systemDefense
                                             && controller.systemDefense.policies) || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Text {
                                            text: modelData.policyName || modelData.instanceId
                                            color: Colors.text
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeS
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                        Text {
                                            text: qsTr("pri %1").arg(modelData.priority)
                                            color: Colors.textMuted
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                        }
                                        Text {
                                            visible: modelData.defaultPolicy === true
                                            text: qsTr("default")
                                            color: Colors.accent
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeXs
                                            font.weight: Font.Medium
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("L2 FILTERS (802.1Q / EtherType)")
                            visible: sysDefCol.isAcm
                                  && sysDefCol.supported
                                  && controller.systemDefense
                                  && (controller.systemDefense.hdrFilters || []).length > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true

                                Repeater {
                                    model: (controller.systemDefense
                                             && controller.systemDefense.hdrFilters) || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Text {
                                            text: modelData.name || modelData.instanceId
                                            color: Colors.text
                                            font.family: Type.sans
                                            font.pixelSize: Type.sizeS
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                        Text {
                                            text: modelData.etherType > 0
                                                ? qsTr("ether 0x%1").arg(modelData.etherType.toString(16))
                                                : ""
                                            color: Colors.textMuted
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                        }
                                        Text {
                                            text: modelData.vlanTag >= 0
                                                ? qsTr("VLAN %1").arg(modelData.vlanTag)
                                                : ""
                                            color: Colors.textMuted
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("L3/L4 FILTERS (IP / ports)")
                            visible: sysDefCol.isAcm
                                  && sysDefCol.supported
                                  && controller.systemDefense
                                  && (controller.systemDefense.ipFilters || []).length > 0
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24

                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true

                                Repeater {
                                    model: (controller.systemDefense
                                             && controller.systemDefense.ipFilters) || []
                                    delegate: ColumnLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 1

                                        RowLayout {
                                            spacing: 10
                                            Layout.fillWidth: true

                                            Text {
                                                text: modelData.name || modelData.instanceId
                                                color: Colors.text
                                                font.family: Type.sans
                                                font.pixelSize: Type.sizeS
                                                Layout.fillWidth: true
                                                elide: Text.ElideMiddle
                                            }
                                            Text {
                                                text: modelData.protocol > 0
                                                    ? qsTr("proto %1").arg(modelData.protocol)
                                                    : ""
                                                color: Colors.textMuted
                                                font.family: Type.mono
                                                font.pixelSize: Type.sizeXs
                                            }
                                            Text {
                                                text: modelData.dstPort > 0
                                                    ? qsTr("dst :%1").arg(modelData.dstPort)
                                                    : ""
                                                color: Colors.textMuted
                                                font.family: Type.mono
                                                font.pixelSize: Type.sizeXs
                                            }
                                        }
                                        Text {
                                            visible: (modelData.srcAddress || modelData.dstAddress || "").length > 0
                                            text: qsTr("%1 → %2")
                                                .arg(modelData.srcAddress || "*")
                                                .arg(modelData.dstAddress || "*")
                                            color: Colors.textMuted
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 18 — Active sessions (SOL / KVM / IDE-R, #160)
                Flickable {
                    contentWidth: width
                    contentHeight: sessionsCol.implicitHeight + 48
                    clip: true

                    ColumnLayout {
                        id: sessionsCol
                        spacing: 18
                        width: parent.width

                        ColumnLayout {
                            spacing: 4
                            Layout.fillWidth: true
                            Layout.topMargin: 24
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            Text {
                                text: qsTr("ACTIVE SESSIONS")
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                font.letterSpacing: 2
                                font.weight: Font.Medium
                            }
                            Text {
                                text: {
                                    const a = controller.activeSessions;
                                    if (!a) return qsTr("Not yet fetched");
                                    const total = ((a.sol  || []).length)
                                                 + ((a.kvm  || []).length)
                                                 + ((a.ider || []).length);
                                    return total === 0
                                        ? qsTr("No redirection sessions are active.")
                                        : qsTr("%1 active session(s)").arg(total);
                                }
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: 20
                            }
                            Text {
                                text: qsTr("Useful when a SOL / KVM / IDE-R launch is rejected: AMT allows one session per channel, so this pane shows who's already holding it.")
                                color: Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        // Reusable Section delegate — declared as an inline
                        // helper Component instead of a separate file so the
                        // tab's three lists share the same skeleton without
                        // adding a one-off QML import.
                        Component {
                            id: sessionsSectionDelegate
                            Section {
                                Layout.fillWidth: true
                                Layout.leftMargin: 24
                                Layout.rightMargin: 24
                                Layout.bottomMargin: 8
                            }
                        }

                        Section {
                            title: qsTr("SOL")
                            accent: Colors.accent
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    visible: ((controller.activeSessions && controller.activeSessions.sol) || []).length === 0
                                    text: qsTr("(no active SOL session)")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                                Repeater {
                                    model: (controller.activeSessions && controller.activeSessions.sol) || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Text {
                                            text: qsTr("%1:%2")
                                                .arg(modelData.sourceAddress || "(unknown)")
                                                .arg(modelData.sourcePort)
                                            color: Colors.text
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeS
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: modelData.sessionInstanceId || ""
                                            color: Colors.textFaint
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("KVM")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    visible: ((controller.activeSessions && controller.activeSessions.kvm) || []).length === 0
                                    text: qsTr("(no active KVM session)")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                                Repeater {
                                    model: (controller.activeSessions && controller.activeSessions.kvm) || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Text {
                                            text: qsTr("%1:%2")
                                                .arg(modelData.sourceAddress || "(unknown)")
                                                .arg(modelData.sourcePort)
                                            color: Colors.text
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeS
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: modelData.sessionInstanceId || ""
                                            color: Colors.textFaint
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }

                        Section {
                            title: qsTr("IDE-R")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24
                            Layout.bottomMargin: 24

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    visible: ((controller.activeSessions && controller.activeSessions.ider) || []).length === 0
                                    text: qsTr("(no active IDE-R session)")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                                Repeater {
                                    model: (controller.activeSessions && controller.activeSessions.ider) || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Text {
                                            text: qsTr("%1:%2")
                                                .arg(modelData.sourceAddress || "(unknown)")
                                                .arg(modelData.sourcePort)
                                            color: Colors.text
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeS
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: modelData.sessionInstanceId || ""
                                            color: Colors.textFaint
                                            font.family: Type.mono
                                            font.pixelSize: Type.sizeXs
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }
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
