// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton
pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    readonly property string sans: sansRegularLoader.name
    readonly property string mono: monoLoader.name

    // Scale follows a 1.25× progression for cleaner hierarchy than the
    // older 11/12/13/16 cluster. Reserve sizeXs for chip-style labels;
    // everything else picks from sizeS upward. sizeMicro sits below the
    // scale proper and is reserved for dense mono info (fingerprints,
    // list-row hex) where staying visibly smaller than sizeXs matters.
    readonly property int sizeMicro: 11 // dense mono (fingerprints)
    readonly property int sizeXs: 12   // chip / micro label
    readonly property int sizeS:  13   // body small
    readonly property int sizeM:  15   // body
    readonly property int sizeL:  18   // section heading
    readonly property int sizeXl: 24   // hero

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
