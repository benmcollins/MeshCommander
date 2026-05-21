// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit a wake alarm (IPS_AlarmClockOccurrence). Edit = delete +
/// re-add since AMT has no Modify for this class; the controller does
/// the two-step. The interval is captured as D/H/M and serialized to
/// the ISO-8601 `PnDTnHnM` form the read side already parses.
/// See #347.
Dialog {
    id: root

    /// Set on openForEdit so the controller knows which row to drop
    /// before the new AddAlarm fires.
    property string editingInstanceId
    property bool isEdit: false

    property string draftElementName
    property string draftDate            // yyyy-MM-dd
    property string draftTime            // HH:mm
    property int draftDays: 0
    property int draftHours: 0
    property int draftMinutes: 0
    property bool draftDeleteOnCompletion: false

    function _todayLocal() {
        const d = new Date();
        const pad = (n) => (n < 10 ? "0" + n : "" + n);
        return d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate());
    }
    function _hourFromNow() {
        const d = new Date(Date.now() + 60 * 60 * 1000);
        const pad = (n) => (n < 10 ? "0" + n : "" + n);
        return pad(d.getHours()) + ":" + pad(d.getMinutes());
    }
    /// Parse `PnDTnHnM` (or `PnD`, `PTnHnM`, etc.) into D/H/M. Empty
    /// input leaves all three at 0 (= non-recurring).
    function _parseIsoDuration(iso) {
        if (!iso) return;
        const m = /^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?)?$/.exec(iso);
        if (!m) return;
        draftDays    = parseInt(m[1] || "0", 10);
        draftHours   = parseInt(m[2] || "0", 10);
        draftMinutes = parseInt(m[3] || "0", 10);
    }
    /// Build the ISO-8601 duration string. Returns "" if all three
    /// fields are zero — the controller / wsman builder treats empty
    /// as "non-recurring" and omits the Interval block on the wire.
    function _buildIsoDuration() {
        if (draftDays === 0 && draftHours === 0 && draftMinutes === 0)
            return "";
        return "P" + draftDays + "DT" + draftHours + "H" + draftMinutes + "M";
    }
    /// Combine date + time into a CIM datetime "yyyy-MM-ddTHH:mm:00Z".
    /// AMT accepts the raw local time + Z (the alarm fires at that
    /// wall-clock moment on the device); we trust the user-entered
    /// time as local-to-target.
    function _buildStartTimeIso() {
        if (!draftDate || !draftTime) return "";
        return draftDate + "T" + draftTime + ":00Z";
    }

    function openForAdd() {
        isEdit = false;
        editingInstanceId = "";
        draftElementName = "";
        draftDate = _todayLocal();
        draftTime = _hourFromNow();
        draftDays = 0;
        draftHours = 0;
        draftMinutes = 0;
        draftDeleteOnCompletion = false;
        open();
    }

    function openForEdit(row) {
        isEdit = true;
        editingInstanceId = row.instanceId || "";
        draftElementName = row.elementName || "";
        // The read side normalizes startTime to ISO-8601 UTC. Split
        // on "T" + drop seconds/zone for the editor; if AMT gave us
        // an unusable string fall back to today/now+1h.
        const iso = row.startTimeIso || "";
        const t = iso.indexOf("T");
        if (t > 0) {
            draftDate = iso.substring(0, t);
            const rest = iso.substring(t + 1);
            // strip seconds + "Z"/offset; keep HH:mm
            draftTime = rest.length >= 5 ? rest.substring(0, 5) : _hourFromNow();
        } else {
            draftDate = _todayLocal();
            draftTime = _hourFromNow();
        }
        draftDays = 0; draftHours = 0; draftMinutes = 0;
        _parseIsoDuration(row.intervalIso || "");
        draftDeleteOnCompletion = !!row.deleteOnCompletion;
        open();
    }

    /// Emitted on Save with the controller-shaped map. `editingInstanceId`
    /// (when non-empty) is the row to delete first; the MachineDetails
    /// window handles the two-step.
    signal confirmed(string editingInstanceId, var fields)

    title: isEdit ? qsTr("Edit wake alarm") : qsTr("Add wake alarm")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 480

    function _isValid() {
        return draftElementName.trim().length > 0
            && draftDate.length === 10
            && draftTime.length === 5;
    }
    function submitIfReady() {
        if (!_isValid()) return;
        const fields = {
            elementName: draftElementName.trim(),
            startTimeIso: _buildStartTimeIso(),
            intervalIso: _buildIsoDuration(),
            deleteOnCompletion: draftDeleteOnCompletion,
        };
        root.confirmed(editingInstanceId, fields);
        root.accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("ALARM")
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
                    Layout.preferredWidth: 110
                }
                TextField {
                    placeholderText: qsTr("Nightly wake")
                    text: root.draftElementName
                    enabled: !root.isEdit
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftElementName = text
                }

                Text {
                    text: qsTr("Date")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    placeholderText: "yyyy-MM-dd"
                    text: root.draftDate
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    inputMask: "9999-99-99"
                    Layout.preferredWidth: 160
                    onTextEdited: root.draftDate = text
                }

                Text {
                    text: qsTr("Time")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    placeholderText: "HH:mm"
                    text: root.draftTime
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    inputMask: "99:99"
                    Layout.preferredWidth: 100
                    onTextEdited: root.draftTime = text
                }
            }
        }

        Section {
            title: qsTr("RECURRENCE")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true

                Text {
                    text: qsTr("Leave at 0 for a one-shot alarm.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }

                RowLayout {
                    spacing: 16
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: qsTr("Days")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                        SpinBox {
                            value: root.draftDays
                            from: 0
                            to: 365
                            editable: true
                            onValueModified: root.draftDays = value
                        }
                    }
                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: qsTr("Hours")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                        SpinBox {
                            value: root.draftHours
                            from: 0
                            to: 23
                            editable: true
                            onValueModified: root.draftHours = value
                        }
                    }
                    ColumnLayout {
                        spacing: 4
                        Text {
                            text: qsTr("Minutes")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                        SpinBox {
                            value: root.draftMinutes
                            from: 0
                            to: 59
                            editable: true
                            onValueModified: root.draftMinutes = value
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                CheckBox {
                    text: qsTr("Delete after firing")
                    checked: root.draftDeleteOnCompletion
                    onToggled: root.draftDeleteOnCompletion = checked
                }
            }
        }

        Text {
            text: root.isEdit
                ? qsTr("AMT has no in-place edit for alarms; saving deletes the old row and creates a fresh one.")
                : qsTr("The alarm fires at the wall-clock moment shown above on the target device.")
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
