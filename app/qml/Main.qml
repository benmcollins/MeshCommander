// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

ApplicationWindow {
    id: root

    property int selectedRow: -1

    width: 1100
    height: 760
    minimumWidth: 920
    minimumHeight: 600
    visible: true
    title: qsTr("QuMesh")
    color: Colors.bg

    // Drive built-in Qt control colours (Button, Menu, MenuItem, Dialog,
    // TextField, SpinBox, ScrollBar) from the same `Colors` tokens our
    // hand-rolled widgets use, so the dark/light toggle is consistent
    // across the whole UI. Each top-level Window applies this block
    // because palette doesn't propagate across Window boundaries.
    palette.window:           ThemePalette.window
    palette.windowText:       ThemePalette.windowText
    palette.base:             ThemePalette.base
    palette.alternateBase:    ThemePalette.alternateBase
    palette.text:             ThemePalette.text
    palette.placeholderText:  ThemePalette.placeholderText
    palette.button:           ThemePalette.button
    palette.buttonText:       ThemePalette.buttonText
    palette.highlight:        ThemePalette.highlight
    palette.highlightedText:  ThemePalette.highlightedText
    palette.toolTipBase:      ThemePalette.toolTipBase
    palette.toolTipText:      ThemePalette.toolTipText
    palette.mid:              ThemePalette.mid
    palette.midlight:         ThemePalette.midlight
    palette.dark:             ThemePalette.dark
    palette.shadow:           ThemePalette.shadow
    palette.light:            ThemePalette.light
    palette.link:             ThemePalette.link

    Component.onCompleted: {
        Colors.dark = themeSettings.dark;
        MigrationController.checkAndMaybeMigrate();
    }

    Settings {
        id: themeSettings
        category: "theme"
        property bool dark: true
    }

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        TitleBar {
            Layout.fillWidth: true
            onOpenCertificates: certificatesLoader.launch()
            onToggleTheme: {
                Colors.dark = !Colors.dark;
                themeSettings.dark = Colors.dark;
            }
        }

        HeartbeatBar {
            Layout.fillWidth: true
            ToolTip.visible: ActivityHeartbeat.lastFailure.length > 0
                              && hoverHandler.hovered
            ToolTip.text: ActivityHeartbeat.lastFailure
            HoverHandler { id: hoverHandler }
        }

        MigrationBanner {
            Layout.fillWidth: true
        }

        SplitView {
            orientation: Qt.Horizontal
            Layout.fillWidth: true
            Layout.fillHeight: true

            handle: Rectangle {
                implicitWidth: 1
                color: Colors.border
            }

            ComputerListView {
                id: listView
                SplitView.preferredWidth: 360
                SplitView.minimumWidth: 280
                onCurrentRowChanged: root.selectedRow = listView.currentRow
                onAddRequested: statusPane.addRequested()
                onScanRequested: scanDialog.openDialog()
                onLocalAmtRequested: localAmtPromptDialog.openForLocal(
                    LocalAmtProbe.tlsAvailable)
                onOpenDetailsRequested: function(row) { detailsLoader.launchFor(row) }
            }

            ComputerStatusPane {
                id: statusPane
                row: root.selectedRow
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420
                onAddRequested: addDialog.openFor(-1)
                onOpenDetailsRequested: function(row) { detailsLoader.launchFor(row) }
            }
        }

        StatusBar {
            Layout.fillWidth: true
        }
    }

    EditComputerDialog {
        id: addDialog
        onComputerSaved: function(savedRow) {
            root.selectedRow = savedRow;
            listView.setCurrentRow(savedRow);
        }
    }

    ScanDialog {
        id: scanDialog
    }

    LocalAmtPromptDialog {
        id: localAmtPromptDialog
        onCredentialsConfirmed: function(user, password, tls) {
            const row = ComputerModel.addComputer(qsTr("This PC"),
                                                  "127.0.0.1",
                                                  user, password, tls);
            if (row >= 0) {
                root.selectedRow = row;
                listView.setCurrentRow(row);
                detailsLoader.launchFor(row);
            }
        }
    }

    Loader {
        id: certificatesLoader
        active: false
        asynchronous: true

        function launch() {
            active = false;
            active = true;
        }

        onStatusChanged: if (status === Loader.Ready && item !== null) item.visible = true

        sourceComponent: CertificatesWindow {
            onClosing: certificatesLoader.active = false
        }
    }

    Loader {
        id: detailsLoader
        active: false
        asynchronous: true
        property int targetRow: -1

        function launchFor(row) {
            targetRow = row;
            active = false;
            active = true;
        }

        function applyMachine() {
            if (item === null || targetRow < 0) return;
            const idx = ComputerModel.index(targetRow, 0);
            item.targetRow = targetRow;
            item.machineName = ComputerModel.data(idx, ComputerModel.NameRole) || "";
            item.machineHost = ComputerModel.data(idx, ComputerModel.HostRole) || "";
            item.machineUser = ComputerModel.data(idx, ComputerModel.UserRole) || "";
            item.machinePass = ComputerModel.data(idx, ComputerModel.PassRole) || "";
            item.machineTls  = ComputerModel.data(idx, ComputerModel.TlsRole)  || false;
            item.machineTrustedFingerprints =
                ComputerModel.data(idx, ComputerModel.TrustedFingerprintsRole) || [];
            item.machineSshConfig = ComputerModel.sshConfigFor(targetRow) || ({});
            item.visible = true;
        }

        onStatusChanged: if (status === Loader.Ready) applyMachine()

        sourceComponent: MachineDetailsWindow {
            onClosing: detailsLoader.active = false
            onTrustedFingerprintPersistRequested: function(fp) {
                if (detailsLoader.targetRow >= 0)
                    ComputerModel.addTrustedFingerprint(detailsLoader.targetRow, fp);
            }
            onTrustedSshHostKeyPersistRequested: function(fp) {
                if (detailsLoader.targetRow >= 0)
                    ComputerModel.addTrustedSshHostKey(detailsLoader.targetRow, fp);
            }
        }
    }
}
