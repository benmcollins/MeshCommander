// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma Singleton

import QtQuick

/// Tiny QML-side path helpers. The Qt6 `url` value type doesn't expose
/// `toLocalFile()` to JavaScript, and there is no `Qt.urlToLocalFile`
/// — so converting a `FileDialog.selectedFile` to a path to feed back
/// into the controllers takes a few lines that are easy to get wrong
/// (especially the Windows leading-slash + percent-encoding cases).
/// Centralising it here keeps every FileDialog site identical.
QtObject {
    /// Convert a `url` (typically `FileDialog.selectedFile`) to an
    /// absolute local-filesystem path. Handles `file://` stripping
    /// and percent-decoding. Returns an empty string for an empty /
    /// null URL.
    function urlToLocalFile(u) {
        if (u === undefined || u === null) return "";
        var s = u.toString();
        if (s.indexOf("file://") === 0) s = s.substring(7);
        return decodeURIComponent(s);
    }
}
