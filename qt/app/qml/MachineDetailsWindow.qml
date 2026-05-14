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
Window {
    id: root

    // Inputs from the main-window Loader.
    property int targetRow: -1
    property string machineName
    property string machineHost
    property string machineUser
    property string machinePass
    property bool   machineTls: false
    property var    machineTrustedFingerprints: []

    signal trustedFingerprintPersistRequested(string fingerprint)

    width: 1100
    height: 760
    minimumWidth: 880
    minimumHeight: 540
    title: machineName.length > 0
        ? qsTr("QuMesh — %1").arg(machineName)
        : qsTr("QuMesh — %1").arg(machineHost)
    color: Colors.bg

    MachineDetailsController {
        id: controller
        host: root.machineHost
        user: root.machineUser
        password: root.machinePass
        tls: root.machineTls
    }

    // Auto-load the overview as soon as the window is created.
    Component.onCompleted: controller.refreshOverview()

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
                        onClicked: controller.refreshOverview()
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

                                Text { text: qsTr("WSMAN protocol"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.amtProtocolVersion || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("Element name"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.systemElementName || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                                Text { text: qsTr("UUID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                                Text { text: controller.systemUuid || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; elide: Text.ElideMiddle }
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

                    Component.onCompleted: controller.refreshNetwork()

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

                    Component.onCompleted: controller.refreshTime()

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
                            Button {
                                text: qsTr("Open SOL")
                                highlighted: true
                                enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                                onClicked: solLoader.launch()
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
                            Button {
                                text: qsTr("Open KVM")
                                highlighted: true
                                enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                                onClicked: kvmLoader.launch()
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
                            Button {
                                text: qsTr("Mount ISO…")
                                highlighted: true
                                enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                                onClicked: iderLoader.launch()
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

                // 6 — Event log (stub)
                ColumnLayout {
                    spacing: 18
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
                            text: qsTr("AMT event log fetch lands in a follow-up — needs WS-Enumeration.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // 7 — Users (stub)
                ColumnLayout {
                    spacing: 18
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
                            text: qsTr("AMT user list lands in a follow-up — needs WS-Enumeration.")
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    // -- Embedded session windows ------------------------------------
    Loader {
        id: solLoader
        active: false
        asynchronous: true
        function launch() { active = false; active = true }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
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
                root.trustedFingerprintPersistRequested(fp);
            }
        }
    }

    Loader {
        id: iderLoader
        active: false
        asynchronous: true
        function launch() { active = false; active = true }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
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
                root.trustedFingerprintPersistRequested(fp);
            }
        }
    }

    Loader {
        id: kvmLoader
        active: false
        asynchronous: true
        function launch() { active = false; active = true }
        function openWindow() {
            if (item === null) return;
            item.targetHost = root.machineHost;
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
                root.trustedFingerprintPersistRequested(fp);
            }
        }
    }
}
