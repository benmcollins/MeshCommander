// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Hardware" section (was the inline section 1
/// before #325). Pulled into its own file — see OverviewSection.qml
/// for the rationale.
Flickable {
    id: root

    required property MachineDetailsController controller

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
