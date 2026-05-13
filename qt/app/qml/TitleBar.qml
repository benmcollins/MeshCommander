pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import MeshCommander

Rectangle {
    id: root

    color: Colors.surface
    implicitHeight: 36

    RowLayout {
        spacing: 12
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16

        Text {
            text: "◇  MESHCOMMANDER"
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }

        Rectangle {
            color: Colors.borderMuted
            implicitWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
        }

        Text {
            text: ComputerModel.rowCount() === 1
                  ? qsTr("1 machine")
                  : qsTr("%1 machines").arg(ComputerModel.rowCount())
            color: Colors.textMuted
            font.family: Type.mono
            font.pixelSize: Type.sizeXs
        }

        Item { Layout.fillWidth: true }
    }

    Rectangle {
        color: Colors.border
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
