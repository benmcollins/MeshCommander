// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

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

    Component.onCompleted: MigrationController.checkAndMaybeMigrate()

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        TitleBar {
            Layout.fillWidth: true
            onOpenCertificates: certificatesLoader.launch()
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
                onAddRequested: editPane.startNewComputer()
            }

            ComputerEditPane {
                id: editPane
                row: root.selectedRow
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420
            }
        }

        StatusBar {
            Layout.fillWidth: true
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
}
