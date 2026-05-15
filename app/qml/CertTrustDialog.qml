// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal "Trust this cert?" prompt shared by SolPanel, IderPanel,
/// KvmPanel, and SessionWindow's WSMAN power controller. Drive via the
/// controller's pendingCert* properties
/// and `awaitingTrust` flag; on accept, calls
/// `controller.trustPendingCert(persist)`, on cancel `controller.close()`.
///
/// The controller is passed in as a generic QtObject — it must expose
/// the standard set of properties (pendingCertSubject etc.) and the
/// trustPendingCert(bool) invokable.
Dialog {
    id: root

    required property QtObject controller

    title: qsTr("Untrusted TLS certificate")
    modal: true
    closePolicy: Popup.NoAutoClose
    visible: controller && controller.awaitingTrust
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton

    contentItem: ColumnLayout {
        spacing: 10
        Layout.preferredWidth: 460

        Text {
            text: qsTr("The remote machine presented a certificate we haven't "
                       + "seen before. Verify the fingerprint out-of-band before "
                       + "trusting.")
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
                text: qsTr("Subject")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                Layout.alignment: Qt.AlignTop
            }
            Text {
                text: root.controller ? root.controller.pendingCertSubject : ""
                color: Colors.text
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Issuer")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                Layout.alignment: Qt.AlignTop
            }
            Text {
                text: root.controller ? root.controller.pendingCertIssuer : ""
                color: Colors.text
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Valid")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                Layout.alignment: Qt.AlignTop
            }
            Text {
                text: root.controller
                    ? (root.controller.pendingCertNotBefore + " – " + root.controller.pendingCertNotAfter)
                    : ""
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
                text: root.controller ? root.controller.pendingCertFingerprint : ""
                color: Colors.text
                font.family: Type.mono
                font.pixelSize: 11
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
                onClicked: if (root.controller) root.controller.close()
            }
            AccentButton {
                text: qsTr("Trust")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
onClicked: if (root.controller)
                    root.controller.trustPendingCert(persistCheck.checked)
            }
        }
    }
}
