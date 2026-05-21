// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Bordered card row used by the three System Defense rule types
/// (policies, L2 filters, L3/L4 filters). Mirrors the Watchdogs row
/// card (see WatchdogsSection.qml:127) so System Defense reads at the
/// same glance under time pressure (closes #381).
///
/// Provides only the card chrome (surface + border + radius + padding)
/// plus the shared `Chip` inline component used for status badges.
/// Callers supply the inner ColumnLayout themselves, so each row type
/// can lay out its title/chips/actions/metadata however it needs.
Rectangle {
    id: root

    /// Caller's children land inside `body`, padded by the standard
    /// 14 / 8 margins so each card matches the Watchdogs row visually.
    default property alias content: body.data

    color: Colors.surface
    border.color: Colors.borderMuted
    border.width: 1
    radius: 8

    // `Layout.fillWidth` hint for callers that drop this directly into
    // a ColumnLayout (the common case for the System Defense rows).
    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + 16

    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 8
        spacing: 4
    }

    /// Chip-style badge — bordered + tinted pill, sized to its text.
    /// Pulled into the card so all three call sites get the identical
    /// look without duplicating the Rectangle/Text recipe. `emphasized`
    /// switches between the default muted treatment and the accent
    /// tint used for "active" badges (e.g. "default", "bound: <port>").
    component Chip: Rectangle {
        property string text: ""
        property color tint: Colors.textMuted
        property bool emphasized: false

        implicitWidth: chipText.implicitWidth + 14
        implicitHeight: chipText.implicitHeight + 6
        radius: 4
        color: emphasized
            ? Qt.rgba(tint.r, tint.g, tint.b, 0.18)
            : Qt.rgba(tint.r, tint.g, tint.b, 0.10)
        border.color: emphasized
            ? Qt.rgba(tint.r, tint.g, tint.b, 0.40)
            : Colors.borderMuted
        border.width: 1

        Text {
            id: chipText
            anchors.centerIn: parent
            text: parent.text
            color: parent.emphasized ? parent.tint : Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.weight: Font.Medium
            font.letterSpacing: 1
        }
    }
}
