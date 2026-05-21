// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Change the state of `AMT_UserInitiatedConnectionService`. AMT
/// exposes four modes — only one is enabled at a time. Used when
/// the operator wants to allow OS-side software (or the BIOS) to
/// initiate the CIRA tunnel rather than waiting for AMT to do so on
/// its own policy schedule.
Dialog {
    id: root

    required property var controller

    /// 32768 Disabled / 32769 BIOS / 32770 OS / 32771 BIOS+OS.
    property int draftState: 32768

    function openForEdit(userInitiated) {
        draftState = (userInitiated && userInitiated.enabledState !== undefined)
            ? userInitiated.enabledState : 32768;
        open();
    }

    title: qsTr("User-initiated CIRA")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 460

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Section {
            title: qsTr("MODE")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true
                RadioButton {
                    text: qsTr("Disabled — AMT manages CIRA on its own schedule")
                    checked: root.draftState === 32768
                    onClicked: root.draftState = 32768
                }
                RadioButton {
                    text: qsTr("BIOS only")
                    checked: root.draftState === 32769
                    onClicked: root.draftState = 32769
                }
                RadioButton {
                    text: qsTr("OS only — OS-side software can open tunnels")
                    checked: root.draftState === 32770
                    onClicked: root.draftState = 32770
                }
                RadioButton {
                    text: qsTr("BIOS + OS")
                    checked: root.draftState === 32771
                    onClicked: root.draftState = 32771
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            FlatButton {
                text: qsTr("Cancel")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.reject()
            }
            AccentButton {
                text: qsTr("Apply")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    root.controller.setUserInitiatedConnectionState(root.draftState);
                    root.accept();
                }
            }
        }
    }
}
