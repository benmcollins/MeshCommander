// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal "Trust this SSH host key?" prompt — the per-machine SSH-tunnel
/// equivalent of CertTrustDialog. Driven by a controller exposing
/// `awaitingSshHostKeyTrust`, `pendingSshHostKeyFingerprint`, and
/// `pendingSshHostKeyType`, and resolved via the controller's
/// `trustPendingSshHostKey(bool)` invokable.
///
/// Pre-#270 the C++ side silently auto-trusted every new key (TOFU
/// every time the server's key changed) — this dialog now gates that
/// decision behind a user click. Refusing closes the dialog without
/// resuming the SSH session; the caller is responsible for tearing it
/// down (typically by calling `setSshConfig({ enabled: false })`).
Dialog {
    id: root

    required property QtObject controller
    /// Caller invokes this on Cancel so the SSH session is torn down.
    /// Default is a no-op; bind to e.g. `controller.setSshConfig({})`
    /// or window-level dismiss logic.
    signal cancelled()

    title: qsTr("Untrusted SSH host key")
    modal: true
    closePolicy: Popup.NoAutoClose
    visible: controller && controller.awaitingSshHostKeyTrust
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        spacing: 10
        Layout.preferredWidth: 460

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Text {
            text: qsTr("The SSH jump host presented a key we haven't seen "
                       + "before. Verify the fingerprint out-of-band before "
                       + "trusting — accepting a forged key lets an attacker "
                       + "in the path intercept every byte to AMT.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.preferredWidth: 460
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 4
            Layout.fillWidth: true
            Layout.topMargin: 6

            Text {
                text: qsTr("Key type")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                Layout.alignment: Qt.AlignTop
            }
            Text {
                text: root.controller ? root.controller.pendingSshHostKeyType : ""
                color: Colors.text
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("SHA-256")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                Layout.alignment: Qt.AlignTop
            }
            Text {
                text: root.controller ? root.controller.pendingSshHostKeyFingerprint : ""
                color: Colors.text
                font.family: Type.mono
                font.pixelSize: Type.sizeMicro
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }
        }

        CheckBox {
            id: persistCheck
            checked: true
            text: qsTr("Remember this fingerprint for future connections")
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            Layout.fillWidth: true
            // #423 — pair the checkbox with its label for AX.
            Accessible.name: qsTr("Remember this fingerprint")
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.topMargin: 6

            Item { Layout.fillWidth: true }
            FlatButton {
                text: qsTr("Cancel")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.cancelled()
            }
            AccentButton {
                text: qsTr("Trust")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: if (root.controller)
                    root.controller.trustPendingSshHostKey(persistCheck.checked)
            }
        }
    }
}
