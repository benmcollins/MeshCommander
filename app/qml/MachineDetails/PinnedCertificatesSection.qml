// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "Pinned certificates" section (was the
/// inline section 8 before #325). Read-only view of the
/// trust-on-first-use fingerprints the AMT-side TLS handshake has
/// already accepted for this machine. See OverviewSection.qml for
/// the file-extract rationale.
ColumnLayout {
    id: root

    /// Pinned SHA-256 fingerprints (uppercase hex, colon-separated)
    /// — sourced from the machine's saved config; passed down from
    /// MachineDetailsWindow.machineTrustedFingerprints.
    required property var machineTrustedFingerprints

    spacing: 18


    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true
        Layout.topMargin: 24
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Text {
            text: qsTr("PINNED CERTIFICATES")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }
        Text {
            text: root.machineTrustedFingerprints.length === 0
                ? qsTr("No certificate pinned yet.")
                : qsTr("%1 SHA-256 fingerprint(s) trusted on first use.")
                    .arg(root.machineTrustedFingerprints.length)
            color: Colors.text
            font.family: Type.sans
            font.pixelSize: Type.sizeS
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    Section {
        title: qsTr("FINGERPRINTS")
        visible: root.machineTrustedFingerprints.length > 0
        Layout.fillWidth: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            Repeater {
                model: root.machineTrustedFingerprints
                delegate: Text {
                    required property string modelData
                    text: modelData
                    color: Colors.textMuted
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
