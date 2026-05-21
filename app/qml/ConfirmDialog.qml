// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Generic "are you sure?" prompt with a single proceed button. The
/// `proceedText` label is configurable so the caller can label the
/// destructive action (e.g. "Delete", "Disable") rather than a
/// generic OK. Use for any action that's hard to undo or that
/// touches a load-bearing entity (own account, admin, etc.).
Dialog {
    id: root

    property string promptTitle: qsTr("Confirm")
    property string body: qsTr("Are you sure?")
    property string proceedText: qsTr("Proceed")
    /// When true, the proceed button uses the standby colour to
    /// signal a destructive / lock-out-risk operation.
    property bool destructive: true

    signal proceed()

    function ask(t, b, action, destructiveFlag) {
        promptTitle = t;
        body = b;
        proceedText = action;
        destructive = destructiveFlag !== false;
        open();
    }

    title: promptTitle
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 460

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Text {
            text: root.body
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
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
            // Proceed button picks its style off `destructive`: a
            // red-tinted FlatButton for irreversible / lock-out-risk
            // actions, an AccentButton for benign confirms. Pre-#400
            // this was hardcoded to AccentButton regardless of the
            // `destructive` flag, which left every "Delete" confirm
            // landing the user's hand on a blue button — undoing the
            // red-tint sweep from #368 right where it matters most.
            Loader {
                sourceComponent: root.destructive
                    ? destructiveProceed
                    : accentProceed
            }
            Component {
                id: destructiveProceed
                FlatButton {
                    destructive: true
                    text: root.proceedText
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: {
                        root.proceed();
                        root.accept();
                    }
                }
            }
            Component {
                id: accentProceed
                AccentButton {
                    text: root.proceedText
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: {
                        root.proceed();
                        root.accept();
                    }
                }
            }
        }
    }
}
