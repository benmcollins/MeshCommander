// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QuMesh

/// Shared base for our detached top-level Windows (SOL, KVM, IDE-R,
/// Certificates, Machine Details). Applies the theme palette so Qt's
/// Basic-style controls (Button, Menu, Dialog, TextField, …) follow
/// the dark/light toggle. Window.palette doesn't propagate across
/// Window boundaries, so every detached window has to wire it up — we
/// just do it once here and let the others inherit.
Window {
    color: Colors.bg

    palette.window:           ThemePalette.window
    palette.windowText:       ThemePalette.windowText
    palette.base:             ThemePalette.base
    palette.alternateBase:    ThemePalette.alternateBase
    palette.text:             ThemePalette.text
    palette.placeholderText:  ThemePalette.placeholderText
    palette.button:           ThemePalette.button
    palette.buttonText:       ThemePalette.buttonText
    palette.highlight:        ThemePalette.highlight
    palette.highlightedText:  ThemePalette.highlightedText
    palette.toolTipBase:      ThemePalette.toolTipBase
    palette.toolTipText:      ThemePalette.toolTipText
    palette.mid:              ThemePalette.mid
    palette.midlight:         ThemePalette.midlight
    palette.dark:             ThemePalette.dark
    palette.shadow:           ThemePalette.shadow
    palette.light:            ThemePalette.light
    palette.link:             ThemePalette.link
}
