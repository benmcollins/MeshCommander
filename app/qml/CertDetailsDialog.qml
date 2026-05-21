// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Display-only view of one `AMT_PublicKeyCertificate` row. Subject /
/// Issuer DN strings are surfaced verbatim — the read side already
/// breaks them into common slots, but for a details pane the raw RFC
/// 2253 string is more honest. The base64 X.509 is shown in a
/// scrollable monospace block so the operator can copy/paste it.
Dialog {
    id: root

    property string subjectCn
    property string subjectRaw
    property string issuerCn
    property string issuerRaw
    property string instanceId
    property bool trustedRoot: false
    property bool hasPrivateKey: false
    property bool active: false
    property int derSizeBytes: 0
    property string x509Base64

    function openForCert(row) {
        subjectCn = row.subjectCn || "";
        subjectRaw = row.subjectRaw || "";
        issuerCn = row.issuerCn || "";
        issuerRaw = row.issuerRaw || "";
        instanceId = row.instanceId || "";
        trustedRoot = row.trustedRoot === true;
        hasPrivateKey = row.hasPrivateKey === true;
        active = row.active === true;
        derSizeBytes = row.derSizeBytes || 0;
        x509Base64 = row.x509Base64 || "";
        open();
    }

    title: qsTr("Certificate details")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 720
    implicitHeight: Math.min(formColumn.implicitHeight + 80,
                              (parent ? parent.height : 720) * 0.85)

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

            Section {
                title: qsTr("SUBJECT")
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true
                    Text {
                        text: root.subjectCn.length > 0 ? root.subjectCn : qsTr("(no CN)")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.subjectRaw
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Section {
                title: qsTr("ISSUER")
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true
                    Text {
                        text: root.issuerCn.length > 0 ? root.issuerCn : qsTr("(no CN)")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.issuerRaw
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Section {
                title: qsTr("PROPERTIES")
                Layout.fillWidth: true
                GridLayout {
                    columns: 2
                    columnSpacing: 24
                    rowSpacing: 6
                    Layout.fillWidth: true

                    Text { text: qsTr("Instance ID"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: root.instanceId; color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; elide: Text.ElideMiddle; Layout.fillWidth: true }

                    Text { text: qsTr("DER size"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: qsTr("%1 bytes").arg(root.derSizeBytes); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs }

                    Text { text: qsTr("Trusted root"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: root.trustedRoot ? qsTr("yes") : qsTr("no"); color: root.trustedRoot ? Colors.accent : Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs }

                    Text { text: qsTr("Private key"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: root.hasPrivateKey ? qsTr("yes") : qsTr("no"); color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs }

                    Text { text: qsTr("Active TLS cert"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: root.active ? qsTr("yes") : qsTr("no"); color: root.active ? Colors.accent : Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs }
                }
            }

            Section {
                title: qsTr("X.509 (base64)")
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 6
                    Layout.fillWidth: true
                    ScrollView {
                        clip: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 220
                        TextArea {
                            readOnly: true
                            selectByMouse: true
                            text: root.x509Base64
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            wrapMode: TextArea.Wrap
                        }
                    }
                }
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AccentButton {
                    text: qsTr("Close")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: root.accept()
                }
            }
        }
    }
}
