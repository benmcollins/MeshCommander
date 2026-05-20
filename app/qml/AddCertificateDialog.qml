// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Install a certificate into the AMT device cert store. Operator can
/// paste a PEM block or pick a .cer / .pem file off disk; the file
/// path is read in QML and dumped into the same TextArea so the
/// controller sees one input shape. Trusted-root toggle selects
/// `AddTrustedRootCertificate` vs `AddCertificate`.
Dialog {
    id: root

    required property var controller

    property string draftPem
    property bool asTrustedRoot: true

    function openForAdd() {
        draftPem = "";
        asTrustedRoot = true;
        open();
    }

    title: qsTr("Add certificate")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 640
    implicitHeight: Math.min(formColumn.implicitHeight + 80,
                              (parent ? parent.height : 720) * 0.85)

    FileDialog {
        id: certFileDialog
        title: qsTr("Open certificate file")
        nameFilters: [
            qsTr("Certificate files (*.pem *.cer *.crt *.der)"),
            qsTr("All files (*)")
        ]
        // Route the read through the controller's
        // `readTextFromPath()` — uses QFile on the C++ side, no UI
        // hitch for multi-MB PKCS#7 chains. Pre-#298 this used a
        // synchronous XMLHttpRequest("GET", url, false) which blocked
        // the GUI thread.
        //
        // Note: this still hands DER bytes through a QString
        // TextArea binding when the operator picks a .cer file, so
        // non-ASCII bytes can come through as replacement characters
        // — that's the same shape as before. Operators with raw DER
        // should convert to PEM first.
        onAccepted: {
            const text = root.controller.readTextFromPath(
                certFileDialog.selectedFile.toString());
            if (text.length > 0) root.draftPem = text;
        }
    }

    contentItem: ColumnLayout {
        id: formColumn
        spacing: 14

        Section {
            title: qsTr("CERTIFICATE")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Paste PEM below, or pick a file:")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                    }
                    FlatButton {
                        text: qsTr("Browse…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: certFileDialog.open()
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 240
                    clip: true
                    TextArea {
                        placeholderText: "-----BEGIN CERTIFICATE-----\n…\n-----END CERTIFICATE-----"
                        text: root.draftPem
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        wrapMode: TextArea.NoWrap
                        onTextChanged: root.draftPem = text
                    }
                }
            }
        }

        Section {
            title: qsTr("ROLE")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true
                RadioButton {
                    text: qsTr("Trusted root — anchor for cert-chain verification")
                    checked: root.asTrustedRoot
                    onClicked: root.asTrustedRoot = true
                }
                RadioButton {
                    text: qsTr("Chain / intermediate CA")
                    checked: !root.asTrustedRoot
                    onClicked: root.asTrustedRoot = false
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
                text: qsTr("Add")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.draftPem.trim().length > 0
                onClicked: {
                    root.controller.addCertificateFromPem(root.draftPem,
                                                          root.asTrustedRoot);
                    root.accept();
                }
            }
        }
    }
}
