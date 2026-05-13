pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshCommander

ApplicationWindow {
    id: root

    property int selectedRow: -1

    width: 970
    height: 760
    minimumWidth: 970
    minimumHeight: 640
    visible: true
    title: qsTr("MeshCommander")

    Component.onCompleted: MigrationController.checkAndMaybeMigrate()

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        MigrationBanner {
            Layout.fillWidth: true
        }

        SplitView {
            orientation: Qt.Horizontal
            Layout.fillWidth: true
            Layout.fillHeight: true

            ComputerListView {
                id: listView
                SplitView.preferredWidth: 320
                SplitView.minimumWidth: 240
                onCurrentRowChanged: root.selectedRow = listView.currentRow
                onAddRequested: editPane.startNewComputer()
            }

            ComputerEditPane {
                id: editPane
                row: root.selectedRow
                SplitView.fillWidth: true
                SplitView.minimumWidth: 360
            }
        }
    }
}
