// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Detached window hosting one KVM session.
Window {
    id: root

    property string targetHost
    property int targetPort: 16994
    property string user
    property string password
    property string label: qsTr("Remote Desktop")

    function start() {
        controller.host = root.targetHost;
        controller.port = root.targetPort;
        controller.user = root.user;
        controller.password = root.password;
        controller.open();
    }

    width: 1024
    height: 720
    minimumWidth: 640
    minimumHeight: 400
    title: qsTr("QuMesh — %1 — %2").arg(root.label).arg(root.targetHost)
    color: Colors.bg

    KvmController {
        id: controller
    }

    onClosing: controller.close()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: controller.state === KvmController.Connected ? Colors.on
                     : controller.state === KvmController.Failed ? Colors.error
                     : controller.state === KvmController.Disconnected ? Colors.off
                     : Colors.standby
            }

            Text {
                text: {
                    switch (controller.state) {
                    case KvmController.Disconnected:   return qsTr("Disconnected");
                    case KvmController.Connecting:     return qsTr("Connecting…");
                    case KvmController.Authenticating: return qsTr("Authenticating…");
                    case KvmController.Negotiating:    return qsTr("Negotiating display…");
                    case KvmController.Connected:      return qsTr("%1×%2")
                                                                .arg(controller.desktopWidth)
                                                                .arg(controller.desktopHeight);
                    case KvmController.Failed:         return qsTr("Failed: %1").arg(controller.lastError);
                    }
                    return "";
                }
                color: controller.state === KvmController.Failed ? Colors.error : Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Ctrl+Alt+Del")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                enabled: controller.state === KvmController.Connected
                onClicked: controller.sendCtrlAltDel()
            }

            Button {
                text: controller.state === KvmController.Disconnected
                      || controller.state === KvmController.Failed
                    ? qsTr("Reconnect") : qsTr("Disconnect")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    if (controller.state === KvmController.Disconnected
                        || controller.state === KvmController.Failed) {
                        root.start();
                    } else {
                        controller.close();
                    }
                }
            }
        }

        KvmViewer {
            id: viewer
            controller: controller
            focus: true
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Component.onCompleted: viewer.forceActiveFocus()
}
