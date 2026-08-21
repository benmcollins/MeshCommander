// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

/// Embeddable SOL terminal pane. Owns its own `SolController`; the
/// containing window drives connect/disconnect through `start()` /
/// `stop()`. Used by `SessionWindow.qml` as one of its tabs.
Item {
    id: root

    property string targetHost
    property string machineName
    property string user
    property string password
    property bool tls: false
    property var trustedFingerprints: []
    property var sshConfig: ({})

    // Prefix the save dialogs default to. Prefer the user-given name;
    // fall back to the host when no name is set.
    readonly property string saveNamePrefix:
        machineName.length > 0 ? machineName : targetHost

    signal trustedFingerprintPersistRequested(string fingerprint)
    signal trustedSshHostKeyPersistRequested(string fingerprint)
    signal peerCertVerifiedByPin(string fingerprint)

    /// The operator asked to connect. The host gates this on user
    /// consent when the firmware requires it (#433).
    signal connectRequested()
    onSshConfigChanged: controller.setSshConfig(root.sshConfig || ({}))
    onVisibleChanged: if (visible) term.forceActiveFocus()
    Component.onCompleted: if (visible) term.forceActiveFocus()

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

    /// SSH host-key prompt for the per-tab tunnel (#270). Cancel
    /// disables the SSH config so the tunnel tears down.
    SshHostKeyTrustDialog {
        controller: controller
        onCancelled: controller.setSshConfig({})
    }

    // Open the save dialogs with a default name pre-populated.
    function openScreenshotDialog() {
        screenshotDialog.currentFolder =
            StandardPaths.writableLocation(StandardPaths.DocumentsLocation);
        screenshotDialog.selectedFile = screenshotDialog.currentFolder + "/"
            + FilenameFormatter.defaultName(root.saveNamePrefix, "png");
        screenshotDialog.open();
    }
    function openRecordDialog() {
        recordDialog.currentFolder =
            StandardPaths.writableLocation(StandardPaths.DocumentsLocation);
        recordDialog.selectedFile = recordDialog.currentFolder + "/"
            + FilenameFormatter.defaultName(root.saveNamePrefix, "cast");
        recordDialog.open();
    }

    FileDialog {
        id: recordDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "cast"
        nameFilters: [qsTr("asciicast v2 (*.cast)"), qsTr("All files (*)")]
        title: qsTr("Record SOL session to asciicast")
        onAccepted: {
            const path = Paths.urlToLocalFile(recordDialog.selectedFile);
            if (path.length > 0) {
                const title = qsTr("QuMesh SOL — %1").arg(root.targetHost);
                controller.startRecording(path, title);
            }
        }
    }

    QtObject {
        id: recState
        property int elapsedSec: 0
    }
    Timer {
        running: controller.recording
        interval: 1000
        repeat: true
        onTriggered: recState.elapsedSec += 1
        onRunningChanged: if (!running) recState.elapsedSec = 0
    }

    FileDialog {
        id: screenshotDialog

        // `grabToImage` is asynchronous and the QQuickItemGrabResult is
        // garbage-collected once its JS reference drops, taking the FBO
        // texture with it — losing the image before saveToFile runs.
        // Stashing the result on a QML id keeps it alive across the
        // async boundary; we drop the reference right after saving.
        property var pendingGrab: null

        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: [qsTr("PNG images (*.png)"), qsTr("All files (*)")]
        title: qsTr("Save SOL screenshot")
        onAccepted: {
            const path = Paths.urlToLocalFile(screenshotDialog.selectedFile);
            if (path.length === 0) return;
            term.grabToImage(function(result) {
                screenshotDialog.pendingGrab = result;
                result.saveToFile(path);
                screenshotDialog.pendingGrab = null;
            });
        }
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
                text: qsTr("Save screenshot")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                enabled: controller.state === SolController.Connected
                onClicked: root.openScreenshotDialog()
            }

            Rectangle {
                visible: controller.recording
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: Colors.error
                SequentialAnimation on opacity {
                    running: controller.recording
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 0.25; duration: 600 }
                    NumberAnimation { from: 0.25; to: 1.0; duration: 600 }
                }
            }
            Text {
                visible: controller.recording
                text: {
                    const s = recState.elapsedSec;
                    const m = Math.floor(s / 60);
                    return qsTr("REC %1:%2").arg(m).arg(String(s % 60).padStart(2, "0"));
                }
                color: Colors.error
                font.family: Type.mono
                font.pixelSize: Type.sizeXs
                font.features: ({ "tnum": 1 })
            }

            Button {
                text: controller.recording ? qsTr("Stop recording")
                                            : qsTr("Record video")
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                enabled: controller.state === SolController.Connected
                onClicked: {
                    if (controller.recording) controller.stopRecording();
                    else                       root.openRecordDialog();
                }
            }

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
                        root.connectRequested();
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
}
