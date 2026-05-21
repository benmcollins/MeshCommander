// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Network" section. Read-only view of the
/// AMT NIC configuration — IPv4/IPv6, DNS, MAC. Pulled into its
/// own file; see OverviewSection.qml for the rationale.
Flickable {
    id: root

    required property MachineDetailsController controller

    contentWidth: width
    contentHeight: networkCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: networkCol
        spacing: 18
        width: parent.width

        SectionHeader {
            eyebrow: qsTr("NETWORK")
            title: controller.networkInterfaces.length === 0
                ? qsTr("Not yet fetched")
                : qsTr("%1 interface(s)")
                    .arg(controller.networkInterfaces.length)
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
                        font.pixelSize: Type.sizeL
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
