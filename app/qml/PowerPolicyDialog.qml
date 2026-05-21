// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Radio-list dialog for `AMT_SystemPowerScheme.SetPowerScheme`. Reads
/// `controller.powerSchemes` and `controller.currentPowerSchemeId` for
/// the model and the initial selection; resolves into
/// `confirmed(instanceId)`. See #162.
Dialog {
    id: root

    required property MachineDetailsController controller

    // Tracks which radio is dotted right now. Initialised on open from
    // the controller so the dialog reflects firmware state — picking a
    // scheme and cancelling out doesn't push anything.
    property string selectedInstanceId: ""

    signal confirmed(string instanceId)

    title: qsTr("Intel AMT power policy")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.Cancel | Dialog.Ok
    implicitWidth: 520

    onAboutToShow: {
        root.selectedInstanceId = root.controller.currentPowerSchemeId;
    }

    onAccepted: {
        if (root.selectedInstanceId.length > 0
            && root.selectedInstanceId !== root.controller.currentPowerSchemeId)
            root.confirmed(root.selectedInstanceId);
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("Select the power policy AMT should enforce. The choice persists across reboots.")
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ButtonGroup {
            id: schemeGroup
        }

        Repeater {
            model: root.controller.powerSchemes

            delegate: RadioButton {
                id: schemeRadio
                required property var modelData
                Layout.fillWidth: true
                ButtonGroup.group: schemeGroup
                checked: root.selectedInstanceId === schemeRadio.modelData.instanceId
                // Strip the legacy "N:" prefix some firmwares put on the
                // description so the radio label reads naturally.
                text: {
                    const d = schemeRadio.modelData.description || "";
                    const colon = d.indexOf(":");
                    return colon >= 0 ? d.substring(colon + 1).trim() : d;
                }
                onClicked: root.selectedInstanceId = schemeRadio.modelData.instanceId
            }
        }
    }
}
