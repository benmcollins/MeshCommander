// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QuMesh

/// Renders the QuMesh brand mark from the baked SVG resources. The
/// CMake build emits two colour variants — slate (`#1e293b`) for the
/// light theme and a near-white (`#E6EDF3`) for the dark theme — and
/// this component picks the right one based on `Colors.dark`.
///
/// Set `small: true` for renders at or below ~32 px; that swaps the
/// full mark for the simplified variant (no interior lattice) so the
/// detail doesn't muddy.
Image {
    id: root

    property bool small: false

    source: {
        const base = small ? "qumesh-mark-small" : "qumesh-mark";
        const tint = Colors.dark ? "light" : "slate";
        return "qrc:/icons/" + base + "-" + tint + ".svg";
    }
    sourceSize.width: width
    sourceSize.height: height
    smooth: true
    fillMode: Image.PreserveAspectFit
}
