// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit an Agent Presence watchdog (AMT_AgentPresenceWatchdog).
/// Edit = delete + RegisterAgent since AMT has no Modify; the controller
/// chains the two-step. Per-watchdog actions (AddAction chain) are
/// edited in WatchdogActionsDialog. See #348 / #350.
Dialog {
    id: root

    /// The DeviceID of the row being edited (empty on Add). Returned
    /// in `confirmed` so the controller knows which row to drop.
    property string editingDeviceIdGuid
    property bool isEdit: false

    property string draftDeviceIdGuid
    property string draftDescription
    /// MonitoredEntity enum: 7 = Application (the common default).
    property int draftMonitoredEntityCode: 7
    property int draftStartupIntervalSec: 60
    property int draftTimeoutIntervalSec: 30

    /// Same enum as `monitoredEntityName` in wsman/src/operations.cpp.
    readonly property var monitoredEntities: [
        { code: 2, label: qsTr("Operating System") },
        { code: 3, label: qsTr("OS Boot Process") },
        { code: 4, label: qsTr("OS Shutdown Process") },
        { code: 5, label: qsTr("Firmware Boot Process") },
        { code: 6, label: qsTr("BIOS Boot Process") },
        { code: 7, label: qsTr("Application") },
        { code: 8, label: qsTr("Service Processor") },
        { code: 1, label: qsTr("Other") }
    ]

    function _entityIndexFor(code) {
        for (let i = 0; i < monitoredEntities.length; i++) {
            if (monitoredEntities[i].code === code) return i;
        }
        return monitoredEntities.length - 1; // Other
    }

    /// AMT identifies watchdogs by a 16-byte GUID. A random one keeps
    /// operators from having to invent and remember an identifier; if
    /// they need a stable one (e.g. for an agent that re-registers
    /// itself) they can paste it in.
    function _generateGuid() {
        const hex = "0123456789abcdef";
        let s = "";
        for (let i = 0; i < 32; i++) {
            if (i === 8 || i === 12 || i === 16 || i === 20) s += "-";
            // For positions 12 (version, force to 4) and 16 (variant,
            // force to 8/9/a/b) follow RFC 4122 v4 — AMT accepts any
            // GUID, but staying inside the spec avoids confusing
            // anyone reading the row later.
            if (i === 12) { s += "4"; continue; }
            if (i === 16) { s += hex[8 + Math.floor(Math.random() * 4)]; continue; }
            s += hex[Math.floor(Math.random() * 16)];
        }
        return s;
    }

    function openForAdd() {
        isEdit = false;
        editingDeviceIdGuid = "";
        draftDeviceIdGuid = _generateGuid();
        draftDescription = "";
        draftMonitoredEntityCode = 7;
        draftStartupIntervalSec = 60;
        draftTimeoutIntervalSec = 30;
        open();
    }

    function openForEdit(row) {
        isEdit = true;
        editingDeviceIdGuid = row.deviceIdGuid || "";
        draftDeviceIdGuid = row.deviceIdGuid || "";
        draftDescription = row.description || "";
        draftMonitoredEntityCode = row.monitoredEntityCode >= 0
            ? row.monitoredEntityCode : 7;
        draftStartupIntervalSec = row.startupIntervalSec > 0
            ? row.startupIntervalSec : 60;
        draftTimeoutIntervalSec = row.timeoutIntervalSec > 0
            ? row.timeoutIntervalSec : 30;
        open();
    }

    /// Emitted on Save with the controller-shaped map. `editingDeviceIdGuid`
    /// (when non-empty) is the row to delete first; MachineDetailsWindow
    /// dispatches to controller.replaceAgentPresenceWatchdog or add.
    signal confirmed(string editingDeviceIdGuid, var fields)

    title: isEdit ? qsTr("Edit watchdog") : qsTr("Add watchdog")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 520

    function _isValidGuid(s) {
        return /^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$/.test(s);
    }
    function _isValid() {
        return _isValidGuid(draftDeviceIdGuid)
            && draftTimeoutIntervalSec > 0;
    }
    function submitIfReady() {
        if (!_isValid()) return;
        const fields = {
            deviceIdGuid: draftDeviceIdGuid.trim(),
            description: draftDescription.trim(),
            monitoredEntityCode: draftMonitoredEntityCode,
            startupIntervalSec: draftStartupIntervalSec,
            timeoutIntervalSec: draftTimeoutIntervalSec,
        };
        root.confirmed(editingDeviceIdGuid, fields);
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
            title: qsTr("AGENT")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Device ID")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true
                    TextField {
                        placeholderText: "00000000-0000-0000-0000-000000000000"
                        text: root.draftDeviceIdGuid
                        // The DeviceID is the primary key on AMT's side;
                        // changing it mid-edit means "delete + add a
                        // different watchdog", which is what the user
                        // would do via Delete + Add anyway. Lock it on
                        // edit to keep the operation legible.
                        enabled: !root.isEdit
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftDeviceIdGuid = text
                        // #379 — pair the field with its label for AX.
                        Accessible.name: qsTr("Device ID")
                    }
                    FlatButton {
                        text: qsTr("Generate")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        enabled: !root.isEdit
                        onClicked: root.draftDeviceIdGuid = root._generateGuid()
                    }
                }

                Text {
                    text: qsTr("Description")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                TextField {
                    placeholderText: qsTr("backup daemon")
                    text: root.draftDescription
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftDescription = text
                    // #379 — pair the field with its label for AX.
                    Accessible.name: qsTr("Description")
                }

                Text {
                    text: qsTr("Monitored entity")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                ComboBox {
                    model: root.monitoredEntities
                    textRole: "label"
                    valueRole: "code"
                    currentIndex: root._entityIndexFor(root.draftMonitoredEntityCode)
                    Layout.fillWidth: true
                    onActivated: root.draftMonitoredEntityCode =
                        root.monitoredEntities[currentIndex].code
                    // #379 — pair the field with its label for AX.
                    Accessible.name: qsTr("Monitored entity")
                }
            }
        }

        Section {
            title: qsTr("INTERVALS")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Startup")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                RowLayout {
                    spacing: 6
                    SpinBox {
                        value: root.draftStartupIntervalSec
                        from: 0
                        to: 86400
                        editable: true
                        onValueModified: root.draftStartupIntervalSec = value
                    }
                    Text {
                        text: qsTr("seconds — grace window before the watchdog starts firing")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: qsTr("Timeout")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 130
                }
                RowLayout {
                    spacing: 6
                    SpinBox {
                        value: root.draftTimeoutIntervalSec
                        from: 1
                        to: 86400
                        editable: true
                        onValueModified: root.draftTimeoutIntervalSec = value
                    }
                    Text {
                        text: qsTr("seconds — how long without a heartbeat before the watchdog expires")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        Text {
            text: root.isEdit
                ? qsTr("AMT has no in-place edit for watchdogs; saving deletes the old row and creates a fresh one. Edit actions (reset / power off / event log on expiry) from the watchdog row's Actions button.")
                : qsTr("This registers the watchdog. Configure what it does on expiry (reset / power off / event log) from the watchdog row's Actions button after it's created.")
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
                text: root.isEdit ? qsTr("Save") : qsTr("Add")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root._isValid()
                onClicked: root.submitIfReady()
            }
        }
    }
}
