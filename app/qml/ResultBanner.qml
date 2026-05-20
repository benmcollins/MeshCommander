// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Shared result/status banner used by per-window error and success
/// surfaces (Machine Details, Certificates, ...). The container window
/// drives `text` and `kind`; the banner self-hides on empty text and
/// optionally exposes a Dismiss button. See #283.
///
/// Pre-#283 each window rolled its own surface — MachineDetailsWindow
/// had a red Rectangle, CertificatesWindow a 12 px error line at the
/// bottom, SetupBinWindow shared one bar between ok/error and used
/// `text.startsWith("Saved")` to disambiguate colour. This component
/// replaces the ad-hoc surfaces with a typed (`kind`) banner that
/// callers can also dismiss.
Rectangle {
    id: root

    /// One of `"info"`, `"success"`, `"warning"`, `"error"`. Drives
    /// the background colour and the leading accent stripe.
    property string kind: "info"

    /// Banner body. Empty string hides the banner.
    property string text: ""

    /// Whether to render a Dismiss button. The default is `true`;
    /// flows that want a persistent surface (in-progress migrations,
    /// say) opt out.
    property bool dismissable: true

    /// Emitted on Dismiss click. Callers typically clear the source
    /// `text` (e.g. controller.lastError = "") in response so the
    /// banner doesn't reappear on the next binding evaluation.
    signal dismissed()

    readonly property color _surface: kind === "error"   ? Colors.bannerFailed
                                    : kind === "warning" ? Colors.bannerRunning
                                    : kind === "success" ? Colors.bannerImported
                                    : Colors.surface
    readonly property color _accent:  kind === "error"   ? Colors.error
                                    : kind === "warning" ? Colors.standby
                                    : kind === "success" ? Colors.on
                                    : Colors.accent

    color: _surface
    visible: opacity > 0.001
    opacity: text.length > 0 ? 1.0 : 0.0
    clip: true
    implicitHeight: opacity > 0.001 ? bannerRow.implicitHeight + 16 : 0

    Behavior on opacity        { NumberAnimation { duration: Motion.normal; easing.type: Motion.easeOut } }
    Behavior on implicitHeight { NumberAnimation { duration: Motion.normal; easing.type: Motion.easeOut } }

    RowLayout {
        id: bannerRow
        spacing: 12
        anchors.fill: parent
        anchors.margins: 8

        Rectangle {
            color: root._accent
            implicitWidth: 3
            Layout.fillHeight: true
        }

        Text {
            text: root.text
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        FlatButton {
            text: qsTr("Dismiss")
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            visible: root.dismissable
            onClicked: root.dismissed()
        }
    }
}
