// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add or edit a WiFi profile. PSK (Phase B, #159) and enterprise
/// EAP-TLS / PEAP (Phase C, #223) both share this dialog — when the
/// EAP auth picker is selected, the PSK row hides and the cert /
/// username/password section unfolds.
Dialog {
    id: root

    required property MachineDetailsController controller

    property bool isEdit: false
    property string draftElementName
    property string draftSsid
    /// PSK: 6 = WPA2-PSK, 7 = WPA3-PSK.
    /// Enterprise: 5 = WPA2-Enterprise, 8 = WPA3-Enterprise.
    property int draftAuthenticationMethod: 6
    property int draftEncryptionMethod: 4      // 3 = TKIP, 4 = CCMP
    property int draftPriority: 1
    property string draftPsk
    property bool revealPsk: false

    /// Phase C (#223). Tracks the EAP toggle distinctly from
    /// `draftAuthenticationMethod` so the WPA2/WPA3 cipher picker
    /// stays usable independently of the PSK/EAP one.
    property bool draftEnterprise: false
    /// `CIM_IEEE8021xSettings.AuthenticationProtocol` — 0 = EAP-TLS,
    /// 2 = PEAPv0/MSCHAPv2. The dialog only surfaces these two for
    /// now; the rest of the enum (1, 3, 4, 5) ride along the same
    /// dispatch path if a future operator needs them.
    property int draftAuthenticationProtocol: 0
    property string draftEapUsername
    property string draftEapPassword
    property bool revealEapPassword: false
    property string draftEapServerCertificateName
    property int draftEapServerCertificateNameComparison: 1  // 1 = FullName
    property string draftClientCertInstanceId
    property string draftCaCertInstanceId

    function openForAdd() {
        isEdit = false;
        draftElementName = "";
        draftSsid = "";
        draftAuthenticationMethod = 6;
        draftEncryptionMethod = 4;
        draftPriority = 1;
        draftPsk = "";
        revealPsk = false;
        draftEnterprise = false;
        draftAuthenticationProtocol = 0;
        draftEapUsername = "";
        draftEapPassword = "";
        revealEapPassword = false;
        draftEapServerCertificateName = "";
        draftEapServerCertificateNameComparison = 1;
        draftClientCertInstanceId = "";
        draftCaCertInstanceId = "";
        open();
    }

    function openForEdit(profile) {
        isEdit = true;
        draftElementName = profile.elementName || "";
        draftSsid = profile.ssid || "";
        draftAuthenticationMethod = profile.authenticationMethod >= 0
            ? profile.authenticationMethod : 6;
        draftEnterprise = draftAuthenticationMethod === 5
                       || draftAuthenticationMethod === 8;
        draftEncryptionMethod = profile.encryptionMethod >= 0
            ? profile.encryptionMethod : 4;
        draftPriority = profile.priority >= 0 ? profile.priority : 1;
        draftPsk = "";
        revealPsk = false;
        // EAP fields aren't echoed back by AMT on the read side
        // (passwords / cert bindings come back as EPRs that the
        // current parser doesn't surface), so editing an enterprise
        // profile is a "re-fill the credentials" path.
        draftAuthenticationProtocol = 0;
        draftEapUsername = "";
        draftEapPassword = "";
        revealEapPassword = false;
        draftEapServerCertificateName = "";
        draftEapServerCertificateNameComparison = 1;
        draftClientCertInstanceId = "";
        draftCaCertInstanceId = "";
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

    function submitReady() {
        if (draftElementName.length === 0) return false;
        if (draftSsid.length === 0) return false;
        if (draftEnterprise) {
            if (draftAuthenticationProtocol === 0) {
                // EAP-TLS needs a picked client cert.
                return draftClientCertInstanceId.length > 0;
            }
            // PEAP / TTLS / EAP-FAST need username+password.
            return draftEapUsername.length > 0
                && draftEapPassword.length > 0;
        }
        // PSK path: Add needs ≥ 8 chars; Edit allows blank
        // (keep current) but non-blank still needs ≥ 8.
        if (!isEdit) return pskValid();
        if (draftPsk.length === 0) return true;
        return pskValid();
    }

    function submitIfReady() {
        if (!submitReady()) return;
        const fields = {
            "elementName": draftElementName,
            "ssid": draftSsid,
            "authenticationMethod": draftAuthenticationMethod,
            "encryptionMethod": draftEncryptionMethod,
            "priority": draftPriority,
            "psk": draftPsk,
            "enterpriseEnabled": draftEnterprise,
            "authenticationProtocol": draftAuthenticationProtocol,
            "eapUsername": draftEapUsername,
            "eapPassword": draftEapPassword,
            "eapServerCertificateName": draftEapServerCertificateName,
            "eapServerCertificateNameComparison":
                draftEapServerCertificateNameComparison,
            "clientCertificateInstanceId": draftClientCertInstanceId,
            "caCertificateInstanceId": draftCaCertInstanceId,
        };
        if (isEdit)
            controller.updateWiFiPskProfile(fields);
        else
            controller.addWiFiPskProfile(fields);
        accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

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
                    // #379 — pair the field with its label for AX.
                    Accessible.name: qsTr("Profile name")
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
                    // #379 — pair the field with its label for AX.
                    Accessible.name: qsTr("SSID")
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
                    // #379 — pair the field with its label for AX.
                    Accessible.name: qsTr("Priority")
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
                    text: qsTr("Family")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    RadioButton {
                        text: qsTr("WPA2")
                        checked: root.draftAuthenticationMethod === 6
                                  || root.draftAuthenticationMethod === 5
                        onClicked: root.draftAuthenticationMethod =
                            root.draftEnterprise ? 5 : 6
                    }
                    RadioButton {
                        text: qsTr("WPA3")
                        checked: root.draftAuthenticationMethod === 7
                                  || root.draftAuthenticationMethod === 8
                        onClicked: root.draftAuthenticationMethod =
                            root.draftEnterprise ? 8 : 7
                    }
                }

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
                        text: qsTr("PSK")
                        checked: !root.draftEnterprise
                        onClicked: {
                            root.draftEnterprise = false;
                            // Snap auth method to the PSK variant for
                            // the family the operator already picked.
                            root.draftAuthenticationMethod =
                                root.draftAuthenticationMethod === 8 ? 7 : 6;
                        }
                    }
                    RadioButton {
                        text: qsTr("EAP-TLS")
                        checked: root.draftEnterprise
                              && root.draftAuthenticationProtocol === 0
                        onClicked: {
                            root.draftEnterprise = true;
                            root.draftAuthenticationProtocol = 0;
                            root.draftAuthenticationMethod =
                                root.draftAuthenticationMethod === 7 ? 8 : 5;
                        }
                    }
                    RadioButton {
                        text: qsTr("PEAP")
                        checked: root.draftEnterprise
                              && root.draftAuthenticationProtocol === 2
                        onClicked: {
                            root.draftEnterprise = true;
                            root.draftAuthenticationProtocol = 2;
                            root.draftAuthenticationMethod =
                                root.draftAuthenticationMethod === 7 ? 8 : 5;
                        }
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

                // --- PSK row (hidden in EAP modes) ---------------
                Text {
                    visible: !root.draftEnterprise
                    text: qsTr("PSK")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    visible: !root.draftEnterprise
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

                // --- EAP-TLS: client cert picker -----------------
                Text {
                    visible: root.draftEnterprise
                          && root.draftAuthenticationProtocol === 0
                    text: qsTr("Client cert")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                ComboBox {
                    id: clientCertCombo
                    visible: root.draftEnterprise
                          && root.draftAuthenticationProtocol === 0
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    model: (root.controller.deviceCertStore
                             && root.controller.deviceCertStore.certificates) || []
                    textRole: "subjectCn"
                    Layout.fillWidth: true
                    delegate: ItemDelegate {
                        required property var modelData
                        width: clientCertCombo.width
                        contentItem: Text {
                            text: (modelData.subjectCn && modelData.subjectCn.length > 0)
                                ? modelData.subjectCn
                                : modelData.instanceId
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                    }
                    onActivated: function(idx) {
                        const m = clientCertCombo.model[idx];
                        root.draftClientCertInstanceId =
                            (m && m.instanceId) ? m.instanceId : "";
                    }
                }

                // --- PEAP: username + password ------------------
                Text {
                    visible: root.draftEnterprise
                          && root.draftAuthenticationProtocol === 2
                    text: qsTr("Username")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    visible: root.draftEnterprise
                          && root.draftAuthenticationProtocol === 2
                    text: root.draftEapUsername
                    placeholderText: qsTr("alice@corp.example")
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftEapUsername = text
                }
                Text {
                    visible: root.draftEnterprise
                          && root.draftAuthenticationProtocol === 2
                    text: qsTr("Password")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                RowLayout {
                    visible: root.draftEnterprise
                          && root.draftAuthenticationProtocol === 2
                    spacing: 6
                    Layout.fillWidth: true
                    TextField {
                        echoMode: root.revealEapPassword
                            ? TextInput.Normal : TextInput.Password
                        text: root.draftEapPassword
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftEapPassword = text
                    }
                    FlatButton {
                        text: root.revealEapPassword ? qsTr("Hide") : qsTr("Show")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.revealEapPassword = !root.revealEapPassword
                    }
                }

                // --- Common to both EAP modes: CA cert + server name guard
                Text {
                    visible: root.draftEnterprise
                    text: qsTr("CA cert")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                ComboBox {
                    id: caCertCombo
                    visible: root.draftEnterprise
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    model: (root.controller.deviceCertStore
                             && root.controller.deviceCertStore.certificates) || []
                    textRole: "subjectCn"
                    Layout.fillWidth: true
                    delegate: ItemDelegate {
                        required property var modelData
                        width: caCertCombo.width
                        contentItem: Text {
                            text: (modelData.subjectCn && modelData.subjectCn.length > 0)
                                ? modelData.subjectCn
                                : modelData.instanceId
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                    }
                    onActivated: function(idx) {
                        const m = caCertCombo.model[idx];
                        root.draftCaCertInstanceId =
                            (m && m.instanceId) ? m.instanceId : "";
                    }
                }
                Text {
                    visible: root.draftEnterprise
                    text: qsTr("Server name")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    visible: root.draftEnterprise
                    text: root.draftEapServerCertificateName
                    placeholderText: qsTr("Optional. RADIUS cert CN/SAN to require.")
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftEapServerCertificateName = text
                }
            }
        }

        Text {
            visible: root.draftEnterprise
            text: qsTr("EAP-TLS needs a client cert + key already in the device store; use Issue certificate or Add certificate to put one there first. PEAP just needs the RADIUS account credentials.")
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
                enabled: root.submitReady()
                onClicked: root.submitIfReady()
            }
        }
    }
}
