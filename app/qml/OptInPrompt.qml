// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal that asks the operator for the 6-digit consent code AMT is
/// displaying on the target's local screen. Drive it via `openFor()`;
/// resolves into `submitted(code)` or `cancelled`.
Dialog {
    id: root

    /// Optional error string from a previous \`sendOptInCode\` attempt
    /// (wrong code, expired, etc.). Shown in red under the field.
    property string errorText: ""

    /// Seconds the firmware will wait for the operator to enter the
    /// consent code (from `IPS_KVMRedirectionSettingData.OptInPolicyTimeout`).
    /// 0 hides the countdown. Set via `openFor(timeout)`.
    property int timeoutSec: 0
    property int secondsRemaining: 0

    signal submitted(int code)
    signal cancelled

    title: qsTr("Enter consent code")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 480

    onClosed: countdownTimer.stop()

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (root.secondsRemaining <= 0) {
                stop();
                root.cancelled();
                root.reject();
                return;
            }
            root.secondsRemaining -= 1;
        }
    }

    function openFor(timeout) {
        root.errorText = "";
        codeField.text = "";
        root.timeoutSec = timeout || 0;
        root.secondsRemaining = root.timeoutSec;
        countdownTimer.start();
        root.open();
        codeField.forceActiveFocus();
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: qsTr("AMT is displaying a 6-digit consent code on the remote machine's local screen. Read it from the target and enter it here to unlock the session.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TextField {
            id: codeField
            placeholderText: qsTr("123456")
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 0; top: 999999 }
            maximumLength: 6
            font.family: Type.mono
            font.pixelSize: Type.sizeL
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Keys.onReturnPressed: if (codeField.text.length === 6) submitBtn.clicked()
        }

        Text {
            visible: root.timeoutSec > 0
            text: qsTr("Time remaining: %1s").arg(root.secondsRemaining)
            color: root.secondsRemaining <= 10 ? Colors.error : Colors.textMuted
            font.family: Type.mono
            font.pixelSize: Type.sizeXs
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Text {
            visible: root.errorText.length > 0
            text: root.errorText
            color: Colors.error
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
                onClicked: {
                    root.cancelled();
                    root.reject();
                }
            }

            AccentButton {
                id: submitBtn
                text: qsTr("Submit")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: codeField.text.length === 6
                onClicked: {
                    root.submitted(parseInt(codeField.text));
                    // Don't close yet — the controller may report a wrong
                    // code and the caller will call `openFor()` again with
                    // an updated `errorText` to re-prompt.
                }
            }
        }
    }
}
