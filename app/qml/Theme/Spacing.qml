// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    // Standard page-edge inset used by the MachineDetails section
    // headers and surrounding content. Lifted out of the per-section
    // copy-paste in #377 so a future redesign can re-tune the gutter
    // in one place.
    readonly property int pageMargin: 24
}
