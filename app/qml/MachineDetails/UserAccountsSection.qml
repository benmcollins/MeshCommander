// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "User accounts" section (was the inline
/// section 12 before #325). Threads through the two dialogs the
/// section's controls open — `userAccountDialog` for add/edit,
/// `adminPasswordDialog` for the admin-rotate flow. See
/// OverviewSection.qml for the file-extract rationale.
ColumnLayout {
    id: root

    required property MachineDetailsController controller
    required property var userAccountDialog
    required property var adminPasswordDialog

    spacing: 8


    // Hidden-account toggle drives the model filter
    // below. AMT marks internal accounts with names
    // starting in `$$` (e.g. `$$OsAdmin`); operators
    // rarely want to see them.
    property bool showHidden: false
    function filteredAccounts() {
        if (showHidden) return controller.userAccounts;
        return (controller.userAccounts || []).filter(a => !a.hidden);
    }

    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true
        Layout.topMargin: 24
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        Text {
            text: qsTr("USER ACCOUNTS")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Text {
                text: controller.userAccounts.length === 0
                    ? qsTr("No accounts.")
                    : qsTr("%1 accounts")
                        .arg(parent.parent.parent.filteredAccounts().length)
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: 20
                Layout.fillWidth: true
            }
            CheckBox {
                text: qsTr("Show hidden ($$)")
                checked: parent.parent.parent.showHidden
                onToggled: parent.parent.parent.showHidden = checked
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            FlatButton {
                text: qsTr("Rotate admin…")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                onClicked: {
                    // Default the dialog's username to the
                    // admin row we already enumerated, if
                    // any — otherwise it stays blank.
                    let adminUser = "";
                    for (let i = 0; i < (controller.userAccounts || []).length; ++i) {
                        const a = controller.userAccounts[i];
                        if (a.handle === -1) {
                            adminUser = a.digestUsername || "";
                            break;
                        }
                    }
                    adminPasswordDialog.openForRotate(adminUser);
                }
            }
            AccentButton {
                text: qsTr("Add account…")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                onClicked: userAccountDialog.openForAdd()
            }
        }
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.bottomMargin: 24
        clip: true
        model: parent.filteredAccounts()
        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: 56
            color: index % 2 === 0 ? "transparent" : Colors.elevated
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 2

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    Text {
                        text: modelData.name || qsTr("(unnamed)")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        visible: modelData.handle === -1
                        text: qsTr("ADMIN")
                        color: Colors.accent
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        font.weight: Font.Medium
                        font.letterSpacing: 1
                    }
                    Text {
                        visible: modelData.isKerberos === true
                        text: qsTr("KERBEROS")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        font.weight: Font.Medium
                        font.letterSpacing: 1
                    }
                    Text {
                        visible: !modelData.enabled
                        text: qsTr("DISABLED")
                        color: Colors.standby
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        font.weight: Font.Medium
                        font.letterSpacing: 1
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: {
                            if (modelData.handle === -1)
                                return qsTr("Administrator (full access)");
                            if (modelData.isAdmin)
                                return modelData.accessPermissionLabel
                                    + " · " + qsTr("Administrator");
                            const r = modelData.realmsLabel || "";
                            return r.length > 0
                                ? modelData.accessPermissionLabel + " · " + r
                                : modelData.accessPermissionLabel;
                        }
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // Per-row actions. Admin entry only
                    // supports rotation via the top-bar
                    // button so the row actions are hidden.
                    FlatButton {
                        visible: modelData.handle !== -1
                                 && !modelData.isKerberos
                        text: qsTr("Edit")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: userAccountDialog.openForEdit(modelData)
                    }
                    FlatButton {
                        visible: modelData.handle !== -1
                        text: modelData.enabled
                            ? qsTr("Disable")
                            : qsTr("Enable")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        // Only tint when the visible label is "Disable" —
                        // re-enabling an account is not destructive.
                        destructive: modelData.enabled
                        onClicked: {
                            const isOwnRow = modelData.digestUsername
                                === controller.user;
                            if (modelData.enabled && isOwnRow) {
                                confirmDialog.ask(
                                    qsTr("Disable your own account?"),
                                    qsTr("This will lock your current AMT session out. You'll need another account to reconnect."),
                                    qsTr("Disable anyway"),
                                    true);
                                confirmDialog.pendingHandle = modelData.handle;
                                confirmDialog.pendingAction = "disable";
                            } else {
                                controller.setAccountEnabled(
                                    modelData.handle, !modelData.enabled);
                            }
                        }
                    }
                    FlatButton {
                        visible: modelData.handle !== -1
                        text: qsTr("Delete")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        destructive: true
                        onClicked: {
                            const isOwnRow = modelData.digestUsername
                                === controller.user;
                            confirmDialog.ask(
                                isOwnRow
                                    ? qsTr("Delete your own account?")
                                    : qsTr("Delete user account?"),
                                isOwnRow
                                    ? qsTr("Removing %1 will lock this AMT session out immediately. You'll need another account to reconnect.").arg(modelData.name)
                                    : qsTr("Removing %1 cannot be undone — the AMT handle is freed and any clients using it will fail to authenticate.").arg(modelData.name),
                                qsTr("Delete"),
                                true);
                            confirmDialog.pendingHandle = modelData.handle;
                            confirmDialog.pendingAction = "delete";
                        }
                    }
                }
            }
        }

        Text {
            visible: controller.userAccounts.length === 0 && !controller.busy
            anchors.centerIn: parent
            text: qsTr("No user accounts.")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeS
        }
    }
}
