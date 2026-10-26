//
//  CustomQueryDialog.qml
//
//  Created by Zander Otavka on 7/14/16
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.12

import controlsUit 1.0
import stylesUit 1.0
import "../windows"
import "alert"
import "alert/standardButtons.js" as StandardButtons

// The desktop's prompt with several inputs, drawn as an alert: a text field (textInput { text, monospace }), a list
// whose label leads its shown choice (comboBox { label, items, index }) and a checkbox (checkBox { label, checked,
// disableForItems, checkStateOnDisable, warningOnDisable }), the warning showing under the list. selected() gives the
// answers as JSON: { textInput, comboBox, comboBoxText, checkBox }.
ModalWindow {
    id: root
    visible: true
    keyboardOverride: true  // Disable ModalWindow's keyboard.
    frame: AlertShadow { }

    anchors.centerIn: undefined
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.top: parent ? parent.top : undefined
    anchors.topMargin: parent ? Math.max(0, Math.round((parent.height - height) / 3)) : 0

    signal selected(var result);
    signal canceled();

    property int icon: 0  // kept for callers: every alert shows Darkride's picture
    property string text
    property var textInput;
    property var comboBox;
    property var checkBox;
    property var buttonTexts: ({ ok: "Add" })
    property int titleWidth: 0  // ModalWindow's own frame sets it while the window is made

    property bool keyboardEnabled: false
    property bool keyboardRaised: false
    property bool punctuationMode: false
    readonly property bool keyboardShown: keyboardEnabled && keyboardRaised

    property var warning: "";
    property var result;
    property var implicitCheckState: null;

    width: keyboardShown ? Math.max(panel.width, keyboard.keyboardWidth) : panel.width
    height: panel.implicitHeight + (keyboardShown ? keyboard.raisedHeight + 8 : 0)

    function updateCheckbox() {
        if (checkBox && checkBox.disableForItems) {
            var currentItemInDisableList = checkBox.disableForItems.indexOf(menu.currentIndex) >= 0;
            if (currentItemInDisableList) {
                check.enabled = false;
                if (checkBox.checkStateOnDisable !== null && checkBox.checkStateOnDisable !== undefined) {
                    root.implicitCheckState = check.checked;
                    check.checked = checkBox.checkStateOnDisable;
                }
                root.warning = checkBox.warningOnDisable || "";
            } else {
                check.enabled = true;
                if (root.implicitCheckState !== null) {
                    check.checked = root.implicitCheckState;
                    root.implicitCheckState = null;
                }
                root.warning = "";
            }
        }
    }

    function accept() {
        var answers = {};
        if (textInput) {
            answers.textInput = field.text;
        }
        if (comboBox) {
            answers.comboBox = menu.currentIndex;
            answers.comboBoxText = menu.currentText;
        }
        if (checkBox) {
            answers.checkBox = check.enabled ? check.checked : null;
        }
        root.result = JSON.stringify(answers);
        root.selected(root.result);
        root.destroy();
    }

    function cancel() {
        root.result = null;
        root.canceled();
        root.destroy();
    }

    // A window holds one content item, its last child, so all it shows goes in this one. ModalWindow.qml reads hifi.
    Item {
        width: root.width
        height: root.height

        HifiConstants { id: hifi }

        AlertPanel {
            id: panel
            x: parent ? Math.round((parent.width - width) / 2) : 0
            title: root.title
            text: root.text
            buttons: StandardButtons.list(StandardButtons.OK | StandardButtons.CANCEL, StandardButtons.OK, 0, root.buttonTexts)
            onClicked: {
                if (value === StandardButtons.OK) {
                    root.accept();
                } else {
                    root.cancel();
                }
            }

            AlertField {
                id: field
                visible: !!root.textInput
                monospace: !!(root.textInput && root.textInput.monospace)
                placeholderText: root.textInput && root.textInput.placeholderText ? root.textInput.placeholderText : ""
            }

            AlertMenu {
                id: menu
                visible: !!root.comboBox
                model: root.comboBox ? root.comboBox.items : []
                displayText: (root.comboBox && root.comboBox.label ? root.comboBox.label + ": " : "") + currentText
                onActivated: root.updateCheckbox()
            }

            Text {
                width: parent.width
                visible: root.warning !== ""
                text: root.warning
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Qt.rgba(1, 1, 1, 0.74)
                font.pixelSize: 11
                lineHeightMode: Text.FixedHeight
                lineHeight: 15
            }

            AlertCheck {
                id: check
                visible: !!root.checkBox
                text: root.checkBox ? root.checkBox.label : ""
            }
        }

        Keyboard {
            id: keyboard
            raised: root.keyboardShown
            numeric: root.punctuationMode
            y: panel.implicitHeight + 8
        }
    }

    Keys.onPressed: {
        if (!visible) {
            return;
        }
        switch (event.key) {
            case Qt.Key_Escape:
            case Qt.Key_Back:
                event.accepted = true;
                root.cancel();
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
                event.accepted = true;
                root.accept();
                break;
        }
    }

    Component.onCompleted: {
        keyboardEnabled = HMD.active;
        if (textInput && textInput.text !== undefined) {
            field.text = textInput.text;
        }
        if (comboBox && comboBox.index !== undefined) {
            menu.currentIndex = comboBox.index;
        }
        if (checkBox && checkBox.checked !== undefined) {
            check.checked = checkBox.checked;
        }
        updateCheckbox();
        if (textInput) {
            field.forceActiveFocus();
        } else if (comboBox) {
            menu.forceActiveFocus();
        }
    }
}
