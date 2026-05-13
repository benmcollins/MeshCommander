import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 970
    height: 760
    minimumWidth: 970
    minimumHeight: 640
    visible: true
    title: qsTr("MeshCommander")

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Label {
            text: qsTr("MeshCommander")
            font.pointSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: qsTr("Qt rewrite — scaffolding only")
            opacity: 0.6
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
