// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Modal "Add range" dialog. Probes a user-supplied IP / range / CIDR
/// with ASF presence pings on UDP/623 and lists Intel AMT firmware that
/// replies. Multi-select rows + "Add selected" inserts ComputerInfo
/// entries via the singleton ComputerModel.
Dialog {
    id: root

    function openDialog() {
        scanner.setComputerModel(ComputerModel);
        open();
    }

    title: qsTr("Scan for AMT devices")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 640
    implicitHeight: Math.min(
        formColumn.implicitHeight + 80,
        (parent ? parent.height : 720) * 0.85)

    ScannerController { id: scanner }

    function submitIfReady() {
        if (scanner.selectedCount <= 0 || scanner.scanning) return;
        scanner.addSelected();
        root.accept();
    }

    contentItem: ColumnLayout {
        id: formColumn
        spacing: 14

        // #423 — Dialog extends Popup, not Item, so Accessible.* must
        // sit on the contentItem (Item-derived ColumnLayout).
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("TARGET")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 10
                Layout.fillWidth: true

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    TextField {
                        id: target
                        // Pre-fill a /24 as the most common starting
                        // point; the user can clear or edit before
                        // scanning. Using `text:` (not an imperative
                        // assignment in openDialog) so the field
                        // initialises declaratively.
                        text: "10.0.0.0/24"
                        placeholderText: "10.0.0.0/24, 10.0.0.5-10.0.0.99, or 10.0.0.5"
                        font.family: Type.mono
                        font.pixelSize: Type.sizeM
                        color: Colors.text
                        enabled: !scanner.scanning
                        Layout.fillWidth: true
                        onAccepted: if (text.length > 0 && !scanner.scanning) scanner.start(text)
                    }

                    AccentButton {
                        text: scanner.scanning ? qsTr("Stop") : qsTr("Scan")
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        enabled: target.text.length > 0
                        onClicked: {
                            if (scanner.scanning) scanner.stop();
                            else scanner.start(target.text);
                        }
                    }
                }

                Text {
                    text: scanner.status.length > 0
                          ? scanner.status
                          : qsTr("Enter a single IP, an inclusive range, or a /16–/32 CIDR. Hosts answering with an Intel pong on UDP/623 will appear below.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Section {
            title: qsTr("RESULTS")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Column header
                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    Text { text: "";                            Layout.preferredWidth: 24 }
                    Text { text: qsTr("Address");               Layout.preferredWidth: 140; color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: qsTr("Name");                  Layout.fillWidth: true;     color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: qsTr("AMT");                   Layout.preferredWidth: 60;  color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: qsTr("Provisioning");          Layout.preferredWidth: 100; color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                    Text { text: qsTr("TLS");                   Layout.preferredWidth: 40;  color: Colors.textMuted; font.family: Type.sans; font.pixelSize: Type.sizeXs }
                }

                Rectangle {
                    color: Colors.borderMuted
                    // `height:` is ignored inside a Layout child — use
                    // Layout.preferredHeight so the 1 px separator
                    // actually renders at one pixel tall.
                    Layout.preferredHeight: 1
                    Layout.fillWidth: true
                }

                ListView {
                    id: resultsList
                    model: scanner
                    clip: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 240
                    Layout.fillHeight: true
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: rowItem
                        required property int index
                        required property string ip
                        required property string reverseDns
                        required property string amtVersion
                        required property string provisioning
                        required property bool tlsCapable
                        required property bool selected

                        width: resultsList.width
                        height: 32
                        color: hover.hovered ? Colors.elevated : "transparent"

                        // Make each scan-result row addressable to screen readers (#379).
                        // Mirror the ComputerListView pattern.
                        Accessible.role: Accessible.ListItem
                        Accessible.name: qsTr("%1, %2, AMT %3, %4, TLS %5")
                            .arg(rowItem.ip)
                            .arg(rowItem.reverseDns.length > 0
                                ? rowItem.reverseDns
                                : qsTr("no PTR"))
                            .arg(rowItem.amtVersion)
                            .arg(rowItem.provisioning)
                            .arg(rowItem.tlsCapable ? qsTr("yes") : qsTr("no"))
                        Accessible.selected: rowItem.selected
                        Accessible.selectable: true

                        HoverHandler { id: hover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            anchors.rightMargin: 4
                            spacing: 8

                            CheckBox {
                                checked: rowItem.selected
                                Layout.preferredWidth: 24
                                onClicked: scanner.setSelected(rowItem.index, checked)
                            }

                            Text {
                                text: rowItem.ip
                                color: Colors.text
                                font.family: Type.mono
                                font.pixelSize: Type.sizeS
                                elide: Text.ElideRight
                                Layout.preferredWidth: 140
                            }

                            Text {
                                text: rowItem.reverseDns.length > 0
                                      ? rowItem.reverseDns
                                      : qsTr("(no PTR)")
                                color: rowItem.reverseDns.length > 0
                                       ? Colors.text
                                       : Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: rowItem.amtVersion
                                color: Colors.textMuted
                                font.family: Type.mono
                                font.pixelSize: Type.sizeS
                                Layout.preferredWidth: 60
                            }

                            Text {
                                text: rowItem.provisioning
                                color: Colors.textMuted
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                Layout.preferredWidth: 100
                            }

                            Text {
                                text: rowItem.tlsCapable ? qsTr("yes") : qsTr("no")
                                color: rowItem.tlsCapable ? Colors.accent : Colors.textFaint
                                font.family: Type.sans
                                font.pixelSize: Type.sizeS
                                Layout.preferredWidth: 40
                            }
                        }

                        Rectangle {
                            color: Colors.borderMuted
                            height: 1
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                        }
                    }

                    Text {
                        visible: scanner.count === 0 && !scanner.scanning
                        anchors.centerIn: parent
                        text: qsTr("No results yet. Click Scan.")
                        color: Colors.textFaint
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                    }
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            FlatButton {
                text: qsTr("Cancel")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    if (scanner.scanning) scanner.stop();
                    root.reject();
                }
            }

            AccentButton {
                text: qsTr("Add %1 machine(s)").arg(scanner.selectedCount)
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: scanner.selectedCount > 0 && !scanner.scanning
                onClicked: root.submitIfReady()
            }
        }
    }
}
