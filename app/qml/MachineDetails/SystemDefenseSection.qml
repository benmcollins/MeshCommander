// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "System Defense" section (was the inline
/// section 17 before #325; ACM only — see #165). Pulled into its
/// own file; see OverviewSection.qml for the rationale.
Flickable {
    id: root

    required property MachineDetailsController controller
    required property var hdr8021FilterDialog
    required property var ipHeadersFilterDialog
    required property var systemDefensePolicyDialog

    contentWidth: width
    contentHeight: sysDefCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: sysDefCol
        spacing: 18
        width: parent.width

        readonly property bool isAcm:
            controller.provisioningMode !== 4
        readonly property bool supported:
            controller.systemDefense
                ? controller.systemDefense.supported !== false
                : true

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Layout.topMargin: 24
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            Text {
                text: qsTr("SYSTEM DEFENSE")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                font.letterSpacing: 2
                font.weight: Font.Medium
            }
            Text {
                text: {
                    if (!sysDefCol.isAcm)
                        return qsTr("ACM only — this device is provisioned in Client Control Mode.");
                    if (!sysDefCol.supported)
                        return qsTr("Not supported by this firmware.");
                    const sd = controller.systemDefense || {};
                    const n = ((sd.policies || []).length)
                            + ((sd.hdrFilters || []).length)
                            + ((sd.ipFilters || []).length);
                    return n === 0
                        ? qsTr("No policies or filters configured.")
                        : qsTr("%1 entr%2 across policies / filters")
                              .arg(n)
                              .arg(n === 1 ? "y" : "ies");
                }
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: 20
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Text {
                    visible: sysDefCol.isAcm && sysDefCol.supported
                    text: qsTr("Add / edit deferred to follow-up (#353).")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    Layout.fillWidth: true
                }
                FlatButton {
                    text: qsTr("Refresh stats")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    enabled: sysDefCol.isAcm && sysDefCol.supported
                    onClicked: root.controller.refreshSystemDefenseStats()
                }
            }
        }

        Section {
            title: qsTr("POLICIES")
            visible: sysDefCol.isAcm
                  && sysDefCol.supported
                  && !!root.controller.systemDefense
            accent: Colors.accent
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Text {
                        text: ((root.controller.systemDefense
                                  && root.controller.systemDefense.policies) || []).length === 0
                            ? qsTr("No policies configured.")
                            : ""
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }
                    Item { Layout.fillWidth: true }
                    FlatButton {
                        text: qsTr("Add policy")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.systemDefensePolicyDialog.openForAdd()
                    }
                }

                Repeater {
                    model: (root.controller.systemDefense
                             && root.controller.systemDefense.policies) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10
                        /// `portDeviceId` of whichever port currently
                        /// has this policy bound (one binding per
                        /// policy at most). Empty when unbound.
                        readonly property string boundPort: {
                            const all = (root.controller.systemDefense
                                          && root.controller.systemDefense.portBindings) || {};
                            const m = all[modelData.instanceId];
                            return (m && m.portDeviceId) || "";
                        }

                        Text {
                            text: modelData.policyName || modelData.instanceId
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                        Text {
                            text: qsTr("pri %1").arg(modelData.priority)
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                        }
                        Text {
                            visible: modelData.defaultPolicy === true
                            text: qsTr("default")
                            color: Colors.accent
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.weight: Font.Medium
                        }
                        Text {
                            visible: parent.boundPort.length > 0
                            text: qsTr("bound: %1").arg(parent.boundPort)
                            color: Colors.accent
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.weight: Font.Medium
                        }
                        FlatButton {
                            text: qsTr("Bind…")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            visible: parent.boundPort.length === 0
                            onClicked: {
                                portBindingPrompt.openForBind(
                                    parent.modelData.instanceId,
                                    parent.modelData.policyName || parent.modelData.instanceId);
                            }
                        }
                        FlatButton {
                            text: qsTr("Unbind")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            visible: parent.boundPort.length > 0
                            onClicked: {
                                // The DeviceID ends in the port index;
                                // tail-extract it (same trick the
                                // wsman extractFilterHandle helper uses).
                                const id = parent.boundPort;
                                let idx = id.length;
                                while (idx > 0 && id.charCodeAt(idx - 1) >= 48
                                                && id.charCodeAt(idx - 1) <= 57) idx--;
                                const portIndex = idx < id.length
                                    ? parseInt(id.substring(idx), 10) : -1;
                                if (portIndex >= 0)
                                    root.controller.unbindSystemDefensePolicy(
                                        portIndex, parent.modelData.instanceId);
                            }
                        }
                        FlatButton {
                            text: qsTr("Delete")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: {
                                systemDefenseConfirmDialog.pendingInstanceId =
                                    parent.modelData.instanceId || "";
                                systemDefenseConfirmDialog.pendingKind = "policy";
                                systemDefenseConfirmDialog.ask(
                                    qsTr("Delete policy"),
                                    qsTr("Remove %1 from the AMT System Defense stack. Any port bindings are dropped on the device.")
                                        .arg(parent.modelData.policyName
                                             || parent.modelData.instanceId
                                             || qsTr("the policy")),
                                    qsTr("Delete"),
                                    true);
                            }
                        }
                    }
                }
            }
        }

        Section {
            title: qsTr("L2 FILTERS (802.1Q / EtherType)")
            visible: sysDefCol.isAcm
                  && sysDefCol.supported
                  && !!root.controller.systemDefense
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Text {
                        text: ((root.controller.systemDefense
                                  && root.controller.systemDefense.hdrFilters) || []).length === 0
                            ? qsTr("No L2 filters configured.")
                            : ""
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }
                    Item { Layout.fillWidth: true }
                    FlatButton {
                        text: qsTr("Add L2 filter")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.hdr8021FilterDialog.openForAdd()
                    }
                }

                Repeater {
                    model: (root.controller.systemDefense
                             && root.controller.systemDefense.hdrFilters) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10
                        readonly property var stat: {
                            const all = (root.controller.systemDefense
                                          && root.controller.systemDefense.stats) || {};
                            return all[modelData.instanceId];
                        }

                        Text {
                            text: modelData.name || modelData.instanceId
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                        Text {
                            text: modelData.etherType > 0
                                ? qsTr("ether 0x%1").arg(modelData.etherType.toString(16))
                                : ""
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                        }
                        Text {
                            text: modelData.vlanTag >= 0
                                ? qsTr("VLAN %1").arg(modelData.vlanTag)
                                : ""
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                        }
                        Text {
                            visible: parent.stat !== undefined
                            text: parent.stat
                                ? qsTr("pass %1 / drop %2")
                                      .arg(parent.stat.packetsPassed)
                                      .arg(parent.stat.packetsDropped)
                                : ""
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                        }
                        FlatButton {
                            text: qsTr("Delete")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: {
                                systemDefenseConfirmDialog.pendingInstanceId =
                                    parent.modelData.instanceId || "";
                                systemDefenseConfirmDialog.pendingKind = "hdr";
                                systemDefenseConfirmDialog.ask(
                                    qsTr("Delete L2 filter"),
                                    qsTr("Remove %1 from the AMT System Defense L2 filter set.")
                                        .arg(parent.modelData.name
                                             || parent.modelData.instanceId
                                             || qsTr("the filter")),
                                    qsTr("Delete"),
                                    true);
                            }
                        }
                    }
                }
            }
        }

        Section {
            title: qsTr("L3/L4 FILTERS (IP / ports)")
            visible: sysDefCol.isAcm
                  && sysDefCol.supported
                  && !!root.controller.systemDefense
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Text {
                        text: ((root.controller.systemDefense
                                  && root.controller.systemDefense.ipFilters) || []).length === 0
                            ? qsTr("No L3/L4 filters configured.")
                            : ""
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }
                    Item { Layout.fillWidth: true }
                    FlatButton {
                        text: qsTr("Add L3/L4 filter")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: root.ipHeadersFilterDialog.openForAdd()
                    }
                }

                Repeater {
                    model: (root.controller.systemDefense
                             && root.controller.systemDefense.ipFilters) || []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 1
                        readonly property var stat: {
                            const all = (root.controller.systemDefense
                                          && root.controller.systemDefense.stats) || {};
                            return all[modelData.instanceId];
                        }

                        RowLayout {
                            spacing: 10
                            Layout.fillWidth: true

                            Text {
                                text: modelData.name || modelData.instanceId
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                            Text {
                                text: modelData.protocol > 0
                                    ? qsTr("proto %1").arg(modelData.protocol)
                                    : ""
                                color: Colors.textMuted
                                font.family: Type.mono
                                font.pixelSize: Type.sizeXs
                            }
                            Text {
                                text: modelData.dstPort > 0
                                    ? qsTr("dst :%1").arg(modelData.dstPort)
                                    : ""
                                color: Colors.textMuted
                                font.family: Type.mono
                                font.pixelSize: Type.sizeXs
                            }
                            Text {
                                visible: parent.parent.stat !== undefined
                                text: parent.parent.stat
                                    ? qsTr("pass %1 / drop %2")
                                          .arg(parent.parent.stat.packetsPassed)
                                          .arg(parent.parent.stat.packetsDropped)
                                    : ""
                                color: Colors.textMuted
                                font.family: Type.mono
                                font.pixelSize: Type.sizeXs
                            }
                            FlatButton {
                                text: qsTr("Delete")
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                onClicked: {
                                    systemDefenseConfirmDialog.pendingInstanceId =
                                        parent.parent.modelData.instanceId || "";
                                    systemDefenseConfirmDialog.pendingKind = "ip";
                                    systemDefenseConfirmDialog.ask(
                                        qsTr("Delete L3/L4 filter"),
                                        qsTr("Remove %1 from the AMT System Defense L3/L4 filter set.")
                                            .arg(parent.parent.modelData.name
                                                 || parent.parent.modelData.instanceId
                                                 || qsTr("the filter")),
                                        qsTr("Delete"),
                                        true);
                                }
                            }
                        }
                        Text {
                            visible: (modelData.srcAddress || modelData.dstAddress || "").length > 0
                            text: qsTr("%1 → %2")
                                .arg(modelData.srcAddress || "*")
                                .arg(modelData.dstAddress || "*")
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }
    }
}
