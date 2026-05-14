// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton

import QtQuick

QtObject {
    property bool dark: true

    readonly property color bg:          dark ? "#0D1117" : "#FFFFFF"
    readonly property color surface:     dark ? "#161B22" : "#F6F8FA"
    readonly property color elevated:    dark ? "#1F2530" : "#FFFFFF"
    readonly property color border:      dark ? "#30363D" : "#D0D7DE"
    readonly property color borderMuted: dark ? "#21262D" : "#E6EAF0"

    readonly property color text:        dark ? "#E6EDF3" : "#1F2328"
    readonly property color textMuted:   dark ? "#7D8590" : "#656D76"
    // textFaint must still hit WCAG AA on Colors.bg. Old values
    // (#484F58 dark / #8C959F light) measured ~3.3:1 — failing AA.
    // The lift here keeps the "subtle" feel while meeting 4.5:1.
    readonly property color textFaint:   dark ? "#6E747A" : "#767E88"

    readonly property color accent:      "#3D8BFD"
    readonly property color accentSoft:  dark ? "#1F3A6B" : "#DDEAFB"

    readonly property color on:          "#3FB950"
    readonly property color off:         "#6E7681"
    readonly property color standby:     "#D29922"
    readonly property color error:       "#F85149"

    readonly property color bannerImported: dark ? "#163B1F" : "#DDF0E2"
    readonly property color bannerFailed:   dark ? "#4F1A1A" : "#FCE6E6"
    readonly property color bannerRunning:  dark ? "#4A3B12" : "#FFF4CC"
}
