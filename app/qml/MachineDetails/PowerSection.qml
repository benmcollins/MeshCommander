// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Power" section (was the inline section 2
/// before #325). Hosts the buttons that drive the basic AMT power
/// actions plus the Boot to… menu (BootMenu.qml; see #299). All five
/// pre-shipped cap-gated prompts are threaded through so BootMenu
/// can render Secure Erase / Platform Erase / HTTPS / OCR.
ColumnLayout {
    id: root

    required property MachineDetailsController controller
    required property string machineHost
    required property var confirmPower
    required property var powerPolicyDialog
    required property var secureErasePrompt
    required property var platformErasePrompt
    required property var httpsBootPrompt
    required property var ocrPrompt

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
            font.pixelSize: Type.sizeXl
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
