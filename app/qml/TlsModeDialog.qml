// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Edit one row of `AMT_TLSSettingData` (Local LMS or Remote 16993).
/// The QML side rejects Mutual-auth + empty TrustedCN list because
/// that combination locks the operator behind a handshake the firmware
/// can't satisfy.
Dialog {
    id: root

    required property var controller

    property string instanceId
    property bool isLocal: false

    property bool draftEnabled: false
    property bool draftMutualAuth: false
    property bool draftAcceptNonSecureConnections: false
    property string draftTrustedCnText // newline-separated CNs

    function openForRow(row) {
        instanceId = row.instanceId;
        isLocal = row.isLocal === true;
        draftEnabled = row.enabled === true;
        draftMutualAuth = row.mutualAuthentication === true;
        draftAcceptNonSecureConnections = row.acceptNonSecureConnections === true;
        draftTrustedCnText = (row.trustedCn || []).join("\n");
        open();
    }

    title: qsTr("TLS mode — %1")
        .arg(isLocal ? qsTr("Local (LMS)") : qsTr("Remote (16993)"))
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560

    function trustedCnList() {
        return draftTrustedCnText
            .split(/\r?\n/)
            .map(l => l.trim())
            .filter(l => l.length > 0);
    }

    contentItem: ColumnLayout {
        spacing: 14

        Section {
            title: qsTr("MODE")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                CheckBox {
                    text: qsTr("Enable TLS on this endpoint")
                    checked: root.draftEnabled
                    onToggled: root.draftEnabled = checked
                }
                CheckBox {
                    text: qsTr("Require mutual (client-cert) authentication")
                    enabled: root.draftEnabled
                    checked: root.draftMutualAuth
                    onToggled: root.draftMutualAuth = checked
                }
                CheckBox {
                    text: qsTr("Also accept plain (non-TLS) connections")
                    enabled: root.draftEnabled
                    checked: root.draftAcceptNonSecureConnections
                    onToggled: root.draftAcceptNonSecureConnections = checked
                }
            }
        }

        Section {
            title: qsTr("TRUSTED CLIENT CERT CNs")
            visible: root.draftMutualAuth
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true
                Text {
                    text: qsTr("One Common Name per line. The firmware only accepts client certs whose Subject CN matches one of these.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    clip: true
                    TextArea {
                        placeholderText: "operator.example.com\nadmin.example.com"
                        text: root.draftTrustedCnText
                        font.family: Type.mono
                        font.pixelSize: Type.sizeS
                        onTextChanged: root.draftTrustedCnText = text
                    }
                }
            }
        }

        Text {
            visible: root.draftMutualAuth && root.trustedCnList().length === 0
            text: qsTr("Mutual auth needs at least one trusted CN, otherwise the endpoint will reject every handshake.")
            color: Colors.standby
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("Switching to mutual TLS without a valid client cert configured locks the endpoint until a USB re-provisioning. Test from a second machine before relying on it.")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
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
            AccentButton {
                text: qsTr("Apply")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: !root.draftMutualAuth || root.trustedCnList().length > 0
                onClicked: {
                    root.controller.setTlsSettingsForInstance(
                        root.instanceId,
                        root.draftEnabled,
                        root.draftMutualAuth,
                        root.draftAcceptNonSecureConnections,
                        root.trustedCnList());
                    root.accept();
                }
            }
        }
    }
}
