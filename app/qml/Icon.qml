// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QuMesh

/// Themed Lucide icon. The CMake bake produces `-slate` / `-light`
/// variants of each Lucide SVG at configure time; this component
/// picks the right one based on the active palette and renders it
/// crisp at the requested pixel size.
Image {
    id: root

    /// Lucide icon name (e.g. "lock", "users", "alarm-clock"). Must
    /// match a file baked into `qrc:/icons/` — see #248 and the
    /// `_lucide_icons` list in `app/CMakeLists.txt`.
    property string name

    /// Rendered size in logical pixels. Matches `sourceSize` so the
    /// SVG rasterises at the target resolution (no upscaling blur).
    property int size: 16

    source: name.length > 0
        ? "qrc:/icons/" + name + (Colors.dark ? "-light" : "-slate") + ".svg"
        : ""
    implicitWidth: size
    implicitHeight: size
    sourceSize.width: size
    sourceSize.height: size
    fillMode: Image.PreserveAspectFit
    smooth: true
    asynchronous: false
}
