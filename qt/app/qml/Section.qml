pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import MeshCommander

ColumnLayout {
    id: root

    property string title: ""
    default property alias contentItems: contentColumn.data

    spacing: 12

    RowLayout {
        spacing: 10
        Layout.fillWidth: true

        Text {
            text: root.title
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }

        Rectangle {
            color: Colors.borderMuted
            implicitHeight: 1
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }
    }

    ColumnLayout {
        id: contentColumn
        spacing: 8
        Layout.fillWidth: true
    }
}
