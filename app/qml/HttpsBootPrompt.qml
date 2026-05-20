// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal that collects the HTTPS Boot URL and resolves into
/// `confirmed(url, reset)`. AMT's OCR UEFI HTTPS-Boot path requires
/// the URL to be reachable from the managed host and to terminate in
/// HTTPS (the firmware refuses plain HTTP).
Dialog {
    id: root

    property bool resetFlavour: false
    property bool tlsActive: true

    signal confirmed(string url, bool reset)

    title: qsTr("Boot to HTTPS URL")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.Cancel | Dialog.Yes
    implicitWidth: 520

    function openFor(reset) {
        root.resetFlavour = reset;
        urlField.text = "https://";
        root.open();
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("The remote system will reboot and attempt to fetch the boot image from the URL below over HTTPS.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("The URL must use https:// and must be reachable from the managed host's network, not just from this machine.")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TextField {
            id: urlField
            placeholderText: qsTr("https://server.example.com/path/to/boot.iso")
            font.family: Type.mono
            font.pixelSize: Type.sizeM
            text: "https://"
            Layout.fillWidth: true
        }

        Text {
            visible: !root.tlsActive
            text: qsTr("Note: the WSMAN session to AMT is not TLS — only the firmware's outbound fetch will be encrypted.")
            color: Colors.error
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    onAccepted: root.confirmed(urlField.text, root.resetFlavour)
}
