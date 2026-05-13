pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import MeshCommander

Rectangle {
    id: root

    readonly property int currentState: MigrationController.state
    readonly property bool isImported: currentState === MigrationController.Imported
    readonly property bool isFailed: currentState === MigrationController.Failed
    readonly property bool isMigrating: currentState === MigrationController.Migrating

    color: isFailed   ? Colors.bannerFailed
         : isMigrating ? Colors.bannerRunning
         : isImported  ? Colors.bannerImported
         : "transparent"

    visible: opacity > 0.001
    opacity: (isImported || isFailed || isMigrating) ? 1.0 : 0.0
    clip: true
    implicitHeight: opacity > 0.001 ? bannerRow.implicitHeight + 16 : 0

    Behavior on opacity        { NumberAnimation { duration: Motion.normal; easing.type: Motion.easeOut } }
    Behavior on implicitHeight { NumberAnimation { duration: Motion.normal; easing.type: Motion.easeOut } }

    RowLayout {
        id: bannerRow
        spacing: 12
        anchors.fill: parent
        anchors.margins: 8

        Rectangle {
            color: root.isFailed   ? Colors.error
                 : root.isMigrating ? Colors.standby
                 : Colors.on
            implicitWidth: 3
            Layout.fillHeight: true
        }

        Text {
            text: MigrationController.message
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Dismiss")
            flat: true
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            visible: !root.isMigrating
            onClicked: root.opacity = 0
        }
    }
}
