// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Embeddable SOL terminal pane. Owns its own `SolController`; the
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

    SolController {
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
            if (state === SolController.Connected) ActivityHeartbeat.reportSuccess();
            else if (state === SolController.Failed)
                ActivityHeartbeat.reportFailure(qsTr("SOL: %1").arg(lastError));
        }
    }

    CertTrustDialog {
        controller: controller
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 10
            Layout.fillWidth: true

            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: controller.state === SolController.Connected ? Colors.on
                     : controller.state === SolController.Failed ? Colors.error
                     : controller.state === SolController.Disconnected ? Colors.off
                     : Colors.standby
            }

            Text {
                text: {
                    switch (controller.state) {
                    case SolController.Disconnected:   return qsTr("Disconnected");
                    case SolController.Connecting:     return qsTr("Connecting…");
                    case SolController.Authenticating: return qsTr("Authenticating…");
                    case SolController.Opening:        return qsTr("Opening session…");
                    case SolController.Connected:      return qsTr("Connected");
                    case SolController.Failed:         return qsTr("Failed: %1").arg(controller.lastError);
                    }
                    return "";
                }
                color: controller.state === SolController.Failed ? Colors.error : Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeS
            }

            Item { Layout.fillWidth: true }

            Button {
                text: controller.state === SolController.Connected
                      || controller.state === SolController.Connecting
                      || controller.state === SolController.Authenticating
                      || controller.state === SolController.Opening
                    ? qsTr("Disconnect")
                    : qsTr("Connect")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    if (controller.state === SolController.Disconnected
                        || controller.state === SolController.Failed) {
                        root.start();
                    } else {
                        controller.close();
                    }
                }
            }
        }

        Terminal {
            id: term
            screen: controller.screen
            Layout.fillWidth: true
            Layout.fillHeight: true
            onKeyInput: function(text) { controller.sendText(text); }
            onControlSequence: function(seq) { controller.sendText(seq); }
        }
    }

    onVisibleChanged: if (visible) term.forceActiveFocus()
    Component.onCompleted: if (visible) term.forceActiveFocus()
}
