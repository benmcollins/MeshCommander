// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick
import QuMesh

/// Centralised mapping from our semantic `Colors` tokens to Qt's
/// QPalette roles. Every detached `Window` reads these out into its
/// own `palette` block so the Basic-style controls (Button, Menu,
/// MenuItem, Dialog, TextField, SpinBox, …) follow the dark/light
/// toggle. Without this they'd render against Qt's default light
/// palette and produce dark-on-dark or light-on-light combos.
QtObject {
    // Window / pane backgrounds.
    readonly property color window:        Colors.bg
    readonly property color windowText:    Colors.text

    // Text-input + list backgrounds.
    readonly property color base:          Colors.surface
    readonly property color alternateBase: Colors.elevated
    readonly property color text:          Colors.text
    readonly property color placeholderText: Colors.textFaint

    // Buttons.
    readonly property color button:        Colors.elevated
    readonly property color buttonText:    Colors.text

    // Selection / highlight.
    readonly property color highlight:        Colors.accent
    readonly property color highlightedText:  "#FFFFFF"

    // Disabled state shading — keep readable but visibly dimmer.
    readonly property color disabledText:     Colors.textFaint
    readonly property color disabledButton:   Colors.surface
    readonly property color disabledBase:     Colors.surface

    // Tooltips.
    readonly property color toolTipBase:    Colors.elevated
    readonly property color toolTipText:    Colors.text

    // Frame shades. Qt uses these to draw bevels / separators.
    readonly property color mid:            Colors.border
    readonly property color midlight:       Colors.borderMuted
    readonly property color dark:           Colors.borderMuted
    readonly property color shadow:         Colors.border
    readonly property color light:          Colors.elevated

    // Links (rarely used but Qt expects them).
    readonly property color link:           Colors.accent
    readonly property color linkVisited:    Colors.accent
}
