// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal status panel for a running batch action (#202). Bound to the
/// `BatchController` singleton; opens when a batch starts, lets the
/// user watch per-host status land, and offers a "Retry failed" once
/// the batch reaches the idle state.
///
/// Not a Drawer — Drawer's gesture model is heavy for what's basically
/// a "watch this finish, then close" panel. A Dialog with custom
/// chrome is lighter and reads more like the existing prompt dialogs
/// (HttpsBootPrompt / PlatformErasePrompt).
Dialog {
    id: root

    /// Localized action label shown in the header (e.g. "Power on").
    /// Caller sets this on `open()`; falls back to a generic label.
    property string actionLabel: qsTr("Batch action")

    /// `ComputerModel` ref for the host-name lookup on each row. QML
    /// can't import the C++ model directly because the dialog is
    /// generic, so the parent injects it.
    property var computerModel: ComputerModel

    title: qsTr("Batch — %1").arg(actionLabel)
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    closePolicy: BatchController.running ? Popup.NoAutoClose
                                          : (Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent)
    implicitWidth: 560
    implicitHeight: 460

    contentItem: ColumnLayout {
        spacing: 10

        // Header — summary counts as the batch progresses.
        RowLayout {
            spacing: 16
            Layout.fillWidth: true

            Text {
                text: BatchController.running
                    ? qsTr("Running — %1 of %2 done")
                        .arg(BatchController.doneCount + BatchController.failedCount + BatchController.skippedCount)
                        .arg(BatchController.totalJobs)
                    : qsTr("Finished — %1 ok, %2 failed, %3 skipped")
                        .arg(BatchController.doneCount)
                        .arg(BatchController.failedCount)
                        .arg(BatchController.skippedCount)
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
            }

            // Tiny activity indicator while a job is still in-flight.
            BusyIndicator {
                visible: BatchController.running
                running: BatchController.running
                implicitWidth: 18
                implicitHeight: 18
            }
        }

        // Per-host status rows.
        ListView {
            id: jobList
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2
            model: BatchController.jobs

            delegate: Rectangle {
                id: jobRow
                required property int index
                required property var modelData

                width: jobList.width
                implicitHeight: 30
                color: jobRow.modelData.status === 3 // DoneFailed
                    ? Qt.rgba(0.85, 0.25, 0.25, 0.10)
                    : (jobRow.modelData.status === 4 ? Qt.rgba(0.55, 0.55, 0.55, 0.10) : "transparent")

                RowLayout {
                    spacing: 10
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10

                    Text {
                        // ComputerModel exposes NameRole at UserRole+1+1.
                        // Look up the host name via the same role used by
                        // the sidebar; if the row was deleted mid-batch
                        // we surface the raw row index.
                        text: {
                            const idx = root.computerModel.index(jobRow.modelData.row, 0);
                            const nm = root.computerModel.data(idx,
                                root.computerModel.NameRole) || "";
                            const hostName = root.computerModel.data(idx,
                                root.computerModel.HostRole) || "";
                            const label = nm.length > 0 ? nm : hostName;
                            return label.length > 0
                                ? label
                                : qsTr("Row %1").arg(jobRow.modelData.row);
                        }
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: BatchController.statusLabel(jobRow.modelData.status)
                        color: {
                            switch (jobRow.modelData.status) {
                            case 2: return Colors.on; // DoneOk
                            case 3: return "#E5484D"; // DoneFailed
                            case 4: return Colors.textMuted; // Skipped
                            case 1: return Colors.accent; // Running
                            default: return Colors.textMuted; // Pending
                            }
                        }
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        font.weight: Font.Medium
                    }

                    Text {
                        visible: jobRow.modelData.detail
                              && jobRow.modelData.detail.length > 0
                        text: jobRow.modelData.detail || ""
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.preferredWidth: 220
                    }
                }
            }
        }

        // Footer — retry / close buttons.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FlatButton {
                text: qsTr("Retry failed (%1)").arg(BatchController.failedCount)
                visible: BatchController.failedCount > 0
                enabled: !BatchController.running
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: BatchController.retryFailed()
            }

            Item { Layout.fillWidth: true }

            AccentButton {
                text: BatchController.running ? qsTr("Hide") : qsTr("Close")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.close()
            }
        }
    }
}
