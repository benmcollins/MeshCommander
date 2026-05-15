// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal that lets the user pick which Platform Erase sub-actions
/// to perform. Each checkbox is gated on the corresponding bit in
/// `capabilitiesMask` so the firmware only sees options it claims
/// to support. Emits `confirmed(flags, psid, ssdPassword, reset)`
/// on accept.
Dialog {
    id: root

    /// Bitmask from `controller.capPlatformEraseMask`.
    property int capabilitiesMask: 0
    property bool resetFlavour: false
    property bool tlsActive: true

    signal confirmed(int flags, string psid, string ssdPassword, bool reset)

    title: qsTr("Confirm Platform Erase")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.Cancel | Dialog.Yes
    implicitWidth: 520

    function openFor(reset) {
        root.resetFlavour = reset;
        cbPyrite.checked = false;
        cbSsd.checked = false;
        cbTpm.checked = false;
        cbBiosNvm.checked = false;
        cbGolden.checked = false;
        cbCsme.checked = false;
        psidField.clear();
        ssdPwField.clear();
        root.open();
    }

    function _flagSupported(bit) { return (root.capabilitiesMask & (1 << bit)) !== 0 }
    function _flags() {
        let f = 0;
        if (cbPyrite.checked)  f |= (1 << 1);
        if (cbSsd.checked)     f |= (1 << 2);
        if (cbTpm.checked)     f |= (1 << 6);
        if (cbBiosNvm.checked) f |= (1 << 25);
        if (cbGolden.checked)  f |= (1 << 26);
        if (cbCsme.checked)    f |= (1 << 31);
        return f;
    }

    contentItem: ColumnLayout {
        spacing: 8

        Text {
            text: qsTr("This will wipe the remote platform.")
            color: Colors.error
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            font.weight: Font.Medium
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("Select the sub-actions to perform — only options the firmware advertises are enabled.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.bottomMargin: 4
        }

        CheckBox { id: cbPyrite;  text: qsTr("Pyrite Revert");                                   visible: root._flagSupported(1) }
        TextField {
            id: psidField
            visible: cbPyrite.visible && cbPyrite.checked
            placeholderText: qsTr("Pyrite PSID")
            font.family: Type.mono
            font.pixelSize: Type.sizeS
            Layout.fillWidth: true
        }

        CheckBox { id: cbSsd;     text: qsTr("Secure Erase All SSDs");                           visible: root._flagSupported(2) }
        TextField {
            id: ssdPwField
            visible: cbSsd.visible && cbSsd.checked
            echoMode: TextInput.Password
            placeholderText: qsTr("SSD admin password")
            font.family: Type.mono
            font.pixelSize: Type.sizeS
            Layout.fillWidth: true
        }

        CheckBox { id: cbTpm;     text: qsTr("TPM Clear");                                       visible: root._flagSupported(6) }
        CheckBox { id: cbBiosNvm; text: qsTr("Clear BIOS NVM Variables");                        visible: root._flagSupported(25) }
        CheckBox { id: cbGolden;  text: qsTr("BIOS Reload of Golden Configuration");             visible: root._flagSupported(26) }
        CheckBox { id: cbCsme;    text: qsTr("CSME Unconfigure");                                visible: root._flagSupported(31) }

        Text {
            visible: !root.tlsActive
            text: qsTr("Warning: this connection is not TLS, passwords will be sent in the clear.")
            color: Colors.error
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    onAccepted: root.confirmed(_flags(), psidField.text, ssdPwField.text, root.resetFlavour)
}
