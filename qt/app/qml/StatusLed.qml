pragma ComponentBehavior: Bound

import QtQuick
import MeshCommander

Item {
    id: root

    property string ledState: "unknown" // on | off | standby | error | unknown

    implicitWidth: 10
    implicitHeight: 10

    Rectangle {
        radius: width / 2
        color: root.ledState === "on"      ? Colors.on
             : root.ledState === "standby" ? Colors.standby
             : root.ledState === "off"     ? Colors.off
             : root.ledState === "error"   ? Colors.error
             : "transparent"
        border.width: root.ledState === "unknown" ? 1 : 0
        border.color: Colors.textFaint
        anchors.fill: parent

        Behavior on color { ColorAnimation { duration: Motion.normal } }
    }
}
