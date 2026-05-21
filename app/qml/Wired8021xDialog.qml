// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Edit the wired `AMT_8021XProfile`. Credentials / certs aren't
/// editable here — full enterprise credential binding ships with the
/// Phase C EAP-TLS PR. This dialog covers the Enabled toggle and the
/// authentication-protocol selector.
Dialog {
    id: root

    required property MachineDetailsController controller

    property bool draftEnabled: false
    property int draftAuthenticationProtocol: 0

    function openForEdit(wired) {
        draftEnabled = wired.enabled === true;
        draftAuthenticationProtocol = wired.authenticationProtocol >= 0
            ? wired.authenticationProtocol : 0;
        open();
    }

    title: qsTr("Wired 802.1X")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 480

    readonly property var protocolCatalogue: [
        { code: 0, label: qsTr("EAP-TLS") },
        { code: 1, label: qsTr("EAP-TTLS / MSCHAPv2") },
        { code: 2, label: qsTr("PEAPv0 / MSCHAPv2") },
        { code: 3, label: qsTr("PEAPv1 / GTC") },
        { code: 4, label: qsTr("EAP-FAST / MSCHAPv2") },
        { code: 5, label: qsTr("EAP-FAST / GTC") },
    ]

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Section {
            title: qsTr("PROFILE")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true
                CheckBox {
                    text: qsTr("Enabled")
                    checked: root.draftEnabled
                    onToggled: root.draftEnabled = checked
                    // #423 — pair the checkbox with its label for AX.
                    Accessible.name: qsTr("Enabled")
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Protocol")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 110
                    }
                    ComboBox {
                        model: root.protocolCatalogue
                        textRole: "label"
                        valueRole: "code"
                        currentIndex: {
                            for (let i = 0; i < model.length; ++i)
                                if (model[i].code === root.draftAuthenticationProtocol)
                                    return i;
                            return 0;
                        }
                        Layout.fillWidth: true
                        onActivated: function(idx) {
                            root.draftAuthenticationProtocol = model[idx].code;
                        }
                        // #423 — pair the field with its label for AX.
                        Accessible.name: qsTr("Authentication protocol")
                    }
                }
            }
        }

        Text {
            text: qsTr("Credentials / certificates aren't editable here — full enterprise binding (client cert + CA cert) will ship in a later release.")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
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
                    root.controller.setWiredEnterpriseProfile(
                        root.draftEnabled,
                        root.draftAuthenticationProtocol);
                    root.accept();
                }
            }
        }
    }
}
