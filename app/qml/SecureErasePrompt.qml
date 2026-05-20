// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal dialog that asks for the AMT Remote Secure Erase password
/// and resolves into either `accepted(password, reset)` or `rejected`.
/// `tlsActive` flips on the red "password will be sent in the clear"
/// warning when the WSMAN session isn't TLS-pinned.
Dialog {
    id: root

    /// `true` when the caller wants the host reset; `false` for power-on.
    property bool resetFlavour: false
    property bool tlsActive: true

    signal confirmed(string password, bool reset)

    title: qsTr("Confirm Secure Erase")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.Cancel | Dialog.Yes
    implicitWidth: 460

    function openFor(reset) {
        root.resetFlavour = reset;
        pwField.clear();
        root.open();
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("This will wipe the platform on the remote system.")
            color: Colors.error
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            font.weight: Font.Medium
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("Enter the RSE (Remote Secure Erase) password if one is configured. Leave blank if the firmware doesn't require one.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TextField {
            id: pwField
            echoMode: TextInput.Password
            placeholderText: qsTr("RSE password (optional)")
            font.family: Type.mono
            font.pixelSize: Type.sizeM
            Layout.fillWidth: true
        }

        Text {
            visible: !root.tlsActive
            text: qsTr("Warning: this connection is not TLS, the password will be sent in the clear.")
            color: Colors.error
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    onAccepted: root.confirmed(pwField.text, root.resetFlavour)
}
