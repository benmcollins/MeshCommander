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
            Text {
                visible: sysDefCol.isAcm && sysDefCol.supported
                text: qsTr("Read-only — statistics + add/edit defer to Phase B.")
                color: Colors.textFaint
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
            }
        }

        Section {
            title: qsTr("POLICIES")
            visible: sysDefCol.isAcm
                  && sysDefCol.supported
                  && controller.systemDefense
                  && (controller.systemDefense.policies || []).length > 0
            accent: Colors.accent
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true

                Repeater {
                    model: (controller.systemDefense
                             && controller.systemDefense.policies) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10

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
                    }
                }
            }
        }

        Section {
            title: qsTr("L2 FILTERS (802.1Q / EtherType)")
            visible: sysDefCol.isAcm
                  && sysDefCol.supported
                  && controller.systemDefense
                  && (controller.systemDefense.hdrFilters || []).length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true

                Repeater {
                    model: (controller.systemDefense
                             && controller.systemDefense.hdrFilters) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10

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
                    }
                }
            }
        }

        Section {
            title: qsTr("L3/L4 FILTERS (IP / ports)")
            visible: sysDefCol.isAcm
                  && sysDefCol.supported
                  && controller.systemDefense
                  && (controller.systemDefense.ipFilters || []).length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24

            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true

                Repeater {
                    model: (controller.systemDefense
                             && controller.systemDefense.ipFilters) || []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 1

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
