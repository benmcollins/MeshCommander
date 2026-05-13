pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import MeshCommander

Rectangle {
    id: root

    color: Colors.surface
    implicitHeight: 24

    Rectangle {
        color: Colors.border
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    RowLayout {
        spacing: 12
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 1

        Text {
            text: qsTr("READY")
            color: Colors.textMuted
            font.family: Type.mono
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 1
        }

        Item { Layout.fillWidth: true }

        Text {
            text: qsTr("%1 cfg").arg(Qt.application.name)
            color: Colors.textFaint
            font.family: Type.mono
            font.pixelSize: Type.sizeXs
        }
    }
}
