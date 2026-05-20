// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    readonly property int fast:   120
    readonly property int normal: 180
    readonly property int slow:   240

    readonly property int easeOut: Easing.OutCubic
    readonly property int easeInOut: Easing.InOutCubic
}
