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
        { key: "power",     label: qsTr("Power"),          icon: "⏻" },
        { key: "network",   label: qsTr("Network"),        icon: "≋" },
        { key: "time",      label: qsTr("Time"),           icon: "◷" },
        { key: "remote",    label: qsTr("Remote access"),  icon: "▶" },
        { key: "certs",     label: qsTr("Certificates"),   icon: "🔒" },
        { key: "events",    label: qsTr("Event log"),      icon: "≡" },
        { key: "users",     label: qsTr("User accounts"),  icon: "⌥" },
    ]
    property int currentSection: 0

    /// Pull whatever data the active section needs. Skipped silently
    /// while `machineHost` is still empty (the Loader populates the
    /// props *after* the window is constructed, so initial bindings
    /// fire with an empty host before `applyMachine()` runs).
    function refreshCurrent() {
        if (machineHost.length === 0) return;
        switch (currentSection) {
        case 0: controller.refreshOverview();     break;
        case 1: controller.refreshPower();        break;
        case 2: controller.refreshNetwork();      break;
        case 3: controller.refreshTime();         break;
        // 4 = Remote access — no fetch needed.
        // 5 = Certificates (local pins) — comes from the saved machine.
        case 6: controller.refreshEventLog();     break;
        case 7: controller.refreshUserAccounts(); break;
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

                // 1 — Power
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
                                text: qsTr("Refresh")
                                enabled: !controller.busy
                                onClicked: controller.refreshPower()
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // 2 — Network
                Flickable {
                    contentWidth: width
                    contentHeight: networkCol.implicitHeight + 48
                    clip: true

                    // Network data is fetched centrally via
                    // `root.refreshCurrent()` when this section becomes
                    // active.

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
                                text: controller.macAddress || qsTr("Wired interface 0")
                                color: Colors.text
                                font.family: Type.mono
                                font.pixelSize: 20
                            }
                        }

                        Section {
                            title: qsTr("IPV4")
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            Layout.rightMargin: 24

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 6
                                Layout.fillWidth: true

                                Text { text: qsTr("Addressing"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.dhcpEnabled ? qsTr("DHCP") : qsTr("Static"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("IP address"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.ipAddress || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Subnet mask"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.subnetMask || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Gateway"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.defaultGateway || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Primary DNS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.primaryDns || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Secondary DNS"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.secondaryDns || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
                            }
                        }
                    }
                }

                // 3 — Time
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

                    FlatButton {
                        text: qsTr("Refresh")
                        Layout.leftMargin: 24
                        enabled: !controller.busy
                        onClicked: controller.refreshTime()
                    }

                    Item { Layout.fillHeight: true }
                }

                // 4 — Remote access (SOL / KVM / IDE-R launchers)
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

                // 5 — Certificates (locally pinned)
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

                // 6 — Event log
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

                // 7 — User accounts
                ColumnLayout {
                    spacing: 8

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
                        Text {
                            text: controller.userAccounts.length === 0
                                ? qsTr("No accounts.")
                                : qsTr("%1 accounts").arg(controller.userAccounts.length)
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: 20
                        }
                        Text {
                            text: qsTr("Read-only — adding / editing accounts comes in a follow-up.")
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
                        model: controller.userAccounts
                        ScrollBar.vertical: ScrollBar {}

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            implicitHeight: 48
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
                                        text: modelData.elementName.length > 0
                                              ? modelData.elementName
                                              : (modelData.name || qsTr("(unnamed)"))
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeM
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
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

                                Text {
                                    text: modelData.instanceID || ""
                                    color: Colors.textFaint
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
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
