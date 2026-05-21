// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Remote access (CIRA)" section (was the
/// inline section 7 before #325). Threads through the four CIRA
/// dialogs the section's controls open. See OverviewSection.qml for
/// the file-extract rationale.
Flickable {
    id: root

    required property MachineDetailsController controller
    required property var envDetectionDialog
    required property var mpsServerDialog
    required property var ciraPolicyDialog
    required property var userInitiatedDialog

    contentWidth: width
    contentHeight: ciraCol.implicitHeight + 48
    clip: true

    ColumnLayout {
        id: ciraCol
        spacing: 18
        width: parent.width

        SectionHeader {
            eyebrow: qsTr("REMOTE ACCESS (CIRA)")
            title: {
                const r = controller.remoteAccess;
                if (!r || !r.ok)
                    return controller.busy
                        ? qsTr("Loading…")
                        : qsTr("No CIRA configuration");
                const n = (r.servers || []).length;
                return n === 0
                    ? qsTr("No MPS servers configured")
                    : qsTr("%1 management server(s)").arg(n);
            }
            // Show "Loading…" while the fetch is in flight
            // (MachineDetailsWindow auto-fires it on section switch).
            // Once it returns empty, surface the empty-state noun
            // instead of telling the user to click a button (#378).
            hint: (!controller.remoteAccess || !controller.remoteAccess.ok)
                ? (controller.busy
                    ? qsTr("Loading…")
                    : qsTr("No CIRA configuration"))
                : ""
        }

        // --- Environment detection -------------------
        Section {
            title: qsTr("ENVIRONMENT DETECTION")
            visible: (controller.remoteAccess && controller.remoteAccess.ok) === true
            accent: Colors.accent
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 6
                Layout.fillWidth: true

                Text { text: qsTr("State"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: (controller.remoteAccess.envDetection
                            && controller.remoteAccess.envDetection.enabled)
                            ? qsTr("Enabled") : qsTr("Disabled")
                    color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true
                }
                Text { text: qsTr("Domains"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: (controller.remoteAccess.envDetection
                            && controller.remoteAccess.envDetection.domainsLabel)
                            || qsTr("(none)")
                    color: Colors.text; font.family: Type.mono; font.pixelSize: Type.sizeXs; Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Text { text: qsTr("User initiation"); color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeS }
                Text {
                    text: (controller.remoteAccess.userInitiated
                            && controller.remoteAccess.userInitiated.label)
                            || qsTr("(unknown)")
                    color: Colors.text; font.family: Type.sans; font.pixelSize: Type.sizeS; Layout.fillWidth: true
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("User initiation…")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    onClicked: userInitiatedDialog.openForEdit(
                        controller.remoteAccess.userInitiated)
                }
                FlatButton {
                    text: qsTr("Domains…")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    onClicked: envDetectionDialog.openForEdit(
                        controller.remoteAccess.envDetection)
                }
            }
        }

        // --- Policies (User Initiated / Alert / Periodic) ---
        Section {
            title: qsTr("CONNECTION POLICIES")
            visible: !!(controller.remoteAccess
                         && controller.remoteAccess.ok)
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    visible: ((controller.remoteAccess && controller.remoteAccess.policies) || []).length === 0
                    text: qsTr("(no policies configured)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Repeater {
                    model: (controller.remoteAccess && controller.remoteAccess.policies) || []
                    delegate: ColumnLayout {
                        id: policyRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 2

                        // Make each policy row addressable to screen readers (#379).
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("CIRA policy %1, %2")
                            .arg(policyRow.modelData.name || qsTr("unnamed"))
                            .arg((policyRow.modelData.mpsNamesLabel || "").length > 0
                                ? policyRow.modelData.mpsNamesLabel
                                : qsTr("no servers"))

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: modelData.name || "—"
                                color: Colors.text
                                font.family: Type.sans
                                font.pixelSize: Type.sizeM
                                Layout.preferredWidth: 140
                            }
                            Text {
                                text: (modelData.mpsNamesLabel || "").length > 0
                                    ? modelData.mpsNamesLabel
                                    : qsTr("(no servers)")
                                color: Colors.textMuted
                                font.family: Type.mono
                                font.pixelSize: Type.sizeXs
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            FlatButton {
                                text: qsTr("Edit")
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                onClicked: ciraPolicyDialog.openForEdit(modelData)
                            }
                            FlatButton {
                                text: qsTr("Delete")
                                font.family: Type.sans
                                font.pixelSize: Type.sizeXs
                                destructive: true
                                onClicked: {
                                    ciraPolicyConfirmDialog.ask(
                                        qsTr("Delete CIRA policy?"),
                                        qsTr("Removes the %1 policy. AMT cascades its MPS bindings.").arg(modelData.name),
                                        qsTr("Delete"), true);
                                    ciraPolicyConfirmDialog.pendingName = modelData.name;
                                }
                            }
                        }
                        Text {
                            visible: (modelData.scheduleLabel || "").length > 0
                                  || modelData.tunnelLifeTime > 0
                            text: {
                                let s = "";
                                if ((modelData.scheduleLabel || "").length > 0)
                                    s += modelData.scheduleLabel;
                                if (modelData.tunnelLifeTime > 0) {
                                    if (s.length > 0) s += " · ";
                                    s += qsTr("tunnel %1 s").arg(modelData.tunnelLifeTime);
                                }
                                return s;
                            }
                            color: Colors.textFaint
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    AccentButton {
                        text: qsTr("Add policy…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        // No point letting the user open the
                        // dialog when there are no MPS rows
                        // to bind — AMT requires at least one.
                        enabled: ((controller.remoteAccess
                                    && controller.remoteAccess.servers) || []).length > 0
                        onClicked: ciraPolicyDialog.openForAdd()
                    }
                }
            }
        }

        // --- MPS servers ----------------------------
        Section {
            title: qsTr("MANAGEMENT PRESENCE SERVERS")
            visible: (controller.remoteAccess
                       && controller.remoteAccess.ok) === true
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    visible: ((controller.remoteAccess && controller.remoteAccess.servers) || []).length === 0
                    text: qsTr("(none configured)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Repeater {
                    model: (controller.remoteAccess && controller.remoteAccess.servers) || []
                    delegate: RowLayout {
                        id: mpsRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        // Make each MPS-server row addressable to screen readers (#379).
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("MPS server %1 port %2%3")
                            .arg(mpsRow.modelData.accessInfo || "")
                            .arg(mpsRow.modelData.port)
                            .arg(mpsRow.modelData.cila === true ? qsTr(", CILA") : "")

                        Text {
                            text: qsTr("%1:%2")
                                .arg(modelData.accessInfo || "")
                                .arg(modelData.port)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.preferredWidth: 240
                        }
                        Text {
                            visible: modelData.cila === true
                            text: qsTr("CILA")
                            color: Colors.standby
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.weight: Font.Medium
                            font.letterSpacing: 1
                        }
                        Text {
                            text: (modelData.cn && modelData.cn.length > 0)
                                ? qsTr("CN: %1").arg(modelData.cn)
                                : ""
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        FlatButton {
                            text: qsTr("Edit")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: mpsServerDialog.openForEdit(modelData)
                        }
                        FlatButton {
                            text: qsTr("Delete")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            destructive: true
                            onClicked: {
                                mpsConfirmDialog.ask(
                                    qsTr("Delete MPS server?"),
                                    qsTr("Removes %1 from the AMT CIRA stack. Linked auth credentials cascade.").arg(modelData.name),
                                    qsTr("Delete"), true);
                                mpsConfirmDialog.pendingName = modelData.name;
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    AccentButton {
                        text: qsTr("Add server…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        onClicked: mpsServerDialog.openForAdd()
                    }
                }
            }
        }

        // --- HTTP proxies (AMT 11+) -----------------
        Section {
            title: qsTr("HTTP PROXIES")
            visible: controller.remoteAccess
                  && controller.remoteAccess.httpProxySupported === true
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    visible: (controller.remoteAccess.httpProxies || []).length === 0
                    text: qsTr("(none configured)")
                    color: Colors.textFaint
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                }
                Repeater {
                    model: controller.remoteAccess.httpProxies || []
                    delegate: RowLayout {
                        id: proxyRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        // Make each HTTP-proxy row addressable to screen readers (#379).
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("HTTP proxy %1 port %2%3")
                            .arg(proxyRow.modelData.accessInfo || "")
                            .arg(proxyRow.modelData.port)
                            .arg((proxyRow.modelData.networkDnsSuffix || "").length > 0
                                ? qsTr(", suffix %1").arg(proxyRow.modelData.networkDnsSuffix)
                                : "")

                        Text {
                            text: qsTr("%1:%2")
                                .arg(modelData.accessInfo || "")
                                .arg(modelData.port)
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.preferredWidth: 240
                        }
                        Text {
                            text: modelData.networkDnsSuffix || ""
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        FlatButton {
                            text: qsTr("Delete")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            destructive: true
                            onClicked: controller.deleteHttpProxy(modelData.name || "")
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 8
                    Text {
                        text: qsTr("%1 / 15")
                            .arg((controller.remoteAccess.httpProxies || []).length)
                        color: Colors.textFaint
                        font.family: Type.mono
                        font.pixelSize: Type.sizeXs
                    }
                    Item { Layout.fillWidth: true }
                    AccentButton {
                        text: qsTr("Add proxy…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        enabled: (controller.remoteAccess.httpProxies || []).length < 15
                        onClicked: httpProxyDialog.openForAdd()
                    }
                }
            }
        }
    }
}
