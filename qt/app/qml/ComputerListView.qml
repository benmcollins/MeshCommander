pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import MeshCommander

ColumnLayout {
    id: root

    readonly property int currentRow: list.currentIndex

    signal addRequested

    ListView {
        id: list
        model: ComputerModel
        clip: true
        keyNavigationEnabled: true
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 0
        Layout.fillWidth: true
        Layout.fillHeight: true

        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            id: rowDelegate

            required property int index
            required property string name
            required property string host
            required property int port

            width: ListView.view.width
            highlighted: ListView.isCurrentItem

            onClicked: list.currentIndex = rowDelegate.index

            contentItem: ColumnLayout {
                spacing: 2

                Label {
                    text: rowDelegate.name.length > 0 ? rowDelegate.name : qsTr("(unnamed)")
                    elide: Text.ElideRight
                    font.bold: true
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("%1:%2").arg(rowDelegate.host).arg(rowDelegate.port)
                    elide: Text.ElideRight
                    opacity: 0.6
                    Layout.fillWidth: true
                }
            }
        }

        Label {
            visible: list.count === 0
            opacity: 0.6
            text: qsTr("No computers yet.\nClick + to add one.")
            horizontalAlignment: Text.AlignHCenter
            anchors.centerIn: parent
        }
    }

    RowLayout {
        spacing: 6
        Layout.fillWidth: true
        Layout.margins: 6

        Button {
            text: qsTr("+")
            ToolTip.text: qsTr("Add computer")
            ToolTip.visible: hovered
            onClicked: root.addRequested()
        }

        Button {
            text: qsTr("-")
            enabled: list.currentIndex >= 0
            ToolTip.text: qsTr("Remove selected")
            ToolTip.visible: hovered
            onClicked: ComputerModel.removeAt(list.currentIndex)
        }

        Item { Layout.fillWidth: true }

        Label {
            text: qsTr("%1 computer(s)").arg(list.count)
            opacity: 0.6
        }
    }
}
