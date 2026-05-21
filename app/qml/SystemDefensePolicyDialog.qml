// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add a new `AMT_SystemDefensePolicy` — name, priority, six default-
/// action flags (Tx/Rx × count/drop/event), and a multi-select picker
/// over the existing L2 + L3 filters. ACM only. See #357.
Dialog {
    id: root

    required property MachineDetailsController controller

    property string draftPolicyName
    property int draftPriority: 0

    property bool draftTxDefaultCount: false
    property bool draftTxDefaultDrop: false
    property bool draftTxDefaultMatchEvent: false
    property bool draftRxDefaultCount: false
    property bool draftRxDefaultDrop: false
    property bool draftRxDefaultMatchEvent: false

    /// Selected filter InstanceIDs. The controller pulls the trailing
    /// integer handle out of each for the on-wire `FilterCreationHandles`
    /// repeated element.
    property list<string> draftFilterInstanceIds: []

    function openForAdd() {
        draftPolicyName = "";
        draftPriority = 0;
        draftTxDefaultCount = false;
        draftTxDefaultDrop = false;
        draftTxDefaultMatchEvent = false;
        draftRxDefaultCount = false;
        draftRxDefaultDrop = false;
        draftRxDefaultMatchEvent = false;
        draftFilterInstanceIds = [];
        open();
    }

    function _toggleFilter(instanceId, checked) {
        const list = draftFilterInstanceIds.slice();
        const idx = list.indexOf(instanceId);
        if (checked && idx < 0)       list.push(instanceId);
        else if (!checked && idx >= 0) list.splice(idx, 1);
        draftFilterInstanceIds = list;
    }

    /// Emitted on Save with the controller-shaped fields map.
    signal confirmed(var fields)

    title: qsTr("Add policy")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 600

    function _isValid() {
        return draftPolicyName.trim().length > 0;
    }
    function submitIfReady() {
        if (!_isValid()) return;
        root.confirmed({
            policyName: draftPolicyName.trim(),
            priority: draftPriority,
            txDefaultCount: draftTxDefaultCount,
            txDefaultDrop: draftTxDefaultDrop,
            txDefaultMatchEvent: draftTxDefaultMatchEvent,
            rxDefaultCount: draftRxDefaultCount,
            rxDefaultDrop: draftRxDefaultDrop,
            rxDefaultMatchEvent: draftRxDefaultMatchEvent,
            filterInstanceIds: draftFilterInstanceIds,
        });
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
            title: qsTr("POLICY")
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
                    placeholderText: qsTr("default-drop")
                    text: root.draftPolicyName
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftPolicyName = text
                    // #379 — pair the field with its label for AX.
                    Accessible.name: qsTr("Policy name")
                }

                Text {
                    text: qsTr("Priority")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                RowLayout {
                    spacing: 6
                    SpinBox {
                        value: root.draftPriority
                        from: 0
                        to: 65535
                        editable: true
                        onValueModified: root.draftPriority = value
                        // #379 — pair the field with its label for AX.
                        Accessible.name: qsTr("Priority")
                    }
                    Text {
                        text: qsTr("higher = applied first")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        Section {
            title: qsTr("DEFAULT ACTIONS (NO MATCH)")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true

                Text {
                    text: qsTr("What the firmware does when a packet doesn't match any included filter.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                }

                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 6
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Outbound (Tx)")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        font.weight: Font.Medium
                    }
                    CheckBox {
                        text: qsTr("Count")
                        checked: root.draftTxDefaultCount
                        onToggled: root.draftTxDefaultCount = checked
                    }
                    CheckBox {
                        text: qsTr("Drop")
                        checked: root.draftTxDefaultDrop
                        onToggled: root.draftTxDefaultDrop = checked
                    }
                    CheckBox {
                        text: qsTr("Event")
                        checked: root.draftTxDefaultMatchEvent
                        onToggled: root.draftTxDefaultMatchEvent = checked
                    }

                    Text {
                        text: qsTr("Inbound (Rx)")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        font.weight: Font.Medium
                    }
                    CheckBox {
                        text: qsTr("Count")
                        checked: root.draftRxDefaultCount
                        onToggled: root.draftRxDefaultCount = checked
                    }
                    CheckBox {
                        text: qsTr("Drop")
                        checked: root.draftRxDefaultDrop
                        onToggled: root.draftRxDefaultDrop = checked
                    }
                    CheckBox {
                        text: qsTr("Event")
                        checked: root.draftRxDefaultMatchEvent
                        onToggled: root.draftRxDefaultMatchEvent = checked
                    }
                }
            }
        }

        Section {
            title: qsTr("FILTERS")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                Text {
                    text: qsTr("Pick the L2 / L3-L4 filters this policy applies. A policy with no filters relies only on the default actions above.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Text {
                    visible: ((root.controller.systemDefense
                                && root.controller.systemDefense.hdrFilters) || []).length > 0
                    text: qsTr("L2 (ethertype)")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.letterSpacing: 1
                }
                Repeater {
                    model: (root.controller.systemDefense
                             && root.controller.systemDefense.hdrFilters) || []
                    delegate: CheckBox {
                        required property var modelData
                        text: (modelData.name || modelData.instanceId)
                            + "  ·  " + modelData.instanceId
                        checked: root.draftFilterInstanceIds.indexOf(modelData.instanceId) >= 0
                        onToggled: root._toggleFilter(modelData.instanceId, checked)
                    }
                }

                Text {
                    visible: ((root.controller.systemDefense
                                && root.controller.systemDefense.ipFilters) || []).length > 0
                    text: qsTr("L3 / L4 (IP / ports)")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.letterSpacing: 1
                    Layout.topMargin: 6
                }
                Repeater {
                    model: (root.controller.systemDefense
                             && root.controller.systemDefense.ipFilters) || []
                    delegate: CheckBox {
                        required property var modelData
                        text: (modelData.name || modelData.instanceId)
                            + "  ·  " + modelData.instanceId
                        checked: root.draftFilterInstanceIds.indexOf(modelData.instanceId) >= 0
                        onToggled: root._toggleFilter(modelData.instanceId, checked)
                    }
                }

                Text {
                    visible: ((root.controller.systemDefense
                                && root.controller.systemDefense.hdrFilters) || []).length === 0
                          && ((root.controller.systemDefense
                                && root.controller.systemDefense.ipFilters) || []).length === 0
                    text: qsTr("No filters available — create some from the L2 / L3 sections first.")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
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
                text: qsTr("Add")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root._isValid()
                onClicked: root.submitIfReady()
            }
        }
    }
}
