// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Editor for `IPS_KVMRedirectionSettingData` (#175). Pulls initial
/// values from `controller` properties at open; on Apply, hands the
/// dirty subset to `controller.setKvmSettings`.
Dialog {
    id: root

    property var controller: null

    // Snapshot at open so we can diff against the firmware's reality
    // and Put only the dirty fields. Leaving a checkbox unchanged
    // means the patch omits that field entirely.
    property bool initialPortEnabled: false
    property bool initialOptIn: false
    property int  initialTimeout: 0
    property bool initialGrey: false

    title: qsTr("KVM settings")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.Cancel | Dialog.Apply
    implicitWidth: 520

    onAboutToShow: {
        if (root.controller) {
            root.initialPortEnabled = root.controller.kvmIs5900PortEnabled;
            root.initialOptIn       = root.controller.kvmOptInPolicy;
            root.initialTimeout     = root.controller.kvmSessionTimeoutMinutes;
            root.initialGrey        = root.controller.kvmGreyscaleRequested;
            portCheck.checked       = root.initialPortEnabled;
            optInCheck.checked      = root.initialOptIn;
            timeoutField.text       = String(root.initialTimeout);
            greyCheck.checked       = root.initialGrey;
            passwordField.text      = "";
        }
    }

    function submitIfReady() {
        if (!root.controller) return;
        const fields = {};
        if (portCheck.checked !== root.initialPortEnabled)
            fields.is5900PortEnabled = portCheck.checked;
        if (optInCheck.checked !== root.initialOptIn)
            fields.optInPolicy = optInCheck.checked;
        const t = parseInt(timeoutField.text);
        if (!isNaN(t) && t !== root.initialTimeout)
            fields.sessionTimeoutMinutes = t;
        if (greyCheck.checked !== root.initialGrey)
            fields.greyscaleRequested = greyCheck.checked;
        // Password is always sent when non-empty (set new) or when
        // the operator ticked "clear". Empty input + no clear =
        // leave existing password alone.
        if (passwordField.text.length > 0)
            fields.rfbPassword = passwordField.text;
        else if (clearPwCheck.checked)
            fields.rfbPassword = "";

        if (Object.keys(fields).length > 0)
            root.controller.setKvmSettings(fields);
        root.close();
    }

    onApplied: root.submitIfReady()

    contentItem: ColumnLayout {
        spacing: 12

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Text {
            text: qsTr("These settings live on the device and apply to every KVM session, not just this one.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        CheckBox {
            id: portCheck
            text: qsTr("Enable VNC listener on port 5900")
            // #423 — pair the checkbox with its label for AX.
            Accessible.name: qsTr("Enable VNC port 5900")
        }

        CheckBox {
            id: optInCheck
            text: qsTr("Require user consent for KVM sessions")
            // #423 — pair the checkbox with its label for AX.
            Accessible.name: qsTr("Require user consent")
        }

        CheckBox {
            id: greyCheck
            text: qsTr("Request 8-bit greyscale (bandwidth saver)")
            // #423 — pair the checkbox with its label for AX.
            Accessible.name: qsTr("Request greyscale")
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Text {
                text: qsTr("Idle timeout (minutes, 0 = none)")
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
            }
            TextField {
                id: timeoutField
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 0; top: 65535 }
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                Layout.preferredWidth: 80
                // #423 — pair the field with its label for AX.
                Accessible.name: qsTr("Idle timeout minutes")
            }
        }

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true

            Text {
                text: qsTr("RFB password (additional to AMT auth)")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
            }
            TextField {
                id: passwordField
                placeholderText: qsTr("Leave empty to keep existing")
                echoMode: TextInput.Password
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
                // #423 — pair the field with its label for AX.
                Accessible.name: qsTr("RFB password")
            }
            CheckBox {
                id: clearPwCheck
                text: qsTr("Clear existing RFB password")
                enabled: passwordField.text.length === 0
                // #423 — pair the checkbox with its label for AX.
                Accessible.name: qsTr("Clear existing RFB password")
            }
        }
    }
}
