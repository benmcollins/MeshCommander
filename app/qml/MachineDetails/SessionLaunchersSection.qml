// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Remote access" section (was the inline
/// section 6 before #325). Launchers for SOL / KVM / IDE-R that hand
/// off to the per-machine SessionWindow via the window-scoped
/// `sessionLoader`. Threads through `confirmPower` for the
/// destructive opt-in changes and the host/user strings the
/// SessionWindow needs. See OverviewSection.qml for the file-extract
/// rationale.
ColumnLayout {
    id: root

    required property MachineDetailsController controller
    required property string machineHost
    required property string machineUser
    required property var confirmPower
    /// Window-scoped Loader that creates SessionWindow on demand.
    /// Section calls `sessionLoader.launchAt(tabIndex)`.
    required property var sessionLoader

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
