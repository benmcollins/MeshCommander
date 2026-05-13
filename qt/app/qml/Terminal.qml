// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QuMesh

/// Read-only-output terminal surface. The host pane wires keyboard
/// input to the `keyInput` / `controlSequence` signals; the widget
/// itself never mutates anything on the C++ side.
Rectangle {
    id: root

    required property TerminalScreen screen

    signal keyInput(string text)
    signal controlSequence(string sequence)

    readonly property int cellWidth: metrics.advanceWidth
    readonly property int cellHeight: metrics.height
    readonly property int padding: 10

    color: "#07090C"
    border.color: Colors.border
    border.width: 1
    radius: 4
    focus: true
    activeFocusOnTab: true

    Accessible.role: Accessible.Terminal
    Accessible.name: qsTr("Serial console")

    TextMetrics {
        id: metrics
        font.family: Type.mono
        font.pixelSize: Type.sizeM
        text: "M"
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: root.padding
        contentWidth: width
        contentHeight: rows.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: rows
            spacing: 0
            width: flick.width

            Repeater {
                model: root.screen.rows

                Text {
                    required property int index

                    width: rows.width
                    height: root.cellHeight
                    // The leading reference to `screen.version` is what
                    // subscribes this binding to screen updates — without
                    // it the engine would never re-evaluate `lineHtml`.
                    text: root.screen.version >= 0
                          ? root.screen.lineHtml(index)
                          : ""
                    textFormat: Text.RichText
                    color: Colors.text
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    renderType: Text.NativeRendering
                    wrapMode: Text.NoWrap
                }
            }
        }

        Rectangle {
            id: cursor
            visible: root.activeFocus
            width: root.cellWidth
            height: root.cellHeight
            x: root.screen.cursorColumn * root.cellWidth
            y: root.screen.cursorRow * root.cellHeight
            color: Colors.accent
            opacity: 0.55

            SequentialAnimation on opacity {
                running: cursor.visible
                loops: Animation.Infinite
                NumberAnimation { from: 0.55; to: 0.15; duration: 600 }
                NumberAnimation { from: 0.15; to: 0.55; duration: 600 }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.forceActiveFocus()
        acceptedButtons: Qt.LeftButton
    }

    Keys.onPressed: function(event) {
        const ctrl = event.modifiers & Qt.ControlModifier;

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.keyInput("\r"); event.accepted = true; return;
        }
        if (event.key === Qt.Key_Backspace) {
            root.keyInput("\b"); event.accepted = true; return;
        }
        if (event.key === Qt.Key_Tab) {
            root.keyInput("\t"); event.accepted = true; return;
        }
        if (event.key === Qt.Key_Escape) {
            root.keyInput("\x1b"); event.accepted = true; return;
        }
        if (event.key === Qt.Key_Up)    { root.controlSequence("\x1b[A"); event.accepted = true; return; }
        if (event.key === Qt.Key_Down)  { root.controlSequence("\x1b[B"); event.accepted = true; return; }
        if (event.key === Qt.Key_Right) { root.controlSequence("\x1b[C"); event.accepted = true; return; }
        if (event.key === Qt.Key_Left)  { root.controlSequence("\x1b[D"); event.accepted = true; return; }

        if (ctrl && event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
            const c = String.fromCharCode(event.key - Qt.Key_A + 1);
            root.keyInput(c); event.accepted = true; return;
        }
        if (event.text.length > 0 && event.text.charCodeAt(0) >= 0x20) {
            root.keyInput(event.text); event.accepted = true;
        }
    }
}
