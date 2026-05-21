// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Device certificate store" section (was the
/// inline section 9 before #325). Threads through the four dialogs
/// the section's controls open. See OverviewSection.qml for the
/// file-extract rationale.
Flickable {
    id: root

    required property MachineDetailsController controller
    required property var addCertificateDialog
    required property var issueCertificateDialog
    required property var certDetailsDialog
    required property var tlsModeDialog

    contentWidth: width
    contentHeight: devCertCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: devCertCol
        spacing: 18
        width: parent.width

        SectionHeader {
            eyebrow: qsTr("DEVICE CERTIFICATES")
            title: {
                const s = controller.deviceCertStore;
                if (!s || !s.certificates)
                    return controller.busy
                        ? qsTr("Loading…")
                        : qsTr("No certificates");
                return qsTr("%1 certificate(s)")
                    .arg(s.certificates.length);
            }
            // Show "Loading…" while the fetch is in flight
            // (MachineDetailsWindow auto-fires it on section switch).
            // Once it returns empty, surface the empty-state noun
            // instead of telling the user to click a button (#378).
            hint: (!controller.deviceCertStore || !controller.deviceCertStore.certificates)
                ? (controller.busy
                    ? qsTr("Loading…")
                    : qsTr("No certificates"))
                : ""
        }

        // --- TLS modes ----------------------------------
        Section {
            title: qsTr("TLS MODES")
            visible: !!(controller.deviceCertStore
                         && controller.deviceCertStore.tlsSettings
                         && controller.deviceCertStore.tlsSettings.length > 0)
            accent: Colors.accent
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: (controller.deviceCertStore && controller.deviceCertStore.tlsSettings) || []
                    delegate: RowLayout {
                        id: tlsRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 12

                        // Make each TLS-mode row addressable to screen readers (#379).
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("TLS %1, %2")
                            .arg(tlsRow.modelData.isLocal
                                ? qsTr("local LMS")
                                : qsTr("remote 16993"))
                            .arg(tlsRow.modelData.label || "")

                        Text {
                            text: modelData.isLocal
                                ? qsTr("Local (LMS)")
                                : qsTr("Remote (16993)")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.preferredWidth: 160
                        }
                        Text {
                            text: {
                                let s = modelData.label;
                                if (modelData.mutualAuthentication
                                    && modelData.trustedCnLabel
                                       && modelData.trustedCnLabel.length > 0)
                                    s += " · " + qsTr("Trusted: ")
                                      + modelData.trustedCnLabel;
                                return s;
                            }
                            color: Colors.text
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        FlatButton {
                            text: qsTr("Edit…")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: tlsModeDialog.openForRow(modelData)
                        }
                    }
                }
            }
        }

        // --- Certificates ------------------------------
        Section {
            title: qsTr("CERTIFICATES")
            visible: !!(controller.deviceCertStore
                         && controller.deviceCertStore.certificates
                         && controller.deviceCertStore.certificates.length > 0)
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    FlatButton {
                        text: qsTr("Issue certificate…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        ToolTip.text: qsTr("Generate a fresh key pair on the device and obtain a CA-signed cert")
                        ToolTip.visible: hovered
                        onClicked: issueCertificateDialog.openForIssue()
                    }
                    AccentButton {
                        text: qsTr("Add certificate…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: addCertificateDialog.openForAdd()
                    }
                }
                Repeater {
                    model: (controller.deviceCertStore && controller.deviceCertStore.certificates) || []
                    delegate: Rectangle {
                        id: certRow
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        implicitHeight: certCol.implicitHeight + 12
                        color: index % 2 === 0
                            ? "transparent" : Colors.elevated
                        radius: 4

                        // Make each cert row addressable to screen readers (#379).
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("Certificate %1, issued by %2%3%4%5")
                            .arg(certRow.modelData.subjectCn
                                 || certRow.modelData.subjectRaw
                                 || qsTr("unnamed"))
                            .arg(certRow.modelData.issuerCn
                                 || certRow.modelData.issuerRaw
                                 || qsTr("unknown"))
                            .arg(certRow.modelData.trustedRoot === true
                                ? qsTr(", trusted root") : "")
                            .arg(certRow.modelData.hasPrivateKey === true
                                ? qsTr(", has private key") : "")
                            .arg(certRow.modelData.active === true
                                ? qsTr(", active") : "")

                        ColumnLayout {
                            id: certCol
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.topMargin: 6
                            spacing: 2

                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true

                                Text {
                                    text: modelData.subjectCn
                                       || modelData.subjectRaw
                                       || qsTr("(unnamed cert)")
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeM
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    visible: modelData.trustedRoot === true
                                    text: qsTr("TRUSTED ROOT")
                                    color: Colors.accent
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.weight: Font.Medium
                                    font.letterSpacing: 1
                                }
                                Text {
                                    visible: modelData.hasPrivateKey === true
                                    text: qsTr("KEY")
                                    color: Colors.standby
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.weight: Font.Medium
                                    font.letterSpacing: 1
                                }
                                Text {
                                    visible: modelData.active === true
                                    text: qsTr("ACTIVE")
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    font.weight: Font.Medium
                                    font.letterSpacing: 1
                                }
                            }
                            Text {
                                text: qsTr("Issued by %1")
                                    .arg(modelData.issuerCn
                                         || modelData.issuerRaw
                                         || qsTr("(unknown)"))
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: qsTr("%1 bytes").arg(modelData.derSizeBytes || 0)
                                    color: Colors.textFaint
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    Layout.fillWidth: true
                                }
                                FlatButton {
                                    text: qsTr("Details…")
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    onClicked: certDetailsDialog.openForCert(modelData)
                                }
                                FlatButton {
                                    text: qsTr("Delete")
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    destructive: true
                                    onClicked: {
                                        certConfirmDialog.ask(
                                            modelData.active === true
                                                ? qsTr("Delete the ACTIVE TLS certificate?")
                                                : qsTr("Delete certificate?"),
                                            modelData.active === true
                                                ? qsTr("This cert is currently bound by AMT_TLSCredentialContext. Removing it breaks TLS until a new cert is bound — the device may become unreachable on its TLS port.")
                                                : qsTr("This removes %1 from the device cert store. The AMT handle is freed and cannot be undeleted.").arg(modelData.subjectCn || modelData.subjectRaw || qsTr("the certificate")),
                                            qsTr("Delete"),
                                            true);
                                        certConfirmDialog.pendingInstance = modelData.instanceId;
                                        certConfirmDialog.pendingKind = "cert";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // --- Orphan key pairs -------------------------
        Section {
            title: qsTr("UNASSIGNED PRIVATE KEYS")
            visible: !!(controller.deviceCertStore
                         && controller.deviceCertStore.orphanKeys
                         && controller.deviceCertStore.orphanKeys.length > 0)
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: (controller.deviceCertStore && controller.deviceCertStore.orphanKeys) || []
                    delegate: RowLayout {
                        id: orphanRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        // Make each orphan-key row addressable to screen readers (#379).
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("Orphan key %1, %2 bytes")
                            .arg(orphanRow.modelData.instanceId)
                            .arg(orphanRow.modelData.derSizeBytes)

                        Text {
                            text: qsTr("%1 — %2 bytes")
                                .arg(modelData.instanceId)
                                .arg(modelData.derSizeBytes)
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeXs
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        FlatButton {
                            text: qsTr("Delete")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            destructive: true
                            onClicked: {
                                certConfirmDialog.ask(
                                    qsTr("Delete orphan private key?"),
                                    qsTr("Removes %1 from the AMT key store. Orphan keys have no matching cert, so this is usually safe — but cannot be undone.").arg(modelData.instanceId),
                                    qsTr("Delete"),
                                    true);
                                certConfirmDialog.pendingInstance = modelData.instanceId;
                                certConfirmDialog.pendingKind = "key";
                            }
                        }
                    }
                }
            }
        }
    }
}
