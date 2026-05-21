// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add a new L3/L4 (`AMT_IPHeadersFilter`) System Defense filter.
/// ACM only. Field shape mirrors the legacy NW.js MeshCommander
/// IPv4/IPv6 branch. See #358.
Dialog {
    id: root

    property string draftName
    /// 4 = IPv4, 6 = IPv6.
    property int draftIpVersion: 4
    /// 0 = Tx (outbound), 1 = Rx (inbound).
    property int draftDirection: 0
    /// 0..4. See Hdr8021FilterDialog for the labels.
    property int draftFilterProfile: 1
    property int draftRateLimit: 100
    property bool draftEventOnMatch: false
    /// IP protocol number; -1 = any.
    property int draftProtocol: -1
    property string draftSrcAddress
    property string draftDstAddress
    /// Single port for Start; Start==End if End is left at -1.
    property int draftSrcPort: -1
    property int draftSrcPortEnd: -1
    property int draftDstPort: -1
    property int draftDstPortEnd: -1

    function openForAdd() {
        draftName = "";
        draftIpVersion = 4;
        draftDirection = 0;
        draftFilterProfile = 1;
        draftRateLimit = 100;
        draftEventOnMatch = false;
        draftProtocol = -1;
        draftSrcAddress = "";
        draftDstAddress = "";
        draftSrcPort = -1;
        draftSrcPortEnd = -1;
        draftDstPort = -1;
        draftDstPortEnd = -1;
        open();
    }

    /// Emitted on Save with the controller-shaped fields map.
    signal confirmed(var fields)

    title: qsTr("Add L3/L4 filter")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560

    function _isValid() {
        if (draftName.trim().length === 0) return false;
        if (draftFilterProfile === 2 && draftRateLimit <= 0) return false;
        if (draftSrcPort > 65535 || draftDstPort > 65535) return false;
        return true;
    }
    function submitIfReady() {
        if (!_isValid()) return;
        const fields = {
            name: draftName.trim(),
            ipVersion: draftIpVersion,
            filterDirection: draftDirection,
            filterProfile: draftFilterProfile,
            filterProfileData: draftRateLimit,
            actionEventOnMatch: draftEventOnMatch,
            protocol: draftProtocol,
            srcAddress: draftSrcAddress.trim(),
            dstAddress: draftDstAddress.trim(),
            srcPort: draftSrcPort,
            srcPortEnd: draftSrcPortEnd,
            dstPort: draftDstPort,
            dstPortEnd: draftDstPortEnd,
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
                    placeholderText: qsTr("Allow management port")
                    text: root.draftName
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftName = text
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("Name")
                }

                Text {
                    text: qsTr("IP version")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                ComboBox {
                    model: [
                        { label: qsTr("IPv4"), value: 4 },
                        { label: qsTr("IPv6"), value: 6 }
                    ]
                    textRole: "label"
                    currentIndex: root.draftIpVersion === 6 ? 1 : 0
                    Layout.fillWidth: true
                    onActivated: root.draftIpVersion = currentIndex === 1 ? 6 : 4
                    // #423 — pair the field with its label for AX.
                    Accessible.name: qsTr("IP version")
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
            title: qsTr("MATCH (OPTIONAL)")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true

                Text {
                    text: qsTr("Leave a field blank or at -1 to match anything.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
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
                        Layout.preferredWidth: 130
                    }
                    ComboBox {
                        // Numeric values are the IP protocol numbers.
                        model: [
                            { label: qsTr("Any"),  value: -1 },
                            { label: qsTr("TCP"),   value: 6 },
                            { label: qsTr("UDP"),   value: 17 },
                            { label: qsTr("ICMP"),  value: 1 },
                            { label: qsTr("ICMPv6"),value: 58 }
                        ]
                        textRole: "label"
                        currentIndex: {
                            switch (root.draftProtocol) {
                            case 6:  return 1;
                            case 17: return 2;
                            case 1:  return 3;
                            case 58: return 4;
                            default: return 0;
                            }
                        }
                        Layout.preferredWidth: 200
                        onActivated: {
                            switch (currentIndex) {
                            case 1: root.draftProtocol = 6;   break;
                            case 2: root.draftProtocol = 17;  break;
                            case 3: root.draftProtocol = 1;   break;
                            case 4: root.draftProtocol = 58;  break;
                            default: root.draftProtocol = -1;
                            }
                        }
                        // #423 — pair the field with its label for AX.
                        Accessible.name: qsTr("Protocol")
                    }

                    Text {
                        text: qsTr("Source IP")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 130
                    }
                    TextField {
                        placeholderText: root.draftIpVersion === 4
                            ? qsTr("10.0.0.5")
                            : qsTr("fe80::1")
                        text: root.draftSrcAddress
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftSrcAddress = text
                        // #423 — pair the field with its label for AX.
                        Accessible.name: qsTr("Source IP")
                    }

                    Text {
                        text: qsTr("Destination IP")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 130
                    }
                    TextField {
                        placeholderText: root.draftIpVersion === 4
                            ? qsTr("10.0.0.5")
                            : qsTr("fe80::1")
                        text: root.draftDstAddress
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftDstAddress = text
                        // #423 — pair the field with its label for AX.
                        Accessible.name: qsTr("Destination IP")
                    }

                    Text {
                        text: qsTr("Source port")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 130
                    }
                    RowLayout {
                        spacing: 6
                        SpinBox {
                            value: root.draftSrcPort
                            from: -1
                            to: 65535
                            editable: true
                            onValueModified: root.draftSrcPort = value
                            // #423 — pair the field with its label for AX.
                            Accessible.name: qsTr("Source port")
                        }
                        Text {
                            text: qsTr("through")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            visible: root.draftSrcPort >= 0
                        }
                        SpinBox {
                            visible: root.draftSrcPort >= 0
                            value: root.draftSrcPortEnd >= 0
                                ? root.draftSrcPortEnd : root.draftSrcPort
                            from: -1
                            to: 65535
                            editable: true
                            onValueModified: root.draftSrcPortEnd = value
                            // #423 — pair the field with its label for AX.
                            Accessible.name: qsTr("Source port end")
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        text: qsTr("Destination port")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 130
                    }
                    RowLayout {
                        spacing: 6
                        SpinBox {
                            value: root.draftDstPort
                            from: -1
                            to: 65535
                            editable: true
                            onValueModified: root.draftDstPort = value
                            // #423 — pair the field with its label for AX.
                            Accessible.name: qsTr("Destination port")
                        }
                        Text {
                            text: qsTr("through")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            visible: root.draftDstPort >= 0
                        }
                        SpinBox {
                            visible: root.draftDstPort >= 0
                            value: root.draftDstPortEnd >= 0
                                ? root.draftDstPortEnd : root.draftDstPort
                            from: -1
                            to: 65535
                            editable: true
                            onValueModified: root.draftDstPortEnd = value
                            // #423 — pair the field with its label for AX.
                            Accessible.name: qsTr("Destination port end")
                        }
                        Item { Layout.fillWidth: true }
                    }
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
            text: qsTr("L3/L4 filters match against IP headers + transport ports. Bind the filter to a policy from the policy editor to activate it.")
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
