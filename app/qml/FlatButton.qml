// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QuMesh

/// `Button { flat: true }` with a keyboard-visible focus ring.
/// Qt's Basic style strips background visuals from flat buttons,
/// which removes the focus indicator too — fails WCAG 2.4.7. This
/// thin wrapper draws a 2 px accent outline on activeFocus while
/// keeping hover/press behavior identical to a stock Button.
///
/// `destructive: true` switches the hover/press wash to an error
/// tint and renders the label in `Colors.error` — used for buttons
/// that delete data or perform other irreversible actions, so the
/// hand falls more obviously on the red side. See #301.
Button {
    id: root

    /// Toggle to mark the button as destructive — applies a red
    /// wash on hover/press and a red label. Default false.
    property bool destructive: false

    flat: true

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.destructive ? Colors.error : Colors.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        // Stock flat-button visuals: faint hover wash, slightly
        // stronger pressed wash, transparent default. Destructive
        // mode swaps the base hover/press tint to a red wash so the
        // button reads as a hot-zone.
        readonly property color tint: root.destructive ? Colors.error : Colors.text
        color: root.down
            ? Qt.rgba(tint.r, tint.g, tint.b, root.destructive ? 0.20 : 0.10)
            : root.hovered
                ? Qt.rgba(tint.r, tint.g, tint.b, root.destructive ? 0.12 : 0.05)
                : "transparent"
        radius: 4

        border.width: root.activeFocus ? 2 : 0
        border.color: root.destructive ? Colors.error : Colors.accent

        Behavior on color { ColorAnimation { duration: Motion.fast } }
        Behavior on border.width { NumberAnimation { duration: Motion.fast } }
    }
}
