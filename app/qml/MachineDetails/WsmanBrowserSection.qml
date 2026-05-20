// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// MachineDetailsWindow "WSMAN browser" section (was the inline
/// section 16 before #325). Dev tool — see #167. Pulled into its
/// own file; see OverviewSection.qml for the rationale.
ColumnLayout {
    id: root

    required property MachineDetailsController controller

    spacing: 8


    // Pretty-print the raw response so the operator sees
    // indented XML rather than a single line. The QtCore
    // QXmlStreamReader/Writer combo handles namespace
    // declarations correctly.
    function prettyPrint(s) {
        if (!s || s.length === 0) return "";
        // Don't try to format an empty / non-XML payload.
        if (s.indexOf("<") < 0) return s;
        const reader = Qt.createQmlObject(
            'import QtQuick; QtObject {}', wsmanPane);
        // Use the built-in DOMParser equivalent — QML
        // doesn't expose QXmlStreamReader, so a simple
        // bracket-aware reflow is "good enough" for the
        // dev-tool use case. Insert a newline after each
        // `>` and indent based on nesting depth.
        let out = "";
        let depth = 0;
        let i = 0;
        while (i < s.length) {
            if (s[i] === "<") {
                const end = s.indexOf(">", i);
                if (end < 0) { out += s.substring(i); break; }
                const tag = s.substring(i, end + 1);
                const isClose = tag.startsWith("</");
                const isSelf  = tag.endsWith("/>") || tag.startsWith("<?");
                if (isClose && depth > 0) depth--;
                out += "  ".repeat(depth) + tag + "\n";
                if (!isClose && !isSelf) depth++;
                i = end + 1;
            } else {
                const next = s.indexOf("<", i);
                const text = (next < 0 ? s.substring(i)
                                       : s.substring(i, next)).trim();
                if (text.length > 0) out += "  ".repeat(depth) + text + "\n";
                i = next < 0 ? s.length : next;
            }
        }
        return out;
    }

    ColumnLayout {
        spacing: 4
        Layout.fillWidth: true
        Layout.topMargin: 24
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        Text {
            text: qsTr("WSMAN BROWSER")
            color: Colors.textMuted
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
            font.letterSpacing: 2
            font.weight: Font.Medium
        }
        Text {
            text: qsTr("Developer tool — raw Get / Enumerate against any AMT WSMAN class.")
            color: Colors.textFaint
            font.family: Type.sans
            font.pixelSize: Type.sizeXs
        }
    }

    Section {
        title: qsTr("REQUEST")
        accent: Colors.accent
        Layout.fillWidth: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24

        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true

            RowLayout {
                spacing: 8
                Layout.fillWidth: true

                ComboBox {
                    id: wsmanKind
                    model: [ qsTr("Get"), qsTr("Enumerate") ]
                    Layout.preferredWidth: 130
                }
                TextField {
                    id: wsmanClass
                    Layout.fillWidth: true
                    placeholderText: qsTr("e.g. AMT_GeneralSettings")
                    font.family: Type.mono
                    font.pixelSize: Type.sizeS
                }
                AccentButton {
                    text: qsTr("Submit")
                    enabled: !controller.busy && wsmanClass.text.trim().length > 0
                    onClicked: {
                        const selectors = {};
                        const lines = (wsmanSelectors.text || "").split("\n");
                        for (let i = 0; i < lines.length; i++) {
                            const eq = lines[i].indexOf("=");
                            if (eq <= 0) continue;
                            const k = lines[i].substring(0, eq).trim();
                            const v = lines[i].substring(eq + 1).trim();
                            if (k.length > 0) selectors[k] = v;
                        }
                        controller.wsmanBrowse(
                            wsmanClass.text.trim(),
                            wsmanKind.currentIndex === 1 ? "enumerate" : "get",
                            selectors);
                    }
                }
            }

            Text {
                text: qsTr("Selectors (Get only) — one Name=Value per line")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
            }
            TextArea {
                id: wsmanSelectors
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                placeholderText: qsTr("InstanceID=Intel(r) AMT:Whatever")
                font.family: Type.mono
                font.pixelSize: Type.sizeS
                wrapMode: TextArea.NoWrap
                enabled: wsmanKind.currentIndex === 0
            }
        }
    }

    Section {
        title: {
            const r = controller.wsmanBrowseResult || {};
            if (Object.keys(r).length === 0) return qsTr("RESPONSE");
            if (!r.ok) return qsTr("RESPONSE — error");
            if (r.kind === "enumerate")
                return qsTr("RESPONSE — %1 item%2")
                    .arg(r.itemCount)
                    .arg(r.itemCount === 1 ? "" : "s");
            return qsTr("RESPONSE");
        }
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.bottomMargin: 24

        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                visible: !!(controller.wsmanBrowseResult
                      && !controller.wsmanBrowseResult.ok
                      && (controller.wsmanBrowseResult.error || "").length > 0)
                text: controller.wsmanBrowseResult
                    ? (controller.wsmanBrowseResult.error || "")
                    : ""
                color: Colors.textFaint
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    readOnly: true
                    text: wsmanPane.prettyPrint(
                        controller.wsmanBrowseResult
                            ? (controller.wsmanBrowseResult.xml || "")
                            : "")
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    wrapMode: TextArea.NoWrap
                    selectByMouse: true
                }
            }
        }
    }
}
