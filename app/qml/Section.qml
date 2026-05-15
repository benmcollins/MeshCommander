// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QuMesh

/// Section card. The label/rule pair (small caps + horizontal line)
/// is the standard hierarchy marker across the app. The body sits on
/// a slightly-elevated surface with a 1 px outline so each Section
/// reads as a distinct data card — the monitoring-dashboard idiom.
///
/// `accent` (optional): when set, the title text and the horizontal
/// rule pick up that hue. Use sparingly for the one or two sections
/// that should draw the eye (POWER, primary CTA group).
Rectangle {
    id: root

    property string title: ""
    property color accent: Colors.textMuted
    default property alias contentItems: contentColumn.data

    color: Colors.surface
    border.color: Colors.borderMuted
    border.width: 1
    radius: 10

    // Fall back to natural sizing when callers don't pin width.
    implicitWidth: layout.implicitWidth + 28
    implicitHeight: layout.implicitHeight + 24

    Behavior on color { ColorAnimation { duration: Motion.fast } }
    Behavior on border.color { ColorAnimation { duration: Motion.fast } }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 12

        RowLayout {
            spacing: 10
            Layout.fillWidth: true

            Text {
                text: root.title
                color: root.accent
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                font.letterSpacing: 2
                font.weight: Font.Medium

                Behavior on color { ColorAnimation { duration: Motion.fast } }
            }

            Rectangle {
                color: root.accent === Colors.textMuted
                    ? Colors.borderMuted
                    : Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.35)
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
}
