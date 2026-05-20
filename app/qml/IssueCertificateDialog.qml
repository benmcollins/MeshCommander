// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Issue a device certificate end-to-end via Phase C (#222). Three
/// steps:
///
/// 1. Pick key length / algorithm / signing algorithm / Subject DN.
///    "Generate" → AMT creates the key pair in firmware and signs a
///    PKCS#10 with the matching private key.
/// 2. Display the firmware-signed PEM CSR with Copy / Save. Operator
///    hands it to their CA.
/// 3. Paste the signed cert PEM, pick which AMT TLS endpoint to bind
///    it to, Install.
Dialog {
    id: root

    required property var controller

    /// Step index: 0 = inputs, 1 = CSR display, 2 = paste signed cert.
    property int step: 0
    /// Firmware-signed CSR (PEM). Populated on `csrReady`.
    property string csrPem: ""
    /// The signed cert the operator pastes back.
    property string signedCertPem: ""

    function openForIssue() {
        step = 0;
        csrPem = "";
        signedCertPem = "";
        // Sensible defaults the operator can override.
        keyLengthBox.currentIndex = 0; // 2048
        signingAlgoBox.currentIndex = 1; // SHA256-RSA
        subjectField.text = qsTr("CN=Intel AMT, O=QuMesh");
        statusBar.text = "";
        open();
    }

    title: qsTr("Issue certificate")
    modal: true
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton
    implicitWidth: 640
    implicitHeight: 540

    FileDialog {
        id: saveCsrDialog
        title: qsTr("Save CSR")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csr"
        nameFilters: [qsTr("PEM CSR (*.csr *.pem)"), qsTr("All files (*)")]
        onAccepted: {
            // QML's FileDialog doesn't give us a write helper; spool
            // the CSR via a temporary `XMLHttpRequest` PUT to the
            // local file path. Falls back to a clipboard hint if the
            // write fails — see the Copy button which is always there.
            const url = saveCsrDialog.selectedFile.toString();
            const xhr = new XMLHttpRequest();
            xhr.open("PUT", url, false);
            xhr.send(root.csrPem);
            if (xhr.status === 0 || xhr.status === 200) {
                statusBar.text = qsTr("Saved CSR to %1").arg(Paths.urlToLocalFile(url));
            } else {
                statusBar.text = qsTr("Couldn't save (status %1); use Copy instead.")
                    .arg(xhr.status);
            }
        }
    }

    Connections {
        target: root.controller
        function onCsrReady(pem) {
            root.csrPem = pem;
            root.step = 1;
            statusBar.text = "";
        }
        function onCsrFailed(err) {
            statusBar.text = err;
            root.step = 0;
        }
        function onCertInstalled(newId) {
            statusBar.text = qsTr("Installed and bound — InstanceID: %1").arg(newId);
            // Auto-close on success; gives the operator a chance to
            // see the confirmation before the dialog vanishes.
            Qt.callLater(root.close);
        }
        function onCertInstallFailed(err) {
            statusBar.text = err;
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Step indicator strip.
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Repeater {
                model: [qsTr("1. Generate"),
                        qsTr("2. CSR"),
                        qsTr("3. Install signed")]
                delegate: Text {
                    required property int index
                    required property string modelData
                    text: modelData
                    color: index === root.step
                        ? Colors.accent
                        : (index < root.step ? Colors.text : Colors.textMuted)
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    font.weight: index === root.step ? Font.Medium : Font.Normal
                }
            }
            Item { Layout.fillWidth: true }
        }

        // Status / error bar.
        Rectangle {
            id: statusBar
            property string text: ""
            visible: text.length > 0
            color: Colors.surface
            radius: 6
            Layout.fillWidth: true
            implicitHeight: visible ? statusText.implicitHeight + 12 : 0
            Text {
                id: statusText
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                text: statusBar.text
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }
        }

        // ---- Step 0: input form -----------------------------------
        ColumnLayout {
            visible: root.step === 0
            spacing: 10
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                Text {
                    text: qsTr("Key length")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                ComboBox {
                    id: keyLengthBox
                    model: [2048, 3072, 4096]
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 140
                }
                Text {
                    text: qsTr("Signing algorithm")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.leftMargin: 24
                }
                ComboBox {
                    id: signingAlgoBox
                    // Index 0 → AMT enum 0 (SHA1-RSA), index 1 → 1 (SHA256-RSA).
                    model: [qsTr("SHA1-RSA (legacy)"), qsTr("SHA256-RSA")]
                    currentIndex: 1
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.fillWidth: true
                }
            }

            Text {
                text: qsTr("Subject DN (RFC 4514 comma-separated):")
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }
            TextField {
                id: subjectField
                Layout.fillWidth: true
                placeholderText: "CN=Intel AMT, O=QuMesh"
                font.family: Type.mono
                font.pixelSize: Type.sizeS
            }

            Text {
                text: qsTr("AMT will generate the key pair inside firmware. The private key never leaves the device; only the signed CSR comes back to you.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Cancel")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: root.close()
                }
                AccentButton {
                    text: qsTr("Generate")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: subjectField.text.trim().length > 0
                    onClicked: {
                        statusBar.text = qsTr("Generating key pair and CSR…");
                        root.controller.generateKeyPairAndCsr(
                            keyLengthBox.model[keyLengthBox.currentIndex],
                            /*keyAlgorithm RSA=*/0,
                            subjectField.text.trim(),
                            signingAlgoBox.currentIndex);
                    }
                }
            }
        }

        // ---- Step 1: display CSR ----------------------------------
        ColumnLayout {
            visible: root.step === 1
            spacing: 10
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                text: qsTr("Hand the CSR below to your CA. Once you have the signed certificate, click Next.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: csrArea
                    text: root.csrPem
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                }
            }

            RowLayout {
                Layout.fillWidth: true
                FlatButton {
                    text: qsTr("Copy")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: {
                        csrArea.selectAll();
                        csrArea.copy();
                        statusBar.text = qsTr("CSR copied to clipboard.");
                    }
                }
                FlatButton {
                    text: qsTr("Save as…")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: saveCsrDialog.open()
                }
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Back")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: root.step = 0
                }
                AccentButton {
                    text: qsTr("Next")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: root.step = 2
                }
            }
        }

        // ---- Step 2: paste signed cert + bind ---------------------
        ColumnLayout {
            visible: root.step === 2
            spacing: 10
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                text: qsTr("Paste the signed certificate PEM your CA returned, then pick which TLS endpoint to bind it to.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: signedArea
                    text: root.signedCertPem
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    placeholderText: "-----BEGIN CERTIFICATE-----\n…\n-----END CERTIFICATE-----"
                    onTextChanged: root.signedCertPem = text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: qsTr("Bind to:")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                }
                ComboBox {
                    id: tlsEndpointBox
                    Layout.fillWidth: true
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    model: (root.controller.deviceCertStore
                             && root.controller.deviceCertStore.tlsSettings) || []
                    textRole: "instanceId"
                    valueRole: "instanceId"
                    delegate: ItemDelegate {
                        required property var modelData
                        width: tlsEndpointBox.width
                        contentItem: Text {
                            text: modelData.instanceId
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Back")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: root.step = 1
                }
                AccentButton {
                    text: qsTr("Install & bind")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: root.signedCertPem.trim().length > 0
                          && tlsEndpointBox.currentIndex >= 0
                    onClicked: {
                        const tls = tlsEndpointBox.model[tlsEndpointBox.currentIndex];
                        statusBar.text = qsTr("Installing and binding…");
                        root.controller.installSignedCertAndBindTls(
                            root.signedCertPem.trim(),
                            tls ? tls.instanceId : "");
                    }
                }
            }
        }
    }
}
