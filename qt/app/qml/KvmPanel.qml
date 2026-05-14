// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Embeddable KVM viewer pane. Owns its own `KvmController`; the
/// containing window drives connect/disconnect through `start()` /
/// `stop()`. Used by `SessionWindow.qml` as one of its tabs.
Item {
    id: root

    property string targetHost
    property string user
    property string password
    property bool tls: false
    property var trustedFingerprints: []
    property var sshConfig: ({})

    signal trustedFingerprintPersistRequested(string fingerprint)
    signal trustedSshHostKeyPersistRequested(string fingerprint)
    signal peerCertVerifiedByPin(string fingerprint)

    onSshConfigChanged: controller.setSshConfig(root.sshConfig || ({}))

    function start() {
        controller.host = root.targetHost;
        controller.user = root.user;
        controller.password = root.password;
        controller.tls = root.tls;
        controller.trustedFingerprints = root.trustedFingerprints;
        controller.open();
    }
    function stop() { controller.close() }

    function focusViewer() { viewer.forceActiveFocus() }

    // X11 keysyms used by the Send-keys menu.
    readonly property int kXkLAlt: 0xFFE9
    readonly property int kXkLCtrl: 0xFFE3
    readonly property int kXkLMeta: 0xFFE7
    readonly property int kXkLShift: 0xFFE1
    readonly property int kXkEscape: 0xFF1B
    readonly property int kXkTab: 0xFF09
    readonly property int kXkF1Base: 0xFFBE

    function chord(keys) {
        for (let i = 0; i < keys.length; ++i)
            controller.sendKey(keys[i], true);
        for (let i = keys.length - 1; i >= 0; --i)
            controller.sendKey(keys[i], false);
    }

    KvmController {
        id: controller
        Component.onCompleted: controller.setSshConfig(root.sshConfig || ({}))
        onTrustedFingerprintAdded: function(fp) {
            root.trustedFingerprintPersistRequested(fp);
        }
        onTrustedSshHostKeyAdded: function(fp) {
            root.trustedSshHostKeyPersistRequested(fp);
        }
        onPeerCertVerifiedByPin: function(fp) { root.peerCertVerifiedByPin(fp) }
        onStateChanged: {
            if (state === KvmController.Connected) ActivityHeartbeat.reportSuccess();
            else if (state === KvmController.Failed)
                ActivityHeartbeat.reportFailure(qsTr("KVM: %1").arg(lastError));
        }
    }

    CertTrustDialog {
        controller: controller
    }

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
                font.features: ({ "tnum": 1 })
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Send keys ▾")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                enabled: controller.state === KvmController.Connected
                onClicked: sendMenu.popup()

                Menu {
                    id: sendMenu
                    MenuItem {
                        text: qsTr("Ctrl+Alt+Del")
                        onTriggered: controller.sendCtrlAltDel()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Alt+Tab")
                        onTriggered: root.chord([root.kXkLAlt, root.kXkTab])
                    }
                    MenuItem {
                        text: qsTr("Alt+F4")
                        onTriggered: root.chord([root.kXkLAlt, root.kXkF1Base + 3])
                    }
                    MenuItem {
                        text: qsTr("Esc")
                        onTriggered: controller.sendKeyTap(root.kXkEscape)
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Win key")
                        onTriggered: controller.sendKeyTap(root.kXkLMeta)
                    }
                    MenuItem {
                        text: qsTr("Win+L (lock)")
                        onTriggered: root.chord([root.kXkLMeta, 0x6C])
                    }
                    MenuItem {
                        text: qsTr("Win+R (run)")
                        onTriggered: root.chord([root.kXkLMeta, 0x72])
                    }
                    MenuItem {
                        text: qsTr("Win+E (explorer)")
                        onTriggered: root.chord([root.kXkLMeta, 0x65])
                    }
                    MenuItem {
                        text: qsTr("Win+D (desktop)")
                        onTriggered: root.chord([root.kXkLMeta, 0x64])
                    }
                    MenuSeparator {}
                    Menu {
                        title: qsTr("Function keys")
                        Repeater {
                            model: 12
                            delegate: MenuItem {
                                required property int index
                                text: qsTr("F%1").arg(index + 1)
                                onTriggered: controller.sendKeyTap(root.kXkF1Base + index)
                            }
                        }
                    }
                }
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

    onVisibleChanged: if (visible) viewer.forceActiveFocus()
    Component.onCompleted: if (visible) viewer.forceActiveFocus()
}
