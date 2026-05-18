// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add a new `IPS_HTTPProxyAccessPoint` to the CIRA stack. Called from
/// the HTTP PROXIES section of `MachineDetailsWindow.qml` when the
/// operator clicks Add proxy. AMT caps the proxy list at 15 — the
/// caller hides this dialog's Add button when the list is full.
Dialog {
    id: root

    property string draftAccessInfo
    property int draftPort: 8080
    property string draftDnsSuffix

    function openForAdd() {
        draftAccessInfo = "";
        draftPort = 8080;
        draftDnsSuffix = "";
        open();
    }

    /// Emitted on Save when the form passes the lightweight checks
    /// below. The controller does the AMT-side validation.
    signal confirmed(string accessInfo, int port, string networkDnsSuffix)

    title: qsTr("Add HTTP proxy")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 460

    contentItem: ColumnLayout {
        spacing: 14

        Section {
            title: qsTr("ACCESS POINT")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Address")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 100
                }
                TextField {
                    placeholderText: qsTr("proxy.corp.example.com or 10.0.0.5")
                    text: root.draftAccessInfo
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftAccessInfo = text
                }

                Text {
                    text: qsTr("Port")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 100
                }
                TextField {
                    text: root.draftPort
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.preferredWidth: 100
                    validator: IntValidator { bottom: 1; top: 65535 }
                    onTextEdited: root.draftPort = parseInt(text) || 0
                }

                Text {
                    text: qsTr("DNS suffix")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 100
                }
                TextField {
                    placeholderText: qsTr("(optional) corp.example.com")
                    text: root.draftDnsSuffix
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftDnsSuffix = text
                }
            }
        }

        Text {
            text: qsTr("CIRA uses this proxy as the outbound hop when reaching the MPS. AMT supports up to 15 access points; only anonymous (no-auth) proxies are configurable here.")
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
                text: qsTr("Add")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root.draftAccessInfo.length > 0
                         && root.draftPort >= 1 && root.draftPort <= 65535
                onClicked: {
                    root.confirmed(root.draftAccessInfo,
                                   root.draftPort,
                                   root.draftDnsSuffix);
                    root.accept();
                }
            }
        }
    }
}
