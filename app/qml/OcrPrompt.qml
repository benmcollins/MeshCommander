// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// One-Click Recovery (#170). Picks one of three boot-to-recovery
/// flavors — Windows Recovery Environment, a BIOS-registered Pre-Boot
/// Authentication image (index 1..N), or a signed HTTPS recovery
/// image with optional cert + image-hash pinning — and emits the
/// matching `confirm*` signal. The MachineDetailsWindow holding this
/// dialog wires each signal to the right `controller.bootToXxx`
/// invocation.
Dialog {
    id: root

    /// Capability gates from `controller.bootCapabilities`. The
    /// parent passes these in so the dialog can grey out the
    /// variants the firmware doesn't expose.
    property bool canWinRE: true
    property bool canLocalPBA: true
    property bool canHttps: true

    property bool resetFlavour: true
    /// 0 = WinRE, 1 = Local PBA, 2 = OCR HTTPS.
    property int variant: 0
    property int pbaIndex: 1
    property string httpsUrl: "https://"
    property string hashAlg: "sha256"
    property string hashHex: ""
    property string pinnedCertHashAlg: ""
    property string pinnedCertHashHex: ""
    property string httpsUsername: ""
    property string httpsPassword: ""
    property bool revealHttpsPassword: false

    signal confirmWinRE(bool reset)
    signal confirmLocalPBA(bool reset, int pbaIndex)
    signal confirmHttps(bool reset, string url, string hashAlg,
                         string hashHex, string pinnedCertHashAlg,
                         string pinnedCertHashHex, string username, string password)

    function openFor(reset) {
        root.resetFlavour = reset;
        // Default to the first capability the device actually exposes.
        if (root.canWinRE) root.variant = 0;
        else if (root.canLocalPBA) root.variant = 1;
        else root.variant = 2;
        root.httpsUrl = "https://";
        root.hashHex = "";
        root.pinnedCertHashAlg = "";
        root.pinnedCertHashHex = "";
        root.httpsUsername = "";
        root.httpsPassword = "";
        root.revealHttpsPassword = false;
        root.pbaIndex = 1;
        open();
    }

    title: qsTr("Boot to recovery")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 580
    implicitHeight: 480

    function looksLikeHex(s, requiredBytes) {
        if (!s) return false;
        const trimmed = s.replace(/[\s:]/g, "");
        if (trimmed.length !== requiredBytes * 2) return false;
        return /^[0-9a-fA-F]+$/.test(trimmed);
    }

    function hashBytesForAlg(alg) {
        if (alg === "sha256") return 32;
        if (alg === "sha384") return 48;
        if (alg === "sha512") return 64;
        return 0;
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: qsTr("The remote system will reboot and attempt the chosen recovery flow. The result lands in BIOSLastStatus, readable after the next overview refresh.")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ---- Variant picker ----------------------------------------
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            RadioButton {
                text: qsTr("Windows Recovery Environment (WinRE)")
                enabled: root.canWinRE
                checked: root.variant === 0
                onClicked: root.variant = 0
            }
            RadioButton {
                text: qsTr("Local PBA")
                enabled: root.canLocalPBA
                checked: root.variant === 1
                onClicked: root.variant = 1
            }
            RadioButton {
                text: qsTr("HTTPS recovery image")
                enabled: root.canHttps
                checked: root.variant === 2
                onClicked: root.variant = 2
            }
        }

        // ---- Local PBA index --------------------------------------
        RowLayout {
            visible: root.variant === 1
            Layout.fillWidth: true
            Text {
                text: qsTr("PBA #")
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }
            SpinBox {
                from: 1
                to: 10
                value: root.pbaIndex
                editable: true
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                onValueModified: root.pbaIndex = value
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("Index across the BIOS-registered OCR boot options.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
            }
        }

        // ---- HTTPS variant fields ---------------------------------
        ColumnLayout {
            visible: root.variant === 2
            spacing: 8
            Layout.fillWidth: true

            TextField {
                id: urlField
                placeholderText: qsTr("https://recovery.example/pba.efi")
                text: root.httpsUrl
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
                onTextEdited: root.httpsUrl = text
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Text {
                    text: qsTr("Image hash")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                ComboBox {
                    id: hashAlgBox
                    model: ["sha256", "sha384", "sha512"]
                    currentIndex: 0
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 110
                    onActivated: function(idx) { root.hashAlg = model[idx]; }
                }
                TextField {
                    placeholderText: qsTr("hex (optional)")
                    text: root.hashHex
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                    onTextEdited: root.hashHex = text
                }
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Text {
                    text: qsTr("Server cert hash")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                ComboBox {
                    id: pinnedAlgBox
                    model: ["", "sha256", "sha384", "sha512"]
                    currentIndex: 0
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.preferredWidth: 110
                    onActivated: function(idx) {
                        root.pinnedCertHashAlg = model[idx];
                    }
                }
                TextField {
                    enabled: root.pinnedCertHashAlg.length > 0
                    placeholderText: qsTr("hex (optional)")
                    text: root.pinnedCertHashHex
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                    onTextEdited: root.pinnedCertHashHex = text
                }
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Text {
                    text: qsTr("Username")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    placeholderText: qsTr("optional (HTTPS basic auth)")
                    text: root.httpsUsername
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                    onTextEdited: root.httpsUsername = text
                }
            }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Text {
                    text: qsTr("Password")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 110
                }
                TextField {
                    echoMode: root.revealHttpsPassword
                        ? TextInput.Normal : TextInput.Password
                    text: root.httpsPassword
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                    onTextEdited: root.httpsPassword = text
                }
                FlatButton {
                    text: root.revealHttpsPassword ? qsTr("Hide") : qsTr("Show")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    onClicked: root.revealHttpsPassword = !root.revealHttpsPassword
                }
            }
        }

        // ---- Reset toggle (shared) --------------------------------
        CheckBox {
            text: qsTr("Reset (vs power-on)")
            checked: root.resetFlavour
            onToggled: root.resetFlavour = checked
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
                text: qsTr("Boot")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: {
                    switch (root.variant) {
                    case 0: return root.canWinRE;
                    case 1: return root.canLocalPBA;
                    case 2: {
                        if (!root.canHttps) return false;
                        if (!root.httpsUrl.startsWith("https://")) return false;
                        // If the operator picked an alg, the hex
                        // length must match exactly.
                        if (root.hashHex.length > 0) {
                            const need = root.hashBytesForAlg(root.hashAlg);
                            if (!root.looksLikeHex(root.hashHex, need)) return false;
                        }
                        if (root.pinnedCertHashAlg.length > 0
                            && root.pinnedCertHashHex.length > 0) {
                            const needCert = root.hashBytesForAlg(root.pinnedCertHashAlg);
                            if (!root.looksLikeHex(root.pinnedCertHashHex, needCert))
                                return false;
                        }
                        return true;
                    }
                    }
                    return false;
                }
                onClicked: {
                    switch (root.variant) {
                    case 0:
                        root.confirmWinRE(root.resetFlavour);
                        break;
                    case 1:
                        root.confirmLocalPBA(root.resetFlavour, root.pbaIndex);
                        break;
                    case 2:
                        root.confirmHttps(root.resetFlavour, root.httpsUrl,
                            root.hashHex.length > 0 ? root.hashAlg : "",
                            root.hashHex,
                            root.pinnedCertHashAlg, root.pinnedCertHashHex,
                            root.httpsUsername, root.httpsPassword);
                        break;
                    }
                    root.close();
                }
            }
        }
    }
}
