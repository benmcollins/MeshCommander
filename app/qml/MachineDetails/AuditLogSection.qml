// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Audit log" section (was the inline section 11
/// before #325). Pulled into its own file — see OverviewSection.qml
/// for the rationale.
ColumnLayout {
    id: root

    required property MachineDetailsController controller

    spacing: 8


    SectionHeader {
        eyebrow: qsTr("AUDIT LOG")
        title: {
            const s = controller.auditLogState;
            if (!s || !s.ok)
                return controller.auditLogEntries.length === 0
                    ? (controller.busy ? qsTr("Loading…") : qsTr("No entries"))
                    : qsTr("%1 entries").arg(controller.auditLogEntries.length);
            let parts = [];
            parts.push(s.enabled ? qsTr("Enabled") : qsTr("Disabled"));
            if (s.locked)     parts.push(qsTr("Locked"));
            if (s.full)       parts.push(qsTr("Full"));
            else if (s.almostFull) parts.push(qsTr("Almost full"));
            if (s.noSigningKey) parts.push(qsTr("No signing key"));
            return parts.join(" · ");
        }

        Text {
            visible: controller.auditLogState
                  && controller.auditLogState.ok === true
            text: qsTr("%1 records · %2%% free")
                .arg(controller.auditLogState.currentNumberOfRecords || 0)
                .arg(controller.auditLogState.percentageFree || 0)
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
        }
        // Show "Loading…" while the fetch is in flight (MachineDetailsWindow
        // auto-fires it on section switch) and the section-specific empty
        // noun once the response comes back empty. Telling the user to
        // click Refresh while the app is already fetching is misleading
        // (#378).
        Text {
            visible: controller.auditLogEntries.length === 0
            text: controller.busy ? qsTr("Loading…") : qsTr("No entries")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
        }
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.bottomMargin: 24
        clip: true
        model: controller.auditLogEntries
        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            id: auditRow
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: 36
            color: index % 2 === 0 ? "transparent" : Colors.elevated

            // Make each audit-log row addressable to screen readers (#379).
            // Mirror the ComputerListView pattern.
            Accessible.role: Accessible.ListItem
            Accessible.name: qsTr("%1, %2, %3, %4")
                .arg(auditRow.modelData.unixSeconds > 0
                    ? Qt.formatDateTime(
                        new Date(auditRow.modelData.unixSeconds * 1000),
                        "yyyy-MM-dd HH:mm:ss")
                    : "—")
                .arg(auditRow.modelData.initiator || "—")
                .arg(auditRow.modelData.eventLabel || "—")
                .arg(auditRow.modelData.netAddress || "")

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12

                Text {
                    text: modelData.unixSeconds > 0
                        ? Qt.formatDateTime(
                            new Date(modelData.unixSeconds * 1000),
                            "yyyy-MM-dd  HH:mm:ss")
                        : "—"
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 160
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.initiator || "—"
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 140
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.eventLabel || "—"
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.netAddress || ""
                    color: Colors.textFaint
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 110
                    elide: Text.ElideRight
                }
            }
        }
    }
}
