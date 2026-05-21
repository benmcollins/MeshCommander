// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Overview" section (was the inline section 0
/// before #325). Pulled into its own file as part of phase 2 of
/// #281 — purely organisational so each section can be reviewed
/// without scrolling past the 4 600-line god file.
Flickable {
    id: root

    required property MachineDetailsController controller
    required property string machineName
    required property string machineHost

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
                font.pixelSize: Type.sizeXl
                font.weight: Font.Medium
            }
            // The "via …" line that used to live here is now rendered
            // directly under the IP in the header card — closer to
            // what it qualifies.
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
                Text { text: root.controller.powerStateLabel; color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Vendor"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.amtVendor || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("AMT version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.amtVersion || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Intel ME version"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.meVersionString.length > 0 ? ("v" + root.controller.meVersionString) : qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Activation"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.provisioningModeLabel; color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Power source"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.powerSourceLabel; color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("WSMAN protocol"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.amtProtocolVersion || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Element name"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.systemElementName || qsTr("(unknown)"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("UUID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.systemUuid || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; elide: Text.ElideMiddle }
            }
        }

        // --- AMT INFO (firmware/SKU fingerprint, #174) ---
        // Surfaces the secondary CIM_SoftwareIdentity rows so
        // operators can tell ISM-vs-AMT and an old build apart at a
        // glance. Pairs with the Boot Capabilities pane in Hardware
        // to answer "why doesn't action X work."
        Section {
            id: amtInfoSection
            title: qsTr("AMT INFO")
            visible: {
                const fp = root.controller.amtFingerprint;
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
                    text: root.controller.amtFingerprint.skuLabel
                          || root.controller.amtFingerprint.sku
                          || qsTr("(unknown)")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                Text { text: qsTr("Vendor ID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: root.controller.amtFingerprint.vendorId || qsTr("(unknown)")
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Main FW build"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: root.controller.amtFingerprint.buildNumber || qsTr("(unknown)")
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Recovery FW"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: root.controller.amtFingerprint.recoveryVersion || qsTr("(unknown)")
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Flash"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: root.controller.amtFingerprint.flash || qsTr("(unknown)")
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

            // Repeater + chip delegate keeps the four chips
            // declarative — each binds straight to the controller,
            // no Loader/Connections juggle.
            Flow {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: [
                        { label: qsTr("Redirection port"),
                          active: root.controller.redirectionListenerEnabled,
                          available: true },
                        { label: qsTr("SOL"),
                          active: root.controller.solEnabled,
                          available: true },
                        { label: qsTr("IDE-R"),
                          active: root.controller.iderEnabled,
                          available: true },
                        { label: qsTr("KVM"),
                          active: root.controller.kvmEnabled,
                          available: root.controller.kvmAvailable },
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
                Text { text: root.controller.hostName || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Domain"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.domainName || qsTr("(none)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("Realm"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.digestRealm || qsTr("(unknown)"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }

                Text { text: qsTr("Network if enabled"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.networkInterfaceEnabled ? qsTr("Yes") : qsTr("No"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }

                Text { text: qsTr("RMCP ping"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text { text: root.controller.rmcpPingResponseEnabled ? qsTr("Enabled") : qsTr("Disabled"); color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true }
            }
        }
    }
}
