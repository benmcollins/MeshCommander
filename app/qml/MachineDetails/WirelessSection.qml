// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Wireless" section (was the inline section 4
/// before #325). Surfaces the AMT WiFi profiles + 802.1x setup with
/// destructive-confirm gating on disable/clear. See OverviewSection.qml
/// for the file-extract rationale.
Flickable {
    id: root

    required property MachineDetailsController controller
    required property string machineHost
    required property var confirmPower
    required property var wifiProfileDialog
    required property var wifiSyncDialog
    required property var wired8021xDialog

    contentWidth: width
    contentHeight: wirelessCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: wirelessCol
        spacing: 18
        width: parent.width

        SectionHeader {
            eyebrow: qsTr("WIRELESS")
            title: {
                const w = controller.wireless;
                if (!w || !w.ok)
                    return controller.busy
                        ? qsTr("Loading…")
                        : qsTr("No wireless profiles");
                if (w.port && w.port.present)
                    return w.port.currentSsid
                        ? w.port.currentSsid
                        : qsTr("Wireless interface");
                return qsTr("No wireless interface");
            }
            // Show "Loading…" while the fetch is in flight
            // (MachineDetailsWindow auto-fires it on section switch).
            // Once it returns empty, surface the empty-state noun
            // instead of telling the user to click a button (#378).
            hint: (!controller.wireless || !controller.wireless.ok)
                ? (controller.busy
                    ? qsTr("Loading…")
                    : qsTr("No wireless profiles"))
                : ""
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
                                destructive: true
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
