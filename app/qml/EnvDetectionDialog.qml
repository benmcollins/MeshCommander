// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Edit `AMT_EnvironmentDetectionSettingData.DetectionStrings`. AMT
/// classifies itself as "on the corporate network" when DNS resolves
/// any of the entries below; only when none resolve does it open a
/// CIRA tunnel to the MPS. An empty list means "always treat as off-
/// network".
Dialog {
    id: root

    required property var controller

    /// Newline-separated buffer the operator types into.
    property string draftDomainsText

    function openForEdit(envDetection) {
        const list = (envDetection && envDetection.domains) || [];
        draftDomainsText = list.join("\n");
        open();
    }

    function domainList() {
        return draftDomainsText
            .split(/\r?\n/)
            .map(l => l.trim())
            .filter(l => l.length > 0);
    }

    title: qsTr("Environment detection domains")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 560

    contentItem: ColumnLayout {
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Section {
            title: qsTr("DOMAINS")
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true
                Text {
                    text: qsTr("One domain per line. AMT resolves each entry; if any DNS query succeeds, the device is considered on-network and CIRA stays idle.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                ScrollView {
                    clip: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    TextArea {
                        placeholderText: "corp.example.com\nintranet.example.com"
                        text: root.draftDomainsText
                        font.family: Type.mono
                        font.pixelSize: Type.sizeS
                        onTextChanged: root.draftDomainsText = text
                        // #423 — pair the field with its label for AX.
                        Accessible.name: qsTr("Detection domains")
                    }
                }
            }
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
                onClicked: {
                    root.controller.setEnvironmentDetection(root.domainList());
                    root.accept();
                }
            }
        }
    }
}
