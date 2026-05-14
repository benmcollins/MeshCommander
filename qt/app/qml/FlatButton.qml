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
Button {
    id: root

    flat: true

    background: Rectangle {
        // Stock flat-button visuals: faint hover wash, slightly
        // stronger pressed wash, transparent default.
        color: root.down
            ? Qt.rgba(Colors.text.r, Colors.text.g, Colors.text.b, 0.10)
            : root.hovered
                ? Qt.rgba(Colors.text.r, Colors.text.g, Colors.text.b, 0.05)
                : "transparent"
        radius: 4

        border.width: root.activeFocus ? 2 : 0
        border.color: Colors.accent

        Behavior on color { ColorAnimation { duration: Motion.fast } }
        Behavior on border.width { NumberAnimation { duration: Motion.fast } }
    }
}
