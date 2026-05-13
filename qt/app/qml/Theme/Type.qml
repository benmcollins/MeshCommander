// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton

import QtQuick

QtObject {
    readonly property string sans: sansRegularLoader.name
    readonly property string mono: monoLoader.name

    readonly property int sizeXs: 11
    readonly property int sizeS:  12
    readonly property int sizeM:  13
    readonly property int sizeL:  16

    property FontLoader sansRegularLoader: FontLoader {
        source: Qt.resolvedUrl("../fonts/IBMPlexSans-Regular.ttf")
    }
    property FontLoader sansMediumLoader: FontLoader {
        source: Qt.resolvedUrl("../fonts/IBMPlexSans-Medium.ttf")
    }
    property FontLoader monoLoader: FontLoader {
        source: Qt.resolvedUrl("../fonts/JetBrainsMono-Regular.ttf")
    }
}
