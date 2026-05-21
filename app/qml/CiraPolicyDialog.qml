// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit a CIRA remote-access policy rule. AMT exposes exactly
/// three triggers (User Initiated / Alert / Periodic); the firmware
/// is idempotent on `Trigger`, so editing an existing policy reuses
/// the Add path with the same trigger. Periodic has a sub-form for
/// the schedule (interval or daily time-of-day). MPS servers are
/// picked from `controller.remoteAccess.servers` and split into CIRA
/// vs CILA buckets by `mpsType`.
Dialog {
    id: root

    required property var controller

    property bool isEdit: false

    /// 0 = User Initiated, 1 = Alert, 2 = Periodic.
    property int draftTrigger: 0
    property int draftTunnelLifeTime: 60
    /// Only used when draftTrigger === 2.
    property string draftPeriodicMode: "interval"  // or "timeOfDay"
    property int draftPeriodicSeconds: 300
    property int draftPeriodicHour: 12
    property int draftPeriodicMinute: 0
    /// Sets of MpsServer.name strings — split by mpsType. Stored as
    /// JS Sets via a backing var; QML's `model.contains(x)` doesn't
    /// reach into Set semantics, so we mirror them as object maps.
    property var ciraSelected: ({})
    property var cilaSelected: ({})

    function openForAdd() {
        isEdit = false;
        draftTrigger = 0;
        draftTunnelLifeTime = 60;
        draftPeriodicMode = "interval";
        draftPeriodicSeconds = 300;
        draftPeriodicHour = 12;
        draftPeriodicMinute = 0;
        ciraSelected = ({});
        cilaSelected = ({});
        open();
    }

    function openForEdit(policy) {
        isEdit = true;
        draftTrigger = policy.trigger || 0;
        draftTunnelLifeTime = policy.tunnelLifeTime || 0;
        // Pull the structured periodic fields directly off the policy
        // record (set by MachineDetailsController::refreshRemoteAccess).
        // The decoder sets at most one of the two periodic flags, so
        // favour whichever is true; otherwise fall back to interval.
        if (policy.periodicInterval) {
            draftPeriodicMode = "interval";
            draftPeriodicSeconds = policy.periodicSeconds || 300;
        } else if (policy.periodicTimeOfDay) {
            draftPeriodicMode = "timeOfDay";
            draftPeriodicHour   = policy.periodicHour || 0;
            draftPeriodicMinute = policy.periodicMinute || 0;
        } else {
            draftPeriodicMode = "interval";
            draftPeriodicSeconds = 300;
        }
        // Re-hydrate CIRA / CILA selections from the policy's bound
        // server-name list. The server's mpsType (looked up against
        // the freshly-read servers list) decides which bucket each
        // name goes into.
        const cira = {};
        const cila = {};
        const ra = root.controller.remoteAccess || {};
        const servers = ra.servers || [];
        const boundNames = policy.mpsNames || [];
        for (let j = 0; j < boundNames.length; ++j) {
            const n = boundNames[j];
            let mpsType = 0;
            for (let k = 0; k < servers.length; ++k) {
                if (servers[k].name === n) {
                    mpsType = servers[k].mpsType || 0;
                    break;
                }
            }
            if (mpsType === 1) cila[n] = true; else cira[n] = true;
        }
        ciraSelected = cira;
        cilaSelected = cila;
        open();
    }

    function _toggleSet(setProp, name) {
        const copy = Object.assign({}, root[setProp]);
        if (copy[name]) {
            delete copy[name];
        } else {
            copy[name] = true;
        }
        root[setProp] = copy;
    }

    function _keys(obj) {
        const out = [];
        for (const k in obj) if (obj[k]) out.push(k);
        return out;
    }

    function _triggerLabel(t) {
        if (t === 0) return qsTr("User Initiated");
        if (t === 1) return qsTr("Alert");
        if (t === 2) return qsTr("Periodic");
        return qsTr("(unknown)");
    }

    title: isEdit ? qsTr("Edit CIRA policy") : qsTr("Add CIRA policy")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 600

    function submitReady() {
        const haveServer = _keys(ciraSelected).length > 0
                        || _keys(cilaSelected).length > 0;
        if (!haveServer) return false;
        if (draftTrigger !== 2) return true;
        if (draftPeriodicMode === "interval")
            return draftPeriodicSeconds > 0;
        if (draftPeriodicMode === "timeOfDay")
            return draftPeriodicHour >= 0 && draftPeriodicHour <= 23
                && draftPeriodicMinute >= 0 && draftPeriodicMinute <= 59;
        return false;
    }

    function submitIfReady() {
        if (!submitReady()) return;
        const fields = {
            "trigger":        draftTrigger,
            "tunnelLifeTime": draftTunnelLifeTime,
            "ciraMpsNames":   _keys(ciraSelected),
            "cilaMpsNames":   _keys(cilaSelected),
        };
        if (draftTrigger === 2) {
            fields["periodicMode"] = draftPeriodicMode;
            if (draftPeriodicMode === "interval") {
                fields["periodicSeconds"] = draftPeriodicSeconds;
            } else {
                fields["periodicHour"]   = draftPeriodicHour;
                fields["periodicMinute"] = draftPeriodicMinute;
            }
        }
        controller.addCiraPolicyRule(fields);
        accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("TRIGGER")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("User Initiated")
                        enabled: !root.isEdit
                        checked: root.draftTrigger === 0
                        onClicked: root.draftTrigger = 0
                    }
                    RadioButton {
                        text: qsTr("Alert")
                        enabled: !root.isEdit
                        checked: root.draftTrigger === 1
                        onClicked: root.draftTrigger = 1
                    }
                    RadioButton {
                        text: qsTr("Periodic")
                        enabled: !root.isEdit
                        checked: root.draftTrigger === 2
                        onClicked: root.draftTrigger = 2
                    }
                }
                Text {
                    visible: root.isEdit
                    text: qsTr("Trigger isn't editable — to change it, remove this policy and add a new one.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Section {
            title: qsTr("TUNNEL")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Lifetime (s)")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                TextField {
                    text: root.draftTunnelLifeTime
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.preferredWidth: 120
                    validator: IntValidator { bottom: 0; top: 86400 }
                    onTextEdited: root.draftTunnelLifeTime = parseInt(text) || 0
                }
            }
        }

        Section {
            title: qsTr("SCHEDULE")
            visible: root.draftTrigger === 2
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("Every N seconds")
                        checked: root.draftPeriodicMode === "interval"
                        onClicked: root.draftPeriodicMode = "interval"
                    }
                    RadioButton {
                        text: qsTr("Daily at HH:MM")
                        checked: root.draftPeriodicMode === "timeOfDay"
                        onClicked: root.draftPeriodicMode = "timeOfDay"
                    }
                }

                GridLayout {
                    visible: root.draftPeriodicMode === "interval"
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Seconds")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 130
                    }
                    TextField {
                        text: root.draftPeriodicSeconds
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.preferredWidth: 120
                        validator: IntValidator { bottom: 1; top: 86400 }
                        onTextEdited: root.draftPeriodicSeconds = parseInt(text) || 0
                    }
                }

                RowLayout {
                    visible: root.draftPeriodicMode === "timeOfDay"
                    spacing: 8
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Time of day")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 130
                    }
                    TextField {
                        text: root.draftPeriodicHour
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.preferredWidth: 60
                        validator: IntValidator { bottom: 0; top: 23 }
                        onTextEdited: root.draftPeriodicHour = parseInt(text) || 0
                    }
                    Text {
                        text: ":"
                        color: Colors.text
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                    }
                    TextField {
                        text: root.draftPeriodicMinute
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.preferredWidth: 60
                        validator: IntValidator { bottom: 0; top: 59 }
                        onTextEdited: root.draftPeriodicMinute = parseInt(text) || 0
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        Section {
            title: qsTr("MPS SERVERS")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("CIRA (external)")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.letterSpacing: 1
                }
                Repeater {
                    model: {
                        const ra = root.controller.remoteAccess || {};
                        const all = ra.servers || [];
                        return all.filter(s => (s.mpsType || 0) === 0);
                    }
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        CheckBox {
                            text: ""
                            checked: !!root.ciraSelected[modelData.name]
                            onClicked: root._toggleSet("ciraSelected", modelData.name)
                        }
                        Text {
                            text: qsTr("%1  (%2:%3)")
                                .arg(modelData.name || "")
                                .arg(modelData.accessInfo || "")
                                .arg(modelData.port || 0)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
                Text {
                    visible: ((root.controller.remoteAccess
                                && root.controller.remoteAccess.servers) || [])
                              .filter(s => (s.mpsType || 0) === 0).length === 0
                    text: qsTr("(no CIRA servers configured)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }

                Text {
                    text: qsTr("CILA (local — AMT 11.6+)")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.letterSpacing: 1
                    Layout.topMargin: 6
                }
                Repeater {
                    model: {
                        const ra = root.controller.remoteAccess || {};
                        const all = ra.servers || [];
                        return all.filter(s => (s.mpsType || 0) === 1);
                    }
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        CheckBox {
                            text: ""
                            checked: !!root.cilaSelected[modelData.name]
                            onClicked: root._toggleSet("cilaSelected", modelData.name)
                        }
                        Text {
                            text: qsTr("%1  (%2:%3)")
                                .arg(modelData.name || "")
                                .arg(modelData.accessInfo || "")
                                .arg(modelData.port || 0)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
                Text {
                    visible: ((root.controller.remoteAccess
                                && root.controller.remoteAccess.servers) || [])
                              .filter(s => (s.mpsType || 0) === 1).length === 0
                    text: qsTr("(no CILA servers configured)")
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
                text: root.isEdit ? qsTr("Save changes") : qsTr("Add policy")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.submitReady()
                onClicked: root.submitIfReady()
            }
        }
    }
}
