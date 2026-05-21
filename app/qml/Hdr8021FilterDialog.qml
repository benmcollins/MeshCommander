// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add a new L2 (`AMT_Hdr8021Filter`) System Defense filter. ACM only.
/// Field shape mirrors the legacy NW.js MeshCommander reference flow.
/// See #353.
Dialog {
    id: root

    property string draftName
    /// Filter type radio: 0 = IP (ethertype 2048), 1 = ARP (2054),
    /// 2 = Custom (numeric ethertype from `draftCustomEtherType`).
    property int draftFilterType: 0
    property int draftCustomEtherType: 0x88a8
    /// AMT direction enum: 0 = Tx (outbound), 1 = Rx (inbound).
    property int draftDirection: 0
    /// FilterProfile enum: 0=Allow+Count, 1=Drop+Count, 2=Rate Limit,
    /// 3=Allow, 4=Drop.
    property int draftFilterProfile: 1
    property int draftRateLimit: 100
    property bool draftEventOnMatch: false

    function _resolvedEtherType() {
        switch (draftFilterType) {
        case 0: return 2048;       // IPv4
        case 1: return 2054;       // ARP
        case 2: return draftCustomEtherType;
        }
        return -1;
    }

    function openForAdd() {
        draftName = "";
        draftFilterType = 0;
        draftCustomEtherType = 0x88a8;
        draftDirection = 0;
        draftFilterProfile = 1;
        draftRateLimit = 100;
        draftEventOnMatch = false;
        open();
    }

    /// Emitted on Save with the controller-shaped fields map.
    signal confirmed(var fields)

    title: qsTr("Add L2 filter")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 540

    function _isValid() {
        if (draftName.trim().length === 0) return false;
        const et = _resolvedEtherType();
        if (et < 0 || et > 0xffff) return false;
        if (draftFilterProfile === 2 && draftRateLimit <= 0) return false;
        return true;
    }
    function submitIfReady() {
        if (!_isValid()) return;
        const fields = {
            name: draftName.trim(),
            etherType: _resolvedEtherType(),
            filterProfile: draftFilterProfile,
            filterProfileData: draftRateLimit,
            filterDirection: draftDirection,
            actionEventOnMatch: draftEventOnMatch,
        };
        root.confirmed(fields);
        root.accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("FILTER")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Name")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                TextField {
                    placeholderText: qsTr("Block ARP storms")
                    text: root.draftName
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftName = text
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("Name")
                }

                Text {
                    text: qsTr("Filter type")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                ComboBox {
                    model: [
                        { label: qsTr("Ethernet IP (0x0800)"),  value: 0 },
                        { label: qsTr("ARP (0x0806)"),          value: 1 },
                        { label: qsTr("Custom ethertype"),      value: 2 }
                    ]
                    textRole: "label"
                    currentIndex: root.draftFilterType
                    Layout.fillWidth: true
                    onActivated: root.draftFilterType = currentIndex
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("Filter type")
                }

                Text {
                    text: qsTr("Custom ethertype")
                    visible: root.draftFilterType === 2
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                TextField {
                    visible: root.draftFilterType === 2
                    placeholderText: "0x88a8"
                    text: "0x" + root.draftCustomEtherType.toString(16)
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.preferredWidth: 140
                    onEditingFinished: {
                        const parsed = parseInt(text, text.startsWith("0x") ? 16 : 10);
                        if (!isNaN(parsed) && parsed >= 0 && parsed <= 0xffff)
                            root.draftCustomEtherType = parsed;
                    }
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("Custom ethertype")
                }

                Text {
                    text: qsTr("Direction")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                ComboBox {
                    model: [
                        { label: qsTr("Outbound (Tx)"), value: 0 },
                        { label: qsTr("Inbound (Rx)"),  value: 1 }
                    ]
                    textRole: "label"
                    currentIndex: root.draftDirection
                    Layout.fillWidth: true
                    onActivated: root.draftDirection = currentIndex
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("Direction")
                }
            }
        }

        Section {
            title: qsTr("ACTION")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Profile")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                ComboBox {
                    // Numeric values match AMT's FilterProfile enum.
                    model: [
                        { label: qsTr("Allow + count"),  value: 0 },
                        { label: qsTr("Drop + count"),   value: 1 },
                        { label: qsTr("Rate limit"),     value: 2 },
                        { label: qsTr("Allow"),          value: 3 },
                        { label: qsTr("Drop"),           value: 4 }
                    ]
                    textRole: "label"
                    currentIndex: root.draftFilterProfile
                    Layout.fillWidth: true
                    onActivated: root.draftFilterProfile = currentIndex
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("Profile")
                }

                Text {
                    text: qsTr("Rate limit")
                    visible: root.draftFilterProfile === 2
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                RowLayout {
                    visible: root.draftFilterProfile === 2
                    spacing: 6
                    SpinBox {
                        value: root.draftRateLimit
                        from: 1
                        to: 1000000
                        editable: true
                        onValueModified: root.draftRateLimit = value
                        // #423 — pair the field with its label for AX.
                        Accessible.name: qsTr("Rate limit packets per second")
                    }
                    Text {
                        text: qsTr("packets per second")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: qsTr("Event on match")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                CheckBox {
                    text: qsTr("Log an event when this filter matches a packet")
                    checked: root.draftEventOnMatch
                    onToggled: root.draftEventOnMatch = checked
                    // #423 — pair the checkbox with its label for AX.
                    Accessible.name: qsTr("Event on match")
                }
            }
        }

        Text {
            text: qsTr("L2 filters are matched against the Ethernet/802.1 headers. The filter is created in isolation — bind it to a policy from the policy editor when that lands.")
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
                text: qsTr("Add")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root._isValid()
                onClicked: root.submitIfReady()
            }
        }
    }
}
