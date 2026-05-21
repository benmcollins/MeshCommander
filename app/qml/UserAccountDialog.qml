// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit a non-admin AMT user account. `handle === -1` toggles
/// add mode; any other value edits the existing handle. Realms are a
/// checkbox grid keyed off the legacy AMT realm-bit indices the read
/// side surfaces. Save calls controller.addUserAccount /
/// updateUserAccount with a patch built from the form deltas.
///
/// Admin rotation is a separate dialog — AMT exposes
/// `SetAdminAclEntryEx` for that and doesn't allow realm changes on
/// the admin entry.
Dialog {
    id: root

    required property MachineDetailsController controller

    property int handle: -1
    readonly property bool isAdd: handle === -1

    property string draftUsername
    property string draftPassword
    property string draftPasswordConfirm
    property bool revealPassword: false
    /// 0 = Local, 1 = Network, 2 = Both.
    property int draftAccessPermission: 2
    /// Checkbox state — keyed by realm bit-index → bool.
    property var draftRealms: ({})

    // Snapshot for dirty-detection on Save.
    property string initialUsername
    property int initialAccessPermission: 2
    property var initialRealms: ({})

    // The CIM realm bit indices that AMT exposes. Bit 3 is the
    // load-bearing "Administrator" bit; granting it gives full
    // access regardless of the others. The labels come from
    // wsman::realmName() via the existing read side, but the bit
    // list itself is stable so we render it client-side here.
    readonly property var realmCatalogue: [
        { bit: 0,  label: qsTr("Redirection") },
        { bit: 1,  label: qsTr("PT Administration") },
        { bit: 2,  label: qsTr("Hardware Asset") },
        { bit: 3,  label: qsTr("Administrator") },
        { bit: 4,  label: qsTr("Storage") },
        { bit: 5,  label: qsTr("Event Manager") },
        { bit: 6,  label: qsTr("Storage Admin") },
        { bit: 7,  label: qsTr("Agent Presence Local") },
        { bit: 8,  label: qsTr("Agent Presence Remote") },
        { bit: 9,  label: qsTr("Circuit Breaker") },
        { bit: 10, label: qsTr("Network Time") },
        { bit: 11, label: qsTr("General Information") },
        { bit: 12, label: qsTr("Firmware Update") },
        { bit: 13, label: qsTr("EIT") },
        { bit: 14, label: qsTr("Local System") },
        { bit: 15, label: qsTr("Endpoint Access Control") },
        { bit: 16, label: qsTr("Endpoint Access Control Admin") },
        { bit: 17, label: qsTr("Event Log Reader") },
        { bit: 18, label: qsTr("Audit Log") },
        { bit: 19, label: qsTr("ACL Realm") },
        { bit: 23, label: qsTr("Local Agent") },
        { bit: 24, label: qsTr("User Access Control") },
    ]

    function openForAdd() {
        handle = -1;
        draftUsername = "";
        draftPassword = "";
        draftPasswordConfirm = "";
        draftAccessPermission = 2;
        draftRealms = {};
        initialUsername = "";
        initialAccessPermission = 2;
        initialRealms = {};
        revealPassword = false;
        open();
    }

    function openForEdit(account) {
        handle = account.handle;
        draftUsername = account.digestUsername || "";
        draftPassword = "";
        draftPasswordConfirm = "";
        draftAccessPermission = account.accessPermission;
        // Convert the realms list (an array of bit-indices) into a
        // bool-keyed map for the checkboxes.
        const r = {};
        for (let i = 0; i < (account.realms || []).length; ++i)
            r[account.realms[i]] = true;
        draftRealms = r;
        initialUsername = draftUsername;
        initialAccessPermission = draftAccessPermission;
        initialRealms = JSON.parse(JSON.stringify(r));
        revealPassword = false;
        open();
    }

    title: isAdd ? qsTr("Add user account") : qsTr("Edit user account")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 640
    implicitHeight: Math.min(formColumn.implicitHeight + 80,
                              (parent ? parent.height : 720) * 0.85)

    function passwordsAgree() {
        return draftPassword.length > 0
               && draftPassword === draftPasswordConfirm;
    }

    function realmsArray() {
        const out = [];
        for (const key in draftRealms)
            if (draftRealms[key]) out.push(parseInt(key));
        return out;
    }

    function realmsChanged() {
        const a = realmsArray().sort();
        const b = [];
        for (const key in initialRealms)
            if (initialRealms[key]) b.push(parseInt(key));
        b.sort();
        if (a.length !== b.length) return true;
        for (let i = 0; i < a.length; ++i)
            if (a[i] !== b[i]) return true;
        return false;
    }

    function commit() {
        if (isAdd) {
            root.controller.addUserAccount({
                "digestUsername":   root.draftUsername,
                "password":         root.draftPassword,
                "accessPermission": root.draftAccessPermission,
                "realms":           root.realmsArray(),
            });
            root.accept();
            return;
        }
        const patch = {};
        if (root.draftUsername !== root.initialUsername)
            patch.digestUsername = root.draftUsername;
        if (root.draftPassword.length > 0)
            patch.password = root.draftPassword;
        if (root.draftAccessPermission !== root.initialAccessPermission)
            patch.accessPermission = root.draftAccessPermission;
        if (root.realmsChanged())
            patch.realms = root.realmsArray();
        if (Object.keys(patch).length === 0) {
            root.reject();
            return;
        }
        root.controller.updateUserAccount(root.handle, patch);
        root.accept();
    }

    /// Centralised primary-action gate so the AccentButton and the
    /// Enter-key handler stay in sync — #279. Returns true if the
    /// form would be acceptable (matches AccentButton's enabled
    /// condition).
    function submitIfReady() {
        if (root.draftUsername.length === 0) return;
        if (root.isAdd ? !root.passwordsAgree()
                       : (root.draftPassword.length > 0 && !root.passwordsAgree())) return;
        root.commit();
    }

    contentItem: ScrollView {
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        contentWidth: availableWidth

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ScrollView).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        ColumnLayout {
            id: formColumn
            width: parent ? parent.width : 0
            spacing: 14

            Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
            Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

            Section {
                title: qsTr("IDENTITY")
                Layout.fillWidth: true

                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Username")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 110
                    }
                    TextField {
                        placeholderText: qsTr("operator")
                        text: root.draftUsername
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftUsername = text
                        // #379 — pair the field with its label for AX.
                        Accessible.name: qsTr("Username")
                    }

                    Text {
                        text: qsTr("Password")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 110
                    }
                    RowLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        TextField {
                            echoMode: root.revealPassword ? TextInput.Normal : TextInput.Password
                            placeholderText: root.isAdd
                                ? qsTr("Required")
                                : qsTr("Leave blank to keep current")
                            text: root.draftPassword
                            font.family: Type.mono
                            font.pixelSize: Type.sizeM
                            Layout.fillWidth: true
                            onTextEdited: root.draftPassword = text
                            // #379 — pair the field with its label for AX.
                            Accessible.name: qsTr("Password")
                        }
                        FlatButton {
                            text: root.revealPassword ? qsTr("Hide") : qsTr("Show")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: root.revealPassword = !root.revealPassword
                        }
                    }

                    Text {
                        text: qsTr("Confirm")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        visible: root.draftPassword.length > 0
                        Layout.preferredWidth: 110
                    }
                    TextField {
                        visible: root.draftPassword.length > 0
                        echoMode: root.revealPassword ? TextInput.Normal : TextInput.Password
                        text: root.draftPasswordConfirm
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        color: root.passwordsAgree() ? Colors.text : Colors.standby
                        Layout.fillWidth: true
                        onTextEdited: root.draftPasswordConfirm = text
                        // #379 — pair the field with its label for AX.
                        Accessible.name: qsTr("Confirm password")
                    }
                }
            }

            Section {
                title: qsTr("ACCESS")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 10
                    Layout.fillWidth: true

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 10
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Where")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.preferredWidth: 110
                        }
                        RowLayout {
                            spacing: 12
                            Layout.fillWidth: true
                            RadioButton {
                                text: qsTr("Local only")
                                checked: root.draftAccessPermission === 0
                                onClicked: root.draftAccessPermission = 0
                            }
                            RadioButton {
                                text: qsTr("Network only")
                                checked: root.draftAccessPermission === 1
                                onClicked: root.draftAccessPermission = 1
                            }
                            RadioButton {
                                text: qsTr("Both")
                                checked: root.draftAccessPermission === 2
                                onClicked: root.draftAccessPermission = 2
                            }
                        }
                    }

                    Text {
                        text: qsTr("Realms — granting Administrator (bit 3) gives full access regardless of the others.")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 4
                        Layout.fillWidth: true
                        Repeater {
                            model: root.realmCatalogue
                            delegate: CheckBox {
                                required property var modelData
                                text: qsTr("[%1] %2")
                                    .arg(modelData.bit).arg(modelData.label)
                                checked: root.draftRealms[modelData.bit] === true
                                onToggled: {
                                    const r = Object.assign({}, root.draftRealms);
                                    if (checked) r[modelData.bit] = true;
                                    else delete r[modelData.bit];
                                    root.draftRealms = r;
                                }
                            }
                        }
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
                    text: root.isAdd ? qsTr("Add account") : qsTr("Save changes")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: {
                        if (root.draftUsername.length === 0) return false;
                        // Add requires a confirmed password; Edit
                        // allows an empty password (means "keep
                        // current"), but if filled it must match.
                        if (root.isAdd) return root.passwordsAgree();
                        if (root.draftPassword.length === 0) return true;
                        return root.passwordsAgree();
                    }
                    onClicked: root.submitIfReady()
                }
            }
        }
    }
}
