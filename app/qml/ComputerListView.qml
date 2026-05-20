// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

Rectangle {
    id: root

    readonly property int currentRow: list.currentIndex
    // Multi-select state for batch actions (#202). When `selectMode`
    // is true, each row shows a checkbox before its content and the
    // single-click handler toggles selection instead of changing
    // `currentRow`. `selectedRows` is exposed as a read-only list of
    // row indices the parent (Main.qml) hands to BatchController.start.
    property bool selectMode: false
    readonly property var selectedRows: Array.from(_selectedSet.values())
    readonly property int selectedCount: _selectedSet.size

    // Backing set for `selectedRows`. Stored as a JS Set so toggle
    // is O(1) and we can derive `selectedCount` without a re-walk.
    property var _selectedSet: new Set()

    function setCurrentRow(row) { list.currentIndex = row; }

    function toggleSelectMode() {
        selectMode = !selectMode;
        if (!selectMode) clearSelection();
    }

    function clearSelection() {
        _selectedSet = new Set();
        _selectedSetChanged();
    }

    function toggleSelected(row) {
        const s = new Set(_selectedSet);
        if (s.has(row)) s.delete(row); else s.add(row);
        _selectedSet = s;
    }

    signal addRequested
    signal scanRequested
    signal openDetailsRequested(int row)
    signal localAmtRequested
    /// Emitted when the user picks a batch action from the menu while
    /// in `selectMode`. `action` is a `BatchAction` enum int from C++.
    signal batchActionRequested(int action, var rows)

    color: Colors.bg

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        // "This PC" affordance, surfaced when Intel LMS is proxying
        // the loopback ports on this machine. Hidden silently on hosts
        // without LMS (most macOS boxes, vPro-less hardware) so the
        // sidebar stays clean. Clicking the row opens
        // LocalAmtPromptDialog for credentials.
        Rectangle {
            visible: LocalAmtProbe.available
            color: localHover.hovered ? Colors.elevated : "transparent"
            Layout.fillWidth: true
            implicitHeight: 44

            Behavior on color { ColorAnimation { duration: Motion.fast } }

            HoverHandler { id: localHover }
            TapHandler { onTapped: root.localAmtRequested() }

            Rectangle {
                color: Colors.accent
                implicitWidth: 2
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }

            ColumnLayout {
                spacing: 1
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                anchors.topMargin: 4
                anchors.bottomMargin: 4

                Text {
                    text: qsTr("This PC")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeM
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: LocalAmtProbe.tlsAvailable
                        ? qsTr("Intel AMT — local TLS")
                        : qsTr("Intel AMT — local")
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                color: Colors.border
                height: 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }
        }

        ListView {
            id: list
            model: ComputerModel
            clip: true
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: Motion.fast
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollBar.vertical: ScrollBar {}

            // Stagger entrance for the first paint and any subsequent
            // inserts. Items fade in + slide up slightly so a fresh
            // model doesn't all appear at once.
            // `ViewTransition.index` is the row's position so each
            // delegate kicks off ~40 ms after the previous one.
            populate: Transition {
                SequentialAnimation {
                    PauseAnimation { duration: ViewTransition.index * 40 }
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
                        NumberAnimation { property: "y"; from: ViewTransition.item.y + 8; to: ViewTransition.item.y; duration: Motion.normal; easing.type: Easing.OutCubic }
                    }
                }
            }
            add: Transition {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
                    NumberAnimation { property: "y"; from: ViewTransition.item.y + 8; to: ViewTransition.item.y; duration: Motion.normal; easing.type: Easing.OutCubic }
                }
            }
            displaced: Transition {
                NumberAnimation { properties: "y"; duration: Motion.fast }
            }

            delegate: Rectangle {
                id: rowItem

                required property int index
                required property string name
                required property string host

                readonly property bool isSelected: root._selectedSet.has(rowItem.index)

                width: list.width
                height: 48
                color: list.currentIndex === rowItem.index
                       ? Colors.accentSoft
                       : (rowItem.isSelected
                           ? Colors.accentSoft
                           : (hoverHandler.hovered ? Colors.elevated : "transparent"))

                Behavior on color { ColorAnimation { duration: Motion.fast } }

                HoverHandler { id: hoverHandler }
                TapHandler {
                    onTapped: {
                        if (root.selectMode) {
                            root.toggleSelected(rowItem.index);
                        } else {
                            list.currentIndex = rowItem.index;
                        }
                    }
                    onDoubleTapped: {
                        if (!root.selectMode) root.openDetailsRequested(rowItem.index);
                    }
                }

                Rectangle {
                    color: Colors.accent
                    implicitWidth: 2
                    visible: list.currentIndex === rowItem.index && !root.selectMode
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                }

                // Selection checkbox. Visible only in multi-select mode;
                // visually anchored at the row's left edge so the
                // ColumnLayout content shifts right when select-mode
                // engages.
                CheckBox {
                    id: selectCheck
                    visible: root.selectMode
                    checked: rowItem.isSelected
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    onToggled: root.toggleSelected(rowItem.index)
                }

                ColumnLayout {
                    spacing: 1
                    anchors.fill: parent
                    anchors.leftMargin: root.selectMode ? 38 : 16
                    anchors.rightMargin: 12
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6

                    Text {
                        text: rowItem.name.length > 0
                              ? rowItem.name
                              : qsTr("Unnamed")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: rowItem.host
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    color: Colors.borderMuted
                    height: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                }
            }

            Text {
                visible: list.count === 0
                opacity: 0.6
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No machines yet.\nClick + to add one.")
                anchors.centerIn: parent
            }
        }

        Rectangle {
            color: Colors.border
            implicitHeight: 1
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.margins: 8

            Button {
                text: "+"
                font.family: Type.mono
                font.pixelSize: Type.sizeL
                implicitWidth: 32
                implicitHeight: 28
                ToolTip.text: qsTr("Add machine")
                ToolTip.visible: hovered
                onClicked: root.addRequested()
            }

            Button {
                text: "−"
                font.family: Type.mono
                font.pixelSize: Type.sizeL
                implicitWidth: 32
                implicitHeight: 28
                enabled: list.currentIndex >= 0
                ToolTip.text: qsTr("Remove selected")
                ToolTip.visible: hovered
                // Pre-#278 this called removeAt() directly — irreversible,
                // no warning. Now route through the same ConfirmDialog
                // the per-machine details pane already uses for delete.
                onClicked: {
                    const row = list.currentIndex;
                    const name = ComputerModel.data(
                        ComputerModel.index(row, 0), ComputerModel.NameRole) || "";
                    confirmRemove.pendingRow = row;
                    confirmRemove.ask(
                        qsTr("Delete machine?"),
                        qsTr("This removes \"%1\" and its saved credentials. "
                             + "This cannot be undone.").arg(name),
                        qsTr("Delete"), true);
                }
            }

            Button {
                text: qsTr("Scan…")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                implicitHeight: 28
                ToolTip.text: qsTr("Discover AMT devices on a subnet")
                ToolTip.visible: hovered
                onClicked: root.scanRequested()
            }

            // Multi-select toggle (#202). Off by default; flips to
            // "Done" while engaged so the affordance to leave the mode
            // is obvious. Acts as the carrier for the Actions menu —
            // we don't surface that menu at all outside select mode.
            Button {
                id: selectBtn
                checkable: true
                checked: root.selectMode
                text: root.selectMode ? qsTr("Done") : qsTr("Select")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                implicitHeight: 28
                ToolTip.text: qsTr("Pick multiple machines to act on at once")
                ToolTip.visible: hovered
                onClicked: root.toggleSelectMode()
            }

            Button {
                id: actionsBtn
                visible: root.selectMode
                enabled: root.selectedCount > 0
                text: qsTr("Actions ▾")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                implicitHeight: 28
                ToolTip.text: root.selectedCount === 0
                    ? qsTr("Tick at least one row")
                    : qsTr("Run an action on %1 selected").arg(root.selectedCount)
                ToolTip.visible: hovered
                onClicked: actionsMenu.open()

                Menu {
                    id: actionsMenu
                    y: -actionsMenu.height
                    Menu {
                        title: qsTr("Power")
                        MenuItem {
                            text: qsTr("Power on")
                            // BatchAction enum values from batchrunner.h:
                            //   PowerOn=1, PowerOffSoft=2, PowerOffHard=3,
                            //   PowerReset=4, PowerCycle=5, SyncClock=6,
                            //   ClearAuditLog=7, ClearEventLog=8.
                            onTriggered: root.batchActionRequested(1, root.selectedRows)
                        }
                        MenuItem {
                            text: qsTr("Power off (soft)")
                            onTriggered: root.batchActionRequested(2, root.selectedRows)
                        }
                        MenuItem {
                            text: qsTr("Power off (hard)")
                            onTriggered: root.batchActionRequested(3, root.selectedRows)
                        }
                        MenuItem {
                            text: qsTr("Reset")
                            onTriggered: root.batchActionRequested(4, root.selectedRows)
                        }
                        MenuItem {
                            text: qsTr("Power cycle")
                            onTriggered: root.batchActionRequested(5, root.selectedRows)
                        }
                    }
                    MenuItem {
                        text: qsTr("Sync clock")
                        onTriggered: root.batchActionRequested(6, root.selectedRows)
                    }
                    MenuItem {
                        text: qsTr("Clear audit log")
                        onTriggered: root.batchActionRequested(7, root.selectedRows)
                    }
                    MenuItem {
                        text: qsTr("Clear event log")
                        onTriggered: root.batchActionRequested(8, root.selectedRows)
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: root.selectMode
                text: root.selectedCount === 1
                      ? qsTr("1 selected")
                      : qsTr("%1 selected").arg(root.selectedCount)
                color: Colors.textMuted
                font.family: Type.mono
                font.pixelSize: Type.sizeXs
            }

            Text {
                visible: !root.selectMode
                text: list.count === 1
                      ? qsTr("1 machine")
                      : qsTr("%1 machines").arg(list.count)
                color: Colors.textFaint
                font.family: Type.mono
                font.pixelSize: Type.sizeXs
            }
        }
    }

    /// Confirmation prompt for the sidebar `−` delete button (#278).
    /// Stores the row to remove on `pendingRow` so the proceed handler
    /// doesn't rely on a possibly-changed `list.currentIndex`.
    ConfirmDialog {
        id: confirmRemove
        property int pendingRow: -1
        onProceed: {
            if (pendingRow >= 0) ComputerModel.removeAt(pendingRow);
            pendingRow = -1;
        }
        onRejected: pendingRow = -1
    }
}
