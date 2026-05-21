// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Embeddable IDE-R control pane. Unlike SOL/KVM, the user mounts
/// manually via the Mount button, so `start()` is a no-op that lets the
/// containing window keep a uniform tab API.
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

    function start() { /* user-driven; mount via the Mount button */ }
    function stop() { controller.close() }

    IderController {
        id: controller
        host: root.targetHost
        user: root.user
        password: root.password
        tls: root.tls
        trustedFingerprints: root.trustedFingerprints
        Component.onCompleted: controller.setSshConfig(root.sshConfig || ({}))
        onTrustedFingerprintAdded: function(fp) {
            root.trustedFingerprintPersistRequested(fp);
        }
        onTrustedSshHostKeyAdded: function(fp) {
            root.trustedSshHostKeyPersistRequested(fp);
        }
        onPeerCertVerifiedByPin: function(fp) { root.peerCertVerifiedByPin(fp) }
        onStateChanged: {
            if (state === IderController.Running) ActivityHeartbeat.reportSuccess();
            else if (state === IderController.Failed)
                ActivityHeartbeat.reportFailure(qsTr("IDE-R: %1").arg(lastError));
        }
    }

    CertTrustDialog {
        controller: controller
    }

    /// SSH host-key prompt for the per-tab tunnel (#270).
    SshHostKeyTrustDialog {
        controller: controller
        onCancelled: controller.setSshConfig({})
    }

    FileDialog {
        id: isoDialog
        nameFilters: [qsTr("ISO images (*.iso)"), qsTr("All files (*)")]
        title: qsTr("Select ISO to mount")
        onAccepted: controller.isoPath = Paths.urlToLocalFile(isoDialog.selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14

        Section {
            title: qsTr("MEDIA")
            Layout.fillWidth: true

            RowLayout {
                spacing: 8
                Layout.fillWidth: true

                Text {
                    text: qsTr("ISO")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 60
                }

                TextField {
                    text: controller.isoPath
                    placeholderText: qsTr("/path/to/install.iso")
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: controller.isoPath = text
                }

                Button {
                    text: qsTr("Browse…")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: isoDialog.open()
                }
            }
        }

        Section {
            title: qsTr("START TRIGGER")
            Layout.fillWidth: true

            RowLayout {
                spacing: 10
                Layout.fillWidth: true

                RadioButton {
                    text: qsTr("Graceful")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    checked: controller.startOption === IderController.Graceful
                    onClicked: controller.startOption = IderController.Graceful
                }
                RadioButton {
                    text: qsTr("On reboot")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    checked: controller.startOption === IderController.OnReboot
                    onClicked: controller.startOption = IderController.OnReboot
                }
                RadioButton {
                    text: qsTr("Immediately")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    checked: controller.startOption === IderController.Immediate
                    onClicked: controller.startOption = IderController.Immediate
                }
            }
        }

        Section {
            title: qsTr("STATUS")
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                spacing: 10
                Layout.fillWidth: true

                Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    radius: 5
                    color: controller.state === IderController.Running ? Colors.on
                         : controller.state === IderController.Failed ? Colors.error
                         : controller.state === IderController.Disconnected ? Colors.off
                         : Colors.standby
                }

                Text {
                    text: {
                        switch (controller.state) {
                        case IderController.Disconnected:   return qsTr("Disconnected");
                        case IderController.Connecting:     return qsTr("Connecting…");
                        case IderController.Authenticating: return qsTr("Authenticating…");
                        case IderController.Opening:        return qsTr("Opening IDE-R session…");
                        case IderController.Running:        return controller.deviceEnabled
                                                                   ? qsTr("Mounted — AMT BIOS armed")
                                                                   : qsTr("Mounted — waiting for AMT to arm");
                        case IderController.Failed:         return qsTr("Failed: %1").arg(controller.lastError);
                        }
                        return "";
                    }
                    color: controller.state === IderController.Failed ? Colors.error : Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 6
                Layout.fillWidth: true
                Layout.topMargin: 6

                Text {
                    text: qsTr("Sent to AMT")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                }
                Text {
                    text: controller.bytesSentToAmt.toLocaleString(Qt.locale(), 'f', 0)
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    font.features: ({ "tnum": 1 })
                }
                Text {
                    text: qsTr("Received")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                }
                Text {
                    text: controller.bytesReceivedFromAmt.toLocaleString(Qt.locale(), 'f', 0)
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    font.features: ({ "tnum": 1 })
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            Button {
                id: mountBtn
                text: controller.state === IderController.Disconnected
                      || controller.state === IderController.Failed
                    ? qsTr("Mount")
                    : qsTr("Unmount")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: controller.isoPath.length > 0
                ToolTip.visible: hovered && !enabled
                ToolTip.text: qsTr("Pick an ISO above")
                onClicked: {
                    if (controller.state === IderController.Disconnected
                        || controller.state === IderController.Failed) {
                        controller.open();
                    } else {
                        controller.close();
                    }
                }
            }
        }
    }
}
