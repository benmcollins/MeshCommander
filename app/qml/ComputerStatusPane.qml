// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Right-side pane: shows the static metadata for the currently
/// selected machine (the JSON we have on disk), plus Edit / Delete.
/// No connection is attempted — that only happens when the user
/// double-clicks the row and opens the details window.
Rectangle {
    id: root

    property int row: -1
    readonly property bool hasSelection: row >= 0 && row < ComputerModel.rowCount()

    function _read(role) {
        return ComputerModel.data(ComputerModel.index(root.row, 0), role);
    }

    readonly property string machineName: hasSelection ? (_read(ComputerModel.NameRole) || "") : ""
    readonly property string machineHost: hasSelection ? (_read(ComputerModel.HostRole) || "") : ""
    readonly property string machineUser: hasSelection ? (_read(ComputerModel.UserRole) || "") : ""
    readonly property string machinePass: hasSelection ? (_read(ComputerModel.PassRole) || "") : ""
    readonly property bool   machineTls:  hasSelection ? (_read(ComputerModel.TlsRole)  || false) : false
    readonly property string machineDigestRealm: hasSelection
        ? (_read(ComputerModel.DigestRealmRole) || "") : ""
    readonly property var    machineTrustedFingerprints: hasSelection
        ? (_read(ComputerModel.TrustedFingerprintsRole) || []) : []

    signal addRequested
    signal scanRequested
    signal openDetailsRequested(int row)

    color: Colors.bg

    EditComputerDialog {
        id: editDialog
        onComputerSaved: function(savedRow) {
            if (root.row !== savedRow) root.row = savedRow;
        }
    }

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        Flickable {
            id: flick
            clip: true
            contentWidth: width
            contentHeight: form.implicitHeight + 32
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                id: form
                spacing: 24
                visible: root.hasSelection
                width: flick.width

                ColumnLayout {
                    spacing: 6
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

                    Text {
                        text: root.machineName.length > 0
                              ? root.machineName
                              : qsTr("Unnamed")
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: 22
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.machineHost
                        color: Colors.textMuted
                        font.family: Type.mono
                        font.pixelSize: Type.sizeS
                    }
                    Text {
                        // Hint that selection alone never touches the
                        // machine. Double-click is the explicit connect.
                        text: qsTr("Double-click to open machine details.")
                        color: Colors.textFaint
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        Layout.topMargin: 4
                    }
                }

                Section {
                    title: qsTr("CONNECTION")
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 6
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Host")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                        Text {
                            text: root.machineHost
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("TLS")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                        Text {
                            text: root.machineTls
                                  ? qsTr("Enabled")
                                  : qsTr("Disabled (plaintext)")
                            color: root.machineTls ? Colors.on : Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("User")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                        }
                        Text {
                            text: root.machineUser.length > 0
                                  ? root.machineUser
                                  : qsTr("(not set)")
                            color: Colors.text
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("Realm")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            visible: root.machineDigestRealm.length > 0
                        }
                        Text {
                            text: root.machineDigestRealm
                            color: Colors.textMuted
                            font.family: Type.mono
                            font.pixelSize: Type.sizeS
                            visible: root.machineDigestRealm.length > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Section {
                    title: qsTr("TLS CERTIFICATE PINS")
                    visible: root.machineTls
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24

                    ColumnLayout {
                        spacing: 6
                        Layout.fillWidth: true

                        Text {
                            text: root.machineTrustedFingerprints.length === 0
                                  ? qsTr("No certificate pinned — the next connection will prompt for trust.")
                                  : qsTr("%1 fingerprint(s) trusted on first use:")
                                      .arg(root.machineTrustedFingerprints.length)
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: root.machineTrustedFingerprints
                            delegate: Text {
                                required property string modelData
                                text: modelData
                                color: Colors.textFaint
                                font.family: Type.mono
                                font.pixelSize: Type.sizeXs
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                Section {
                    title: qsTr("ACTIONS")
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 24

                    Flow {
                        spacing: 8
                        Layout.fillWidth: true

                        AccentButton {
                            text: qsTr("Open details…")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
enabled: root.machineHost.length > 0 && root.machineUser.length > 0
                            onClicked: root.openDetailsRequested(root.row)
                        }
                        FlatButton {
                            text: qsTr("Edit settings…")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            onClicked: editDialog.openFor(root.row)
                        }
                        FlatButton {
                            text: qsTr("Delete machine")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            destructive: true
                            onClicked: {
                                confirmDelete.pendingRow = root.row;
                                confirmDelete.ask(
                                    qsTr("Delete machine"),
                                    qsTr("Remove %1 from the list? This does not change anything on the AMT device itself.")
                                        .arg(root.machineName.length > 0
                                             ? root.machineName : root.machineHost),
                                    qsTr("Delete"),
                                    true);
                            }
                        }
                    }
                }
            }
        }
    }

    /// Pre-#368 this was an inline `Dialog` with an `AccentButton`
    /// (blue) for the proceed action. Now routes through the shared
    /// `ConfirmDialog` like every other delete prompt in the app —
    /// the row to remove is pinned on `pendingRow` so the proceed
    /// handler doesn't race a possibly-cleared `root.row`.
    ConfirmDialog {
        id: confirmDelete
        property int pendingRow: -1
        onProceed: {
            if (pendingRow >= 0) ComputerModel.removeAt(pendingRow);
            pendingRow = -1;
        }
        onRejected: pendingRow = -1
    }

    /// First-run onboarding (#380). Shown whenever the user has no row
    /// selected — which on a fresh install also means no rows at all,
    /// since the sidebar can't have a selection without rows. The
    /// wordmark stays as the visual anchor above the card; the card
    /// itself surfaces the three onboarding paths that were otherwise
    /// buried in the sidebar toolbar (Add / Scan) and the
    /// auto-dismissing migration banner (Import).
    ///
    /// "Import from MeshCommander" only renders when
    /// `MigrationController.state !== NoLegacyData`. That covers the
    /// `Imported` / `Failed` / `Migrating` / `Idle`-with-legacy-present
    /// cases without dragging on-demand legacy probing into the
    /// controller. On a clean machine with no legacy install, the CTA
    /// would be a dead end — so we hide it.
    ColumnLayout {
        id: emptyState
        visible: !root.hasSelection
        spacing: 18
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Spacing.pageMargin, 480)

        readonly property bool hasLegacyData:
            MigrationController.state !== MigrationController.NoLegacyData

        AppMark {
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            Layout.alignment: Qt.AlignHCenter
            opacity: 0.5
        }
        Text {
            text: qsTr("QUMESH")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 3
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }

        Section {
            title: qsTr("GET STARTED")
            Layout.fillWidth: true
            Layout.topMargin: 6

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("Add your first machine")
                    color: Colors.text
                    font.family: Type.sans
                    font.pixelSize: Type.sizeL
                    font.weight: Font.Medium
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    text: emptyState.hasLegacyData
                        ? qsTr("Enter an AMT host by hand, scan a subnet for "
                             + "vPro devices, or import your saved machines "
                             + "from MeshCommander.")
                        : qsTr("Enter an AMT host by hand, or scan a subnet "
                             + "for vPro devices.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // Flow so the buttons wrap to a second line on narrow
                // panes without clipping. Keeps "Add machine" first in
                // tab order regardless of pane width.
                Flow {
                    spacing: 8
                    Layout.fillWidth: true
                    Layout.topMargin: 6

                    AccentButton {
                        text: qsTr("Add machine")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        onClicked: root.addRequested()
                    }
                    FlatButton {
                        text: qsTr("Scan subnet…")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        onClicked: root.scanRequested()
                    }
                    // `MigrationController` is a Q_INVOKABLE singleton
                    // so we route the Import CTA directly into it
                    // instead of pushing another signal through Main.qml
                    // — keeps onboarding wiring local to this file.
                    FlatButton {
                        text: qsTr("Import from MeshCommander")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        visible: emptyState.hasLegacyData
                        enabled: MigrationController.state
                            !== MigrationController.Migrating
                        onClicked: MigrationController.checkAndMaybeMigrate()
                    }
                }
            }
        }
    }
}
