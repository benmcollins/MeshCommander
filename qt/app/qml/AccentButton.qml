// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

import QtQuick
import QtQuick.Controls.Basic
import QuMesh

/// `Button` styled as a primary / accent action. Qt's Basic `Button {
/// highlighted: true }` reads from `palette.dark` for the background
/// and `palette.brightText` for the text, both of which we'd otherwise
/// have to repurpose just to keep that one button readable. Easier to
/// own the contentItem/background here and pin everything to the
/// `Colors.accent` token directly so the colour stays consistent
/// across themes.
Button {
    id: root

    readonly property color _fg: "#FFFFFF"
    readonly property color _bg: Colors.accent
    readonly property color _bgHover: Qt.lighter(Colors.accent, 1.10)
    readonly property color _bgDown: Qt.darker(Colors.accent, 1.10)
    readonly property color _bgDisabled: Colors.borderMuted
    readonly property color _fgDisabled: Colors.textFaint

    palette.buttonText: enabled ? _fg : _fgDisabled

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.enabled ? root._fg : root._fgDisabled
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: !root.enabled ? root._bgDisabled
            : root.down ? root._bgDown
            : root.hovered ? root._bgHover
            : root._bg
        border.width: root.activeFocus ? 2 : 0
        border.color: Qt.lighter(Colors.accent, 1.30)
        Behavior on color { ColorAnimation { duration: Motion.fast } }
    }
}
