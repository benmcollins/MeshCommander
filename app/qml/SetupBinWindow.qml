// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QuMesh

AppWindow {
    id: root

    width: 1100
    height: 720
    minimumWidth: 880
    minimumHeight: 520
    title: qsTr("QuMesh — Setup.bin editor")

    property int selectedRecord: 0
    property var snapshot: ({ version: 2, doNotConsumeRecords: false, records: [] })
    property var catalog: []

    SetupBinController {
        id: controller
        onFileChanged: root.snapshot = controller.fileSnapshot()
        onCurrentPathChanged: root.title = controller.currentPath.length > 0
            ? qsTr("QuMesh — Setup.bin — %1").arg(controller.currentPath)
            : qsTr("QuMesh — Setup.bin editor")
    }

    Component.onCompleted: {
        root.catalog = controller.catalog();
        root.snapshot = controller.fileSnapshot();
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open setup.bin")
        nameFilters: [qsTr("Setup.bin (*.bin)"), qsTr("All files (*)")]
        onAccepted: {
            const path = Paths.urlToLocalFile(openDialog.selectedFile);
            if (!controller.openFromFile(path)) {
                errorBar.text = controller.lastError;
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save setup.bin")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "bin"
        nameFilters: [qsTr("Setup.bin (*.bin)"), qsTr("All files (*)")]
        onAccepted: {
            const path = Paths.urlToLocalFile(saveDialog.selectedFile);
            if (!controller.saveToFile(path)) {
                errorBar.text = controller.lastError;
            } else {
                errorBar.text = qsTr("Saved to %1").arg(path);
            }
        }
    }

    FileDialog {
        id: certFileDialog
        title: qsTr("Choose certificate / blob")
        property int recordIndex: -1
        property int variableIndex: -1
        nameFilters: [qsTr("All files (*)")]
        onAccepted: {
            const path = Paths.urlToLocalFile(certFileDialog.selectedFile);
            if (!controller.setVariableValueFromFile(certFileDialog.recordIndex,
                                                     certFileDialog.variableIndex,
                                                     path)) {
                errorBar.text = controller.lastError;
            }
        }
    }

    Dialog {
        id: addVarDialog
        title: qsTr("Add variable")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        property int recordIndex: -1

        function submit() {
            if (recordIndex < 0 || catalogPicker.currentIndex < 0) return;
            const entry = root.catalog[catalogPicker.currentIndex];
            const idx = controller.addVariable(recordIndex, entry.moduleId, entry.varId);
            if (idx < 0) {
                errorBar.text = qsTr("Couldn't add variable %1/%2")
                    .arg(entry.moduleId).arg(entry.varId);
            }
            addVarDialog.accept();
        }

        contentItem: ColumnLayout {
            spacing: 8
            implicitWidth: 360
            Text {
                text: qsTr("Pick a variable to add. Module 1 is MEBx; module 2 is provisioning.")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ComboBox {
                id: catalogPicker
                Layout.fillWidth: true
                model: root.catalog
                textRole: "name"
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                displayText: currentIndex >= 0 && currentIndex < root.catalog.length
                    ? qsTr("%1/%2 — %3")
                        .arg(root.catalog[currentIndex].moduleId)
                        .arg(root.catalog[currentIndex].varId)
                        .arg(root.catalog[currentIndex].name)
                    : qsTr("(pick one)")
                delegate: ItemDelegate {
                    required property int index
                    required property var modelData
                    width: catalogPicker.width
                    contentItem: Text {
                        text: qsTr("%1/%2 — %3")
                            .arg(modelData.moduleId).arg(modelData.varId).arg(modelData.name)
                        color: Colors.text
                        font.family: Type.sans
                        font.pixelSize: Type.sizeS
                    }
                }
            }
            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Layout.topMargin: 4
                Item { Layout.fillWidth: true }
                FlatButton {
                    text: qsTr("Cancel")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    onClicked: addVarDialog.reject()
                }
                AccentButton {
                    text: qsTr("Add")
                    font.family: Type.sans
                    font.pixelSize: Type.sizeS
                    enabled: catalogPicker.currentIndex >= 0
                    onClicked: addVarDialog.submit()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FlatButton {
                text: qsTr("New")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: controller.newFile(versionBox.currentIndex + 1)
            }
            FlatButton {
                text: qsTr("Open…")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: openDialog.open()
            }
            FlatButton {
                text: qsTr("Save As…")
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onClicked: {
                    const err = controller.validate();
                    if (err.length > 0) {
                        errorBar.text = err;
                        return;
                    }
                    saveDialog.open();
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("Version")
                color: Colors.textMuted
                font.family: Type.sans
                font.pixelSize: Type.sizeXs
            }
            ComboBox {
                id: versionBox
                model: ["v1", "v2", "v3", "v4"]
                currentIndex: Math.max(0, root.snapshot.version - 1)
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onActivated: function(index) {
                    controller.version = index + 1;
                }
            }
            CheckBox {
                id: noConsumeBox
                text: qsTr("Do not consume records")
                checked: root.snapshot.doNotConsumeRecords
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                onToggled: controller.doNotConsumeRecords = checked
            }
        }

        // Status / error bar
        Rectangle {
            id: errorBar
            property string text: ""
            color: text.startsWith("Saved") ? Colors.accentSoft : Colors.surface
            radius: 6
            visible: text.length > 0
            Layout.fillWidth: true
            implicitHeight: visible ? errorText.implicitHeight + 12 : 0

            Text {
                id: errorText
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                text: errorBar.text
                color: Colors.text
                font.family: Type.sans
                font.pixelSize: Type.sizeS
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }
        }

        SplitView {
            orientation: Qt.Horizontal
            Layout.fillWidth: true
            Layout.fillHeight: true

            handle: Rectangle {
                implicitWidth: 1
                color: Colors.border
            }

            // ---- Left pane: records list -----------------------------
            Item {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 220

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        text: qsTr("RECORDS")
                        color: Colors.textMuted
                        font.family: Type.sans
                        font.pixelSize: Type.sizeXs
                        font.letterSpacing: 2
                        font.weight: Font.Medium
                    }

                    ListView {
                        id: recordList
                        clip: true
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: root.selectedRecord
                        model: root.snapshot.records.length

                        delegate: Rectangle {
                            id: recordRow
                            required property int index
                            width: recordList.width
                            implicitHeight: 38
                            color: recordList.currentIndex === recordRow.index
                                ? Colors.accentSoft
                                : "transparent"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.selectedRecord = recordRow.index
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Text {
                                    text: qsTr("Record %1").arg(recordRow.index + 1)
                                    color: Colors.text
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: {
                                        const rec = root.snapshot.records[recordRow.index];
                                        const n = rec ? rec.variables.length : 0;
                                        return n === 1 ? qsTr("1 var") : qsTr("%1 vars").arg(n);
                                    }
                                    color: Colors.textMuted
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        FlatButton {
                            text: qsTr("Add")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            onClicked: {
                                const idx = controller.addRecord();
                                root.selectedRecord = idx;
                            }
                        }
                        FlatButton {
                            text: qsTr("Remove")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeS
                            destructive: true
                            enabled: root.snapshot.records.length > 0
                                  && root.selectedRecord >= 0
                                  && root.selectedRecord < root.snapshot.records.length
                            onClicked: {
                                if (controller.removeRecord(root.selectedRecord)) {
                                    if (root.selectedRecord >= root.snapshot.records.length) {
                                        root.selectedRecord = Math.max(0, root.snapshot.records.length - 1);
                                    }
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ---- Right pane: variables in selected record ------------
            Item {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 480

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: root.snapshot.records.length > 0
                                ? qsTr("RECORD %1").arg(root.selectedRecord + 1)
                                : qsTr("NO RECORDS")
                            color: Colors.textMuted
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            font.letterSpacing: 2
                            font.weight: Font.Medium
                        }

                        Item { Layout.fillWidth: true }

                        CheckBox {
                            id: scrambledBox
                            text: qsTr("Scrambled")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            visible: root.snapshot.records.length > 0
                                  && root.selectedRecord < root.snapshot.records.length
                            checked: visible && root.snapshot.records[root.selectedRecord].scrambled
                            onToggled: controller.setRecordScrambled(root.selectedRecord, checked)
                        }

                        FlatButton {
                            text: qsTr("Add variable…")
                            font.family: Type.sans
                            font.pixelSize: Type.sizeXs
                            enabled: root.snapshot.records.length > 0
                            onClicked: {
                                addVarDialog.recordIndex = root.selectedRecord;
                                addVarDialog.open();
                            }
                        }
                    }

                    ListView {
                        id: variableList
                        clip: true
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 4
                        model: (root.snapshot.records.length > 0
                                && root.selectedRecord < root.snapshot.records.length)
                            ? root.snapshot.records[root.selectedRecord].variables
                            : []

                        delegate: Rectangle {
                            id: varRow
                            required property int index
                            required property var modelData
                            width: variableList.width
                            implicitHeight: 52
                            color: Colors.surface
                            radius: 6
                            border.color: Colors.border
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Text {
                                        text: qsTr("%1/%2  %3")
                                            .arg(varRow.modelData.moduleId)
                                            .arg(varRow.modelData.varId)
                                            .arg(varRow.modelData.name)
                                        color: Colors.text
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeS
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    // Type-aware inline editor.
                                    Loader {
                                        id: editor
                                        Layout.preferredWidth: 220
                                        sourceComponent: {
                                            const tc = varRow.modelData.typeCode;
                                            const hasEnum = varRow.modelData.typeCode === 1
                                                          && (function() {
                                                              for (let i = 0; i < root.catalog.length; ++i) {
                                                                  const c = root.catalog[i];
                                                                  if (c.moduleId === varRow.modelData.moduleId
                                                                      && c.varId === varRow.modelData.varId)
                                                                      return c.enumValues !== undefined;
                                                              }
                                                              return false;
                                                          })();
                                            if (hasEnum) return enumEditor;
                                            if (tc === 0) {
                                                // Binary: cert-add (2/8) takes a file picker;
                                                // everything else is text.
                                                if (varRow.modelData.moduleId === 2
                                                    && varRow.modelData.varId === 8)
                                                    return fileEditor;
                                                return textEditor;
                                            }
                                            if (tc === 1 || tc === 2 || tc === 3) return numberEditor;
                                            if (tc === 4) return guidEditor;
                                            return textEditor;
                                        }
                                    }

                                    FlatButton {
                                        text: qsTr("Remove")
                                        font.family: Type.sans
                                        font.pixelSize: Type.sizeXs
                                        destructive: true
                                        onClicked: controller.removeVariable(
                                            root.selectedRecord, varRow.index)
                                    }
                                }

                                Text {
                                    text: qsTr("Current: %1").arg(varRow.modelData.valueDisplay)
                                    color: Colors.textMuted
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeXs
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // Editor components scoped to the row so each one
                            // sees the right `modelData` via the implicit
                            // parent binding.
                            Component {
                                id: textEditor
                                TextField {
                                    text: varRow.modelData.valueDisplay
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    placeholderText: qsTr("value")
                                    onEditingFinished: controller.setVariableValue(
                                        root.selectedRecord, varRow.index, text)
                                }
                            }
                            Component {
                                id: numberEditor
                                SpinBox {
                                    from: 0
                                    to: varRow.modelData.typeCode === 1
                                        ? 255
                                        : (varRow.modelData.typeCode === 2 ? 65535 : 2147483647)
                                    value: parseInt(varRow.modelData.valueDisplay) || 0
                                    editable: true
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    onValueModified: controller.setVariableValue(
                                        root.selectedRecord, varRow.index, value)
                                }
                            }
                            Component {
                                id: guidEditor
                                TextField {
                                    text: varRow.modelData.valueDisplay
                                    font.family: Type.mono
                                    font.pixelSize: Type.sizeS
                                    placeholderText: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
                                    onEditingFinished: {
                                        if (!controller.setVariableValue(
                                                root.selectedRecord, varRow.index, text)) {
                                            errorBar.text = controller.lastError;
                                        }
                                    }
                                }
                            }
                            Component {
                                id: enumEditor
                                ComboBox {
                                    id: enumBox
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeS
                                    model: (function() {
                                        for (let i = 0; i < root.catalog.length; ++i) {
                                            const c = root.catalog[i];
                                            if (c.moduleId === varRow.modelData.moduleId
                                                && c.varId === varRow.modelData.varId
                                                && c.enumValues !== undefined)
                                                return c.enumValues;
                                        }
                                        return [];
                                    })()
                                    textRole: "label"
                                    currentIndex: (function() {
                                        const cur = parseInt(varRow.modelData.valueDisplay);
                                        for (let i = 0; i < model.length; ++i) {
                                            if (model[i].value === cur) return i;
                                        }
                                        return 0;
                                    })()
                                    onActivated: function(index) {
                                        controller.setVariableValue(
                                            root.selectedRecord, varRow.index,
                                            model[index].value);
                                    }
                                }
                            }
                            Component {
                                id: fileEditor
                                FlatButton {
                                    text: qsTr("Choose file…")
                                    font.family: Type.sans
                                    font.pixelSize: Type.sizeXs
                                    onClicked: {
                                        certFileDialog.recordIndex = root.selectedRecord;
                                        certFileDialog.variableIndex = varRow.index;
                                        certFileDialog.open();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
