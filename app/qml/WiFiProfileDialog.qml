// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit a PSK WiFi profile. Enterprise (EAP-TLS / PEAP) is
/// disabled here and gated to a Phase C PR — it needs cert binding
/// through AMT_PublicKeyCertificate EPRs, which is its own multi-step
/// workflow.
Dialog {
    id: root

    required property var controller

    property bool isEdit: false
    property string draftElementName
    property string draftSsid
    property int draftAuthenticationMethod: 6  // 6 = WPA2-PSK, 7 = WPA3-PSK
    property int draftEncryptionMethod: 4      // 3 = TKIP, 4 = CCMP
    property int draftPriority: 1
    property string draftPsk
    property bool revealPsk: false

    function openForAdd() {
        isEdit = false;
        draftElementName = "";
        draftSsid = "";
        draftAuthenticationMethod = 6;
        draftEncryptionMethod = 4;
        draftPriority = 1;
        draftPsk = "";
        revealPsk = false;
        open();
    }

    function openForEdit(profile) {
        isEdit = true;
        draftElementName = profile.elementName || "";
        draftSsid = profile.ssid || "";
        draftAuthenticationMethod = profile.authenticationMethod >= 0
            ? profile.authenticationMethod : 6;
        draftEncryptionMethod = profile.encryptionMethod >= 0
            ? profile.encryptionMethod : 4;
        draftPriority = profile.priority >= 0 ? profile.priority : 1;
        draftPsk = "";
        revealPsk = false;
        open();
    }

    title: isEdit ? qsTr("Edit WiFi profile") : qsTr("Add WiFi profile")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560

    function pskValid() {
        return draftPsk.length >= 8;
    }

    contentItem: ColumnLayout {
        spacing: 14

        Section {
            title: qsTr("PROFILE")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Name")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    text: root.draftElementName
                    enabled: !root.isEdit
                    placeholderText: qsTr("CorpNet")
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftElementName = text
                }

                Text {
                    text: qsTr("SSID")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    text: root.draftSsid
                    placeholderText: qsTr("(visible network name)")
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftSsid = text
                }

                Text {
                    text: qsTr("Priority")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    text: root.draftPriority
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.preferredWidth: 80
                    validator: IntValidator { bottom: 1; top: 255 }
                    onTextEdited: root.draftPriority = parseInt(text) || 1
                }
            }
        }

        Section {
            title: qsTr("SECURITY")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Auth")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("WPA2-PSK")
                        checked: root.draftAuthenticationMethod === 6
                        onClicked: root.draftAuthenticationMethod = 6
                    }
                    RadioButton {
                        text: qsTr("WPA3-PSK")
                        checked: root.draftAuthenticationMethod === 7
                        onClicked: root.draftAuthenticationMethod = 7
                    }
                }

                Text {
                    text: qsTr("Cipher")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("CCMP/AES")
                        checked: root.draftEncryptionMethod === 4
                        onClicked: root.draftEncryptionMethod = 4
                    }
                    RadioButton {
                        text: qsTr("TKIP")
                        checked: root.draftEncryptionMethod === 3
                        onClicked: root.draftEncryptionMethod = 3
                    }
                }

                Text {
                    text: qsTr("PSK")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    spacing: 6
                    Layout.fillWidth: true
                    TextField {
                        echoMode: root.revealPsk ? TextInput.Normal : TextInput.Password
                        placeholderText: root.isEdit
                            ? qsTr("Leave blank to keep current")
                            : qsTr("≥ 8 characters")
                        text: root.draftPsk
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftPsk = text
                    }
                    FlatButton {
                        text: root.revealPsk ? qsTr("Hide") : qsTr("Show")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.revealPsk = !root.revealPsk
                    }
                }
            }
        }

        Text {
            text: qsTr("Enterprise profiles (EAP-TLS / PEAP) need a client cert and CA cert binding and will arrive in a later release.")
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
                text: root.isEdit ? qsTr("Save changes") : qsTr("Add profile")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: {
                    if (root.draftElementName.length === 0) return false;
                    if (root.draftSsid.length === 0) return false;
                    // For Add, PSK is required (≥ 8 chars).
                    // For Edit, blank PSK means "keep current"; if
                    // non-blank, still ≥ 8.
                    if (!root.isEdit) return root.pskValid();
                    if (root.draftPsk.length === 0) return true;
                    return root.pskValid();
                }
                onClicked: {
                    const fields = {
                        "elementName": root.draftElementName,
                        "ssid": root.draftSsid,
                        "authenticationMethod": root.draftAuthenticationMethod,
                        "encryptionMethod": root.draftEncryptionMethod,
                        "priority": root.draftPriority,
                        "psk": root.draftPsk,
                    };
                    if (root.isEdit)
                        root.controller.updateWiFiPskProfile(fields);
                    else
                        root.controller.addWiFiPskProfile(fields);
                    root.accept();
                }
            }
        }
    }
}
