// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "EventSubscriptions" section. Pulled into its own file
/// — see OverviewSection.qml for the rationale. Pre-#325 this was the
/// inline body of the matching Loader in MachineDetailsWindow.qml.
Flickable {
    id: root

    required property MachineDetailsController controller

    contentWidth: width
    contentHeight: subsCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: subsCol
        spacing: 18
        width: parent.width

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Layout.topMargin: 24
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            Text {
                text: qsTr("EVENT SUBSCRIPTIONS")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                font.letterSpacing: 2
                font.weight: Font.Medium
            }
            Text {
                text: {
                    const es = controller.eventSubscriptions || {};
                    if (Object.keys(es).length === 0)
                        return qsTr("Not yet fetched");
                    const s = es.subscriptions || [];
                    return s.length === 0
                        ? qsTr("No subscriptions configured")
                        : qsTr("%1 subscription%2")
                              .arg(s.length)
                              .arg(s.length === 1 ? "" : "s");
                }
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: 20
            }
            Text {
                text: qsTr("Read-only — Subscribe / UnSubscribe arrives in Phase B.")
                color: Colors.textFaint
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
            }
        }

        Section {
            title: qsTr("ACTIVE SUBSCRIPTIONS")
            visible: !!(controller.eventSubscriptions
                  && controller.eventSubscriptions.subscriptions
                  && controller.eventSubscriptions.subscriptions.length > 0)
            accent: Colors.accent
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true

                Repeater {
                    model: (controller.eventSubscriptions
                             && controller.eventSubscriptions.subscriptions) || []

                    delegate: ColumnLayout {
                        id: subRow
                        required property var modelData
                        spacing: 2
                        Layout.fillWidth: true

                        RowLayout {
                            spacing: 10
                            Layout.fillWidth: true

                            Text {
                                text: subRow.modelData.filterInstanceId || qsTr("(unnamed filter)")
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                            Text {
                                text: subRow.modelData.deliveryModeLabel || ""
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                            }
                        }
                        Text {
                            text: subRow.modelData.destination
                                || subRow.modelData.listenerName
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

        Section {
            title: qsTr("LISTENER DESTINATIONS")
            visible: !!(controller.eventSubscriptions
                  && controller.eventSubscriptions.listeners
                  && controller.eventSubscriptions.listeners.length > 0)
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true

                Repeater {
                    model: (controller.eventSubscriptions
                             && controller.eventSubscriptions.listeners) || []

                    delegate: ColumnLayout {
                        id: listenerRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: listenerRow.modelData.name
                                  + "  ·  "
                                  + listenerRow.modelData.deliveryModeLabel
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: listenerRow.modelData.destination
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

        Section {
            title: qsTr("AVAILABLE FILTERS")
            visible: !!(controller.eventSubscriptions
                  && controller.eventSubscriptions.filters
                  && controller.eventSubscriptions.filters.length > 0)
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Repeater {
                    model: (controller.eventSubscriptions
                             && controller.eventSubscriptions.filters) || []

                    delegate: Text {
                        required property var modelData
                        text: modelData.collectionName
                              || modelData.instanceId
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }
}
