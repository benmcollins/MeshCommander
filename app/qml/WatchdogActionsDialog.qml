// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Manage the per-watchdog action list. AMT models each row in
/// `AMT_AgentPresenceWatchdogAction` as `(Watchdog, OldState, NewState,
/// EventOnTransition, ActionSAC, ActionEAC)` — fire `ActionSAC` when the
/// watchdog transitions from `OldState` to `NewState`. The dialog lists
/// the existing rows for one watchdog with Remove, and an Add form
/// underneath. There's no in-place Edit because AMT doesn't expose one:
/// the firmware only offers AddAction and Delete, so "edit" is two
/// clicks (Remove + Add) and surfacing it as one button would lie. See
/// #350.
Dialog {
    id: root

    required property MachineDetailsController controller

    /// The watchdog this dialog is scoped to.
    property string watchdogDeviceIdGuid
    property string watchdogTitle

    /// Action draft (only set while the Add form is being filled in).
    /// State codes mirror `watchdogStateName` in operations.cpp.
    /// Default to the common case: "Running → Expired", reset the box.
    property int draftOldState: 4   // Running
    property int draftNewState: 8   // Expired
    property bool draftEventOnTransition: true
    property int draftActionSac: 17 // Reset

    readonly property var watchdogStates: [
        { code: 1,  label: qsTr("Not Started") },
        { code: 2,  label: qsTr("Stopped") },
        { code: 4,  label: qsTr("Running") },
        { code: 8,  label: qsTr("Expired") },
        { code: 16, label: qsTr("Suspended") }
    ]

    /// Same enum the read side surfaces via `actionSacName` in
    /// operations.cpp. Anything outside this list shows up as
    /// "Unknown" in the existing-rows list but the operator can still
    /// delete it.
    readonly property var actionSacs: [
        { code: 17, label: qsTr("Reset") },
        { code: 18, label: qsTr("Power Cycle") },
        { code: 16, label: qsTr("Power Off") },
        { code: 2,  label: qsTr("Log to Event Log") },
        { code: 1,  label: qsTr("Send Alert") }
    ]

    /// Actions belonging to this watchdog. Recomputes each time the
    /// controller's `agentPresence` map changes (which the controller
    /// emits after every add / delete refresh).
    readonly property var ownedActions: {
        const ap = controller.agentPresence || {};
        const all = ap.actions || [];
        const out = [];
        for (let i = 0; i < all.length; i++) {
            if (all[i].watchdogDeviceIdGuid === root.watchdogDeviceIdGuid)
                out.push(all[i]);
        }
        return out;
    }

    readonly property bool atCeiling: {
        const ap = controller.agentPresence || {};
        const cap = ap.maxTotalActions || 0;
        const cur = (ap.actions || []).length;
        return cap > 0 && cur >= cap;
    }

    function _stateIndexFor(code) {
        for (let i = 0; i < watchdogStates.length; i++) {
            if (watchdogStates[i].code === code) return i;
        }
        return 0;
    }
    function _stateLabel(code) {
        for (let i = 0; i < watchdogStates.length; i++) {
            if (watchdogStates[i].code === code) return watchdogStates[i].label;
        }
        return qsTr("Unknown");
    }
    function _actionIndexFor(code) {
        for (let i = 0; i < actionSacs.length; i++) {
            if (actionSacs[i].code === code) return i;
        }
        return 0;
    }

    function openFor(watchdogRow) {
        watchdogDeviceIdGuid = watchdogRow.deviceIdGuid || "";
        watchdogTitle = watchdogRow.description && watchdogRow.description.length > 0
            ? watchdogRow.description
            : watchdogRow.deviceIdGuid || "";
        // Reset the Add form so a fresh open doesn't carry yesterday's draft.
        draftOldState = 4;
        draftNewState = 8;
        draftEventOnTransition = true;
        draftActionSac = 17;
        open();
    }

    /// Emitted when the operator clicks Add in the form. The
    /// controller dispatches to addWatchdogAction; the dialog stays
    /// open so the operator can add several actions in one sitting.
    signal addRequested(var fields)
    /// Emitted when the operator clicks Remove on an existing row.
    signal removeRequested(string watchdogDeviceIdGuid,
                            int oldState, int newState)

    title: qsTr("Watchdog actions")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 620

    function _isValid() {
        // Same OldState/NewState would never fire — block at the form.
        return draftOldState !== draftNewState && !root.atCeiling;
    }
    function submitIfReady() {
        if (!_isValid()) return;
        root.addRequested({
            watchdogDeviceIdGuid: root.watchdogDeviceIdGuid,
            oldState: root.draftOldState,
            newState: root.draftNewState,
            eventOnTransition: root.draftEventOnTransition,
            actionSac: root.draftActionSac,
            actionEac: -1,
        });
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Text {
            text: qsTr("Manage actions for %1").arg(root.watchdogTitle)
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeM
            font.weight: Font.Medium
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        Text {
            text: {
                const ap = root.controller.agentPresence || {};
                const cap = ap.maxTotalActions || 0;
                if (cap <= 0)
                    return qsTr("%1 action(s) configured for this watchdog.")
                        .arg(root.ownedActions.length);
                const total = (ap.actions || []).length;
                return qsTr("%1 for this watchdog · %2 / %3 total across the device.")
                    .arg(root.ownedActions.length).arg(total).arg(cap);
            }
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            Layout.fillWidth: true
        }

        Section {
            title: qsTr("CONFIGURED")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                Text {
                    visible: root.ownedActions.length === 0
                    text: qsTr("No actions configured. A watchdog with no actions still works — it just won't take action when the agent expires.")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Repeater {
                    model: root.ownedActions
                    delegate: Rectangle {
                        id: actionRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: rowLayout.implicitHeight + 12
                        color: Colors.surface
                        border.color: Colors.borderMuted
                        border.width: 1
                        radius: 6

                        RowLayout {
                            id: rowLayout
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            spacing: 10

                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true

                                Text {
                                    text: qsTr("%1 → %2  ·  %3")
                                        .arg(actionRow.modelData.oldStateLabel)
                                        .arg(actionRow.modelData.newStateLabel)
                                        .arg(actionRow.modelData.actionSacLabel)
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: actionRow.modelData.eventOnTransition
                                        ? qsTr("logs an event on transition")
                                        : qsTr("silent (no event)")
                                    color: Colors.textFaint
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                }
                            }

                            FlatButton {
                                text: qsTr("Remove")
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                destructive: true
                                onClicked: root.removeRequested(
                                    root.watchdogDeviceIdGuid,
                                    actionRow.modelData.oldState,
                                    actionRow.modelData.newState)
                            }
                        }
                    }
                }
            }
        }

        Section {
            title: qsTr("ADD ACTION")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                Text {
                    visible: root.atCeiling
                    text: qsTr("At the firmware ceiling — remove an action somewhere on the device before adding a new one.")
                    color: Colors.textFaint
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
                        text: qsTr("When transitions from")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 180
                    }
                    ComboBox {
                        model: root.watchdogStates
                        textRole: "label"
                        valueRole: "code"
                        currentIndex: root._stateIndexFor(root.draftOldState)
                        Layout.fillWidth: true
                        onActivated: root.draftOldState =
                            root.watchdogStates[currentIndex].code
                    }

                    Text {
                        text: qsTr("...to")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 180
                    }
                    ComboBox {
                        model: root.watchdogStates
                        textRole: "label"
                        valueRole: "code"
                        currentIndex: root._stateIndexFor(root.draftNewState)
                        Layout.fillWidth: true
                        onActivated: root.draftNewState =
                            root.watchdogStates[currentIndex].code
                    }

                    Text {
                        text: qsTr("Then do")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 180
                    }
                    ComboBox {
                        model: root.actionSacs
                        textRole: "label"
                        valueRole: "code"
                        currentIndex: root._actionIndexFor(root.draftActionSac)
                        Layout.fillWidth: true
                        onActivated: root.draftActionSac =
                            root.actionSacs[currentIndex].code
                    }

                    Item { Layout.preferredWidth: 180 }
                    CheckBox {
                        text: qsTr("Also log an event on the transition")
                        checked: root.draftEventOnTransition
                        onToggled: root.draftEventOnTransition = checked
                    }
                }

                Text {
                    visible: root.draftOldState === root.draftNewState
                    text: qsTr("OldState and NewState must differ — same-state transitions never fire.")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            FlatButton {
                text: qsTr("Close")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.accept()
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
