// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "ActiveSessions" section. Pulled into its own file
/// — see OverviewSection.qml for the rationale. Pre-#325 this was the
/// inline body of the matching Loader in MachineDetailsWindow.qml.
Flickable {
    id: root

    required property MachineDetailsController controller

    contentWidth: width
    contentHeight: sessionsCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: sessionsCol
        spacing: 18
        width: parent.width

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Layout.topMargin: 24
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            Text {
                text: qsTr("ACTIVE SESSIONS")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                font.letterSpacing: 2
                font.weight: Font.Medium
            }
            Text {
                text: {
                    const a = controller.activeSessions;
                    if (!a) return qsTr("Not yet fetched");
                    const total = ((a.sol  || []).length)
                                 + ((a.kvm  || []).length)
                                 + ((a.ider || []).length);
                    return total === 0
                        ? qsTr("No redirection sessions are active.")
                        : qsTr("%1 active session(s)").arg(total);
                }
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: 20
            }
            Text {
                text: qsTr("Useful when a SOL / KVM / IDE-R launch is rejected: AMT allows one session per channel, so this pane shows who's already holding it.")
                color: Colors.textFaint
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // Reusable Section delegate — declared as an inline
        // helper Component instead of a separate file so the
        // tab's three lists share the same skeleton without
        // adding a one-off QML import.
        Component {
            id: sessionsSectionDelegate
            Section {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 8
            }
        }

        Section {
            title: qsTr("SOL")
            accent: Colors.accent
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    visible: ((controller.activeSessions && controller.activeSessions.sol) || []).length === 0
                    text: qsTr("(no active SOL session)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Repeater {
                    model: (controller.activeSessions && controller.activeSessions.sol) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12
                        Text {
                            text: qsTr("%1:%2")
                                .arg(modelData.sourceAddress || "(unknown)")
                                .arg(modelData.sourcePort)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.sessionInstanceId || ""
                            color: Colors.textFaint
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }

        Section {
            title: qsTr("KVM")
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    visible: ((controller.activeSessions && controller.activeSessions.kvm) || []).length === 0
                    text: qsTr("(no active KVM session)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Repeater {
                    model: (controller.activeSessions && controller.activeSessions.kvm) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12
                        Text {
                            text: qsTr("%1:%2")
                                .arg(modelData.sourceAddress || "(unknown)")
                                .arg(modelData.sourcePort)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.sessionInstanceId || ""
                            color: Colors.textFaint
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }

        Section {
            title: qsTr("IDE-R")
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    visible: ((controller.activeSessions && controller.activeSessions.ider) || []).length === 0
                    text: qsTr("(no active IDE-R session)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Repeater {
                    model: (controller.activeSessions && controller.activeSessions.ider) || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12
                        Text {
                            text: qsTr("%1:%2")
                                .arg(modelData.sourceAddress || "(unknown)")
                                .arg(modelData.sourcePort)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.sessionInstanceId || ""
                            color: Colors.textFaint
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }
    }
}
