// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuMesh

/// Add a WS-Eventing subscription on the AMT firmware. The user picks
/// a CIM_FilterCollection (from the Phase A read-side catalog), gives
/// a listener URL + delivery mode, and optionally supplies HTTP basic
/// auth credentials that AMT forwards to the listener.
///
/// No edit path — AMT's only "edit" is unsubscribe + resubscribe, and
/// listener-side credentials aren't echoed back on read, so an editor
/// would have stale fields anyway. See #345.
Dialog {
    id: root

    required property var controller

    property string draftFilterInstanceId
    property string draftNotifyUrl
    /// "PushWithAck" (default) | "Push".
    property string draftDeliveryMode: "PushWithAck"
    property string draftUser
    property string draftPass
    property bool revealPass: false

    function openForAdd() {
        const filters = (root.controller.eventSubscriptions
                          && root.controller.eventSubscriptions.filters) || [];
        draftFilterInstanceId = filters.length > 0
            ? (filters[0].instanceId || "") : "";
        draftNotifyUrl = "";
        draftDeliveryMode = "PushWithAck";
        draftUser = "";
        draftPass = "";
        revealPass = false;
        open();
    }

    /// Emitted on Save with the controller-shaped args. The window
    /// dispatches into `controller.subscribeToEventFilter`.
    signal confirmed(string filterInstanceId, string deliveryMode,
                     string notifyUrl, string user, string pass)

    title: qsTr("Subscribe to events")
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    implicitWidth: 520

    function _isValid() {
        const url = draftNotifyUrl.trim();
        return draftFilterInstanceId.length > 0
            && url.length > 0
            && (url.startsWith("http://") || url.startsWith("https://"));
    }
    function submitIfReady() {
        if (!_isValid()) return;
        root.confirmed(root.draftFilterInstanceId,
                        root.draftDeliveryMode,
                        root.draftNotifyUrl.trim(),
                        root.draftUser,
                        root.draftPass);
        root.accept();
    }

    contentItem: ColumnLayout {
        spacing: 14

        Keys.onReturnPressed: function(event) { root.submitIfReady(); event.accepted = true; }
        Keys.onEnterPressed: function(event) { root.submitIfReady(); event.accepted = true; }

        Section {
            title: qsTr("FILTER")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true

                Text {
                    text: qsTr("Pick the firmware filter that defines which events get pushed.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                ComboBox {
                    model: (root.controller.eventSubscriptions
                             && root.controller.eventSubscriptions.filters) || []
                    textRole: "collectionName"
                    Layout.fillWidth: true
                    currentIndex: {
                        const filters = (root.controller.eventSubscriptions
                                          && root.controller.eventSubscriptions.filters) || [];
                        for (let i = 0; i < filters.length; i++) {
                            if (filters[i].instanceId === root.draftFilterInstanceId)
                                return i;
                        }
                        return filters.length > 0 ? 0 : -1;
                    }
                    onActivated: {
                        const filters = (root.controller.eventSubscriptions
                                          && root.controller.eventSubscriptions.filters) || [];
                        if (currentIndex >= 0 && currentIndex < filters.length)
                            root.draftFilterInstanceId = filters[currentIndex].instanceId || "";
                    }
                }
                Text {
                    visible: root.draftFilterInstanceId.length > 0
                    text: root.draftFilterInstanceId
                    color: Colors.textFaint
                    font.family: Type.mono
                    font.pixelSize: Type.sizeXs
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        Section {
            title: qsTr("LISTENER")
            Layout.fillWidth: true

            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 10
                Layout.fillWidth: true

                Text {
                    text: qsTr("URL")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 120
                }
                TextField {
                    placeholderText: qsTr("https://listener.example/notify")
                    text: root.draftNotifyUrl
                    font.family: Type.mono
                    font.pixelSize: Type.sizeM
                    Layout.fillWidth: true
                    onTextEdited: root.draftNotifyUrl = text
                }

                Text {
                    text: qsTr("Delivery mode")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    Layout.preferredWidth: 120
                }
                ComboBox {
                    model: [
                        { label: qsTr("PushWithAck — firmware retries until acknowledged"),
                          value: "PushWithAck" },
                        { label: qsTr("Push — fire-and-forget"),
                          value: "Push" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.draftDeliveryMode === "Push" ? 1 : 0
                    Layout.fillWidth: true
                    onActivated: root.draftDeliveryMode =
                        currentIndex === 1 ? "Push" : "PushWithAck"
                }
            }
        }

        Section {
            title: qsTr("AUTH (OPTIONAL)")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true

                Text {
                    text: qsTr("If the listener requires HTTP basic auth, AMT forwards these credentials. Leave blank otherwise.")
                    color: Colors.textMuted
                    font.family: Type.sans
                    font.pixelSize: Type.sizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Username")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 120
                    }
                    TextField {
                        placeholderText: qsTr("(optional)")
                        text: root.draftUser
                        font.family: Type.sans
                        font.pixelSize: Type.sizeM
                        Layout.fillWidth: true
                        onTextEdited: root.draftUser = text
                    }

                    Text {
                        text: qsTr("Password")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                        Layout.preferredWidth: 120
                    }
                    RowLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        TextField {
                            placeholderText: qsTr("(optional)")
                            text: root.draftPass
                            echoMode: root.revealPass
                                ? TextInput.Normal
                                : TextInput.Password
                            font.family: Type.sans
                            font.pixelSize: Type.sizeM
                            Layout.fillWidth: true
                            onTextEdited: root.draftPass = text
                        }
                        FlatButton {
                            text: root.revealPass ? qsTr("Hide") : qsTr("Show")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            onClicked: root.revealPass = !root.revealPass
                        }
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
                onClicked: root.reject()
            }
            AccentButton {
                text: qsTr("Subscribe")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                enabled: root._isValid()
                onClicked: root.submitIfReady()
            }
        }
    }
}
