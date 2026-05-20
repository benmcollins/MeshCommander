// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

AppWindow {
    id: root

    width: 920
    height: 560
    minimumWidth: 720
    minimumHeight: 380
    title: qsTr("QuMesh — Certificates")

    property int selectedRow: -1

    FileDialog {
        id: importDialog
        title: qsTr("Import certificate")
        nameFilters: [
            qsTr("Certificates (*.cer *.pem *.crt *.p12 *.pfx)"),
            qsTr("All files (*)"),
        ]
        onAccepted: {
            const path = Paths.urlToLocalFile(importDialog.selectedFile);
            if (path.endsWith(".p12") || path.endsWith(".pfx")) {
                passwordPrompt.path = path;
                passwordPrompt.open();
            } else {
                CertModel.importFromFile(path, "");
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Export certificate")
        fileMode: FileDialog.SaveFile
        property bool wantPem: false
        nameFilters: wantPem
            ? [qsTr("PEM (*.pem)"), qsTr("All files (*)")]
            : [qsTr("DER (*.cer)"), qsTr("All files (*)")]
        onAccepted: {
            if (root.selectedRow < 0) return;
            const path = Paths.urlToLocalFile(exportDialog.selectedFile);
            if (exportDialog.wantPem) {
                CertModel.exportAsPem(root.selectedRow, path);
            } else {
                CertModel.exportAsDer(root.selectedRow, path);
            }
        }
    }

    Dialog {
        id: passwordPrompt
        title: qsTr("PKCS#12 password")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton

        property string path

        contentItem: ColumnLayout {
            spacing: 8
            Text {
                text: qsTr("Enter the password for the PKCS#12 bundle.")
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }
            TextField {
                id: pwField
                echoMode: TextInput.Password
                Layout.fillWidth: true
                Layout.preferredWidth: 280
                font.family: Type.mono
                font.pixelSize: Type.sizeM
                onAccepted: importBtn.clicked()
            }
            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Layout.topMargin: 4
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Cancel")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: passwordPrompt.reject()
                }
                AccentButton {
                    id: importBtn
                    text: qsTr("Import")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: {
                        CertModel.importFromFile(passwordPrompt.path, pwField.text);
                        pwField.clear();
                        passwordPrompt.accept();
                    }
                }
            }
        }

        onRejected: pwField.clear()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Text {
                text: qsTr("CERTIFICATE STORE")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                font.letterSpacing: 2
                font.weight: Font.Medium
            }
            Rectangle {
                color: Colors.borderMuted
                implicitHeight: 1
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }

        Item {
            id: listContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Files dragged onto the window go straight through CertModel,
            // mirroring the Import… dialog. PKCS#12 needs a password we
            // can't infer, so route those through the existing prompt.
            DropArea {
                id: certDrop
                anchors.fill: parent
                keys: ["text/uri-list"]
                onDropped: function(drop) {
                    if (!drop.hasUrls) return;
                    for (let i = 0; i < drop.urls.length; ++i) {
                        const path = Paths.urlToLocalFile(drop.urls[i]);
                        if (path.length === 0) continue;
                        if (path.endsWith(".p12") || path.endsWith(".pfx")) {
                            passwordPrompt.path = path;
                            passwordPrompt.open();
                        } else {
                            CertModel.importFromFile(path, "");
                        }
                    }
                    drop.accept();
                }
            }

            // Empty state — dashed border + verbal cue. Hidden the moment
            // the model has any rows; the underlying ListView takes over.
            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                visible: CertModel.rowCount() === 0
                color: certDrop.containsDrag ? Colors.accentSoft : "transparent"
                radius: 12
                border.width: 1
                border.color: certDrop.containsDrag
                    ? Colors.accent
                    : Colors.borderMuted
                Behavior on color { ColorAnimation { duration: Motion.fast } }
                Behavior on border.color { ColorAnimation { duration: Motion.fast } }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: qsTr("No certificates yet")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeL
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: qsTr("Drag a .cer / .pem / .p12 here, or use Import…")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.alignment: Qt.AlignHCenter
                    }
                    AccentButton {
                        text: qsTr("Import…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
Layout.alignment: Qt.AlignHCenter
                        onClicked: importDialog.open()
                    }
                }
            }

            ListView {
                id: list
                clip: true
                anchors.fill: parent
                visible: CertModel.rowCount() > 0
                model: CertModel
                currentIndex: root.selectedRow

                delegate: Rectangle {
                    id: row

                required property int index
                required property string subject
                required property string issuer
                required property var notAfter
                required property string fingerprint
                required property bool hasPrivateKey

                width: list.width
                implicitHeight: 56
                color: list.currentIndex === row.index ? Colors.accentSoft : "transparent"

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedRow = row.index
                }

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
                            text: row.subject.length > 0 ? row.subject : qsTr("(no common name)")
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeM
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            visible: row.hasPrivateKey
                            text: qsTr("PRIVATE KEY")
                            color: Colors.on
                            font.family: Type.sans
                            font.pixelSize: 9
                            font.letterSpacing: 1
                            font.weight: Font.Medium
                        }
                    }

                    Text {
                        text: {
                            const exp = (row.notAfter && row.notAfter.getTime
                                ? Qt.formatDate(row.notAfter, "yyyy-MM-dd")
                                : "—");
                            return qsTr("Issuer: %1   Expires: %2")
                                .arg(row.issuer.length > 0 ? row.issuer : "—")
                                .arg(exp);
                        }
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: row.fingerprint
                        color: Colors.textFaint
                        font.family: Type.mono
                        font.pixelSize: 10
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
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Button {
                text: qsTr("Import…")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: importDialog.open()
            }
            Button {
                text: qsTr("Export .cer")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.selectedRow >= 0
                onClicked: { exportDialog.wantPem = false; exportDialog.open() }
            }
            Button {
                text: qsTr("Export .pem")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.selectedRow >= 0
                onClicked: { exportDialog.wantPem = true; exportDialog.open() }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Delete")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.selectedRow >= 0
                // Pre-#278 this called removeAt() directly. Cert
                // entries embed PEM private keys; deletion is
                // irreversible.
                onClicked: {
                    confirmDelete.pendingRow = root.selectedRow;
                    confirmDelete.ask(
                        qsTr("Delete certificate?"),
                        qsTr("This removes the certificate and its private "
                             + "key from the store. This cannot be undone."),
                        qsTr("Delete"), true);
                }
            }
            FlatButton {
                text: qsTr("Close")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: root.close()
            }
        }

        /// Error banner (#283). Replaces a 12 px red line at the
        /// bottom of the window — easy to miss, no dismiss. Now
        /// the same shape and prominence as MachineDetailsWindow's
        /// banner, with a Dismiss button.
        ResultBanner {
            id: errBanner
            kind: "error"
            text: CertModel.lastError || ""
            Layout.fillWidth: true
            onDismissed: CertModel.clearLastError()
        }
    }

    ConfirmDialog {
        id: confirmDelete
        property int pendingRow: -1
        onProceed: {
            if (pendingRow >= 0 && CertModel.removeAt(pendingRow)) {
                root.selectedRow = -1;
            }
            pendingRow = -1;
        }
        onRejected: pendingRow = -1
    }
}
