// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QuMesh

/// Standard MachineDetails section header — the eyebrow + title +
/// optional hint stack that opens every section file. Extracted in
/// #377; the eyebrow used to be hand-rolled in 14 different places
/// with subtly drifting font and letter-spacing values.
///
/// Title uses `Type.sizeL` (18) instead of the old hard-coded 20 so
/// the section heading sits on the same 1.25× scale as the rest of
/// the type system. Set `monoTitle: true` on TimeSection where the
/// title is a mono-spaced timestamp.
///
/// `padded` defaults to true: the page-edge gutter is applied here so
/// callers can drop a SectionHeader straight under the section's
/// Flickable/ColumnLayout and get the canonical 24 px frame for free.
/// Set `padded: false` when nesting inside an outer RowLayout that
/// already carries the page margins (e.g. WatchdogsSection where an
/// "Add" button sits to the right of the header).
///
/// Additional hint lines (the records-count line under the AuditLog
/// header, the firmware-ceiling line under the Watchdogs header) can
/// be added as direct children of the SectionHeader; they stack under
/// the primary hint via the default property slot.
ColumnLayout {
    id: root

    required property string eyebrow
    required property string title
    property string hint: ""
    property bool padded: true
    property bool monoTitle: false
    property bool wrapTitle: false

    /// Children land in the extras stack under the primary hint —
    /// used by sections with secondary status lines.
    default property alias extras: extrasColumn.data

    spacing: 4
    Layout.fillWidth: true
    Layout.topMargin: root.padded ? Spacing.pageMargin : 0
    Layout.leftMargin: root.padded ? Spacing.pageMargin : 0
    Layout.rightMargin: root.padded ? Spacing.pageMargin : 0

    Text {
        text: root.eyebrow
        color: Colors.textMuted
        font.family: Type.sans
        font.pixelSize: Type.sizeXs
        font.letterSpacing: 2
        font.weight: Font.Medium
    }

    Text {
        text: root.title
        color: Colors.text
        font.family: root.monoTitle ? Type.mono : Type.sans
        font.pixelSize: Type.sizeL
        wrapMode: root.wrapTitle ? Text.WordWrap : Text.NoWrap
        elide: root.wrapTitle ? Text.ElideNone : Text.ElideRight
        Layout.fillWidth: true
    }

    Text {
        visible: root.hint.length > 0
        text: root.hint
        color: Colors.textFaint
        font.family: Type.sans
        font.pixelSize: Type.sizeXs
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    ColumnLayout {
        id: extrasColumn
        spacing: 4
        Layout.fillWidth: true
    }
}
