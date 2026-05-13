pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshCommander

Rectangle {
    id: root

    readonly property int currentState: MigrationController.state
    readonly property bool isImported: currentState === MigrationController.Imported
    readonly property bool isFailed: currentState === MigrationController.Failed
    readonly property bool isMigrating: currentState === MigrationController.Migrating

    implicitHeight: visible ? bannerRow.implicitHeight + 16 : 0
    visible: isImported || isFailed || isMigrating
    color: isFailed ? "#fde2e1" : isMigrating ? "#fff4cc" : "#e3f2e1"

    RowLayout {
        id: bannerRow

        spacing: 8
        anchors.fill: parent
        anchors.margins: 8

        Label {
            wrapMode: Text.WordWrap
            text: MigrationController.message
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Dismiss")
            flat: true
            visible: root.isImported || root.isFailed
            onClicked: root.visible = false
        }
    }
}
