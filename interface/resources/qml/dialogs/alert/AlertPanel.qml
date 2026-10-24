//
//  AlertPanel.qml
//  interface/resources/qml/dialogs/alert
//
//  Created on 2026-10-19 for Darkride's popups.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2

// The panel every alert is drawn on, in the manner of a Mac alert: Darkride's picture, a bold title, a short text, what
// the alert asks for (a field, a list), details behind a small Details fold-out in a scrolling field, and the buttons:
// side by side when there are two, the default on the right; stacked when there are more, the default on top. Return
// is the default button and Escape the cancel button: the window holding the panel calls accept() and reject().
Rectangle {
    id: panel

    // 1 on the desktop; larger in the tablet, which is seen from further away.
    property real unit: 1
    property string title
    property string text
    property string detailedText
    // Each { text, value, role, cancel }: role "default" (blue, answers Return), "destructive" (red words: it deletes
    // or replaces something for good, so it is never the default) or "plain"; cancel true for the button that answers
    // Escape. clicked() gives the button's value.
    property var buttons: []
    // What the alert asks for, between the text and the buttons.
    default property alias fields: fieldColumn.data

    signal clicked(var value)

    // A box with a text and no title, as a script's alert or prompt gives, shows its text as the title.
    readonly property string shownTitle: title !== "" ? title : text
    readonly property string shownText: title !== "" ? text : ""

    readonly property var defaultButton: firstWhere(function(button) { return button.role === "default"; })
    readonly property var cancelButton: firstWhere(function(button) { return button.cancel === true; })
    property bool detailsOpen: false

    function firstWhere(test) {
        for (var i = 0; i < buttons.length; i++) {
            if (test(buttons[i])) {
                return buttons[i];
            }
        }
        return null;
    }

    function accept() {
        if (defaultButton) {
            clicked(defaultButton.value);
        }
    }

    function reject() {
        if (cancelButton) {
            clicked(cancelButton.value);
        }
    }

    // Two buttons sit side by side when both their words fit; otherwise the buttons stack.
    readonly property bool sideBySide: {
        if (buttons.length !== 2) {
            return false;
        }
        var room = (width - 32 * unit - 8 * unit) / 2 - 20 * unit;
        return buttonMetrics.advanceWidth(buttons[0].text) <= room && buttonMetrics.advanceWidth(buttons[1].text) <= room;
    }

    // The buttons in the order they show. Side by side, Cancel is always on the left (Caity's rule, 10-19), even when
    // it is the default because the other button deletes or replaces something; without a Cancel, the default is on
    // the right. Stacked, the default is first and Cancel last.
    readonly property var shownButtons: {
        if (sideBySide && cancelButton) {
            return [cancelButton].concat(buttons.filter(function(button) { return button !== cancelButton; }));
        }
        var others = [];
        for (var i = 0; i < buttons.length; i++) {
            if (buttons[i] !== defaultButton) {
                others.push(buttons[i]);
            }
        }
        if (!defaultButton) {
            return others;
        }
        return sideBySide ? others.concat([defaultButton]) : [defaultButton].concat(others);
    }

    FontMetrics {
        id: buttonMetrics
        font.pixelSize: 13 * panel.unit
        font.weight: Font.Medium
    }

    width: 260 * unit
    implicitHeight: column.implicitHeight + 36 * unit
    radius: 12 * unit
    color: "#2b2b2e"
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.10)

    Column {
        id: column
        x: 16 * unit
        y: 20 * unit
        width: panel.width - 32 * unit

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 64 * unit
            height: 64 * unit
            source: "../../../images/darkride-logo.png"
            sourceSize.width: 128 * unit
            sourceSize.height: 128 * unit
            fillMode: Image.PreserveAspectFit
            mipmap: true
        }

        Text {
            width: parent.width
            topPadding: 12 * unit
            visible: panel.shownTitle !== ""
            text: panel.shownTitle
            textFormat: panel.title !== "" ? Text.PlainText : Text.AutoText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: "#ffffff"
            font.pixelSize: 13 * unit
            font.bold: true
            lineHeightMode: Text.FixedHeight
            lineHeight: 17 * unit
        }

        Text {
            width: parent.width
            topPadding: 6 * unit
            visible: panel.shownText !== ""
            text: panel.shownText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: Qt.rgba(1, 1, 1, 0.74)
            linkColor: "#4c9ffe"
            font.pixelSize: 11 * unit
            lineHeightMode: Text.FixedHeight
            lineHeight: 15 * unit
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Column {
            id: fieldColumn
            width: parent.width
            topPadding: children.length > 0 ? 10 * unit : 0
            spacing: 6 * unit
        }

        Column {
            width: parent.width
            topPadding: 6 * unit
            spacing: 4 * unit
            visible: panel.detailedText !== ""

            QQC2.AbstractButton {
                id: disclose
                anchors.horizontalCenter: parent.horizontalCenter
                implicitWidth: discloseRow.implicitWidth + 12 * unit
                implicitHeight: 22 * unit
                padding: 0
                focusPolicy: Qt.TabFocus
                onClicked: panel.detailsOpen = !panel.detailsOpen
                background: Rectangle {
                    radius: 5 * unit
                    color: disclose.down ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                    border.width: disclose.visualFocus ? 2 : 0
                    border.color: Qt.rgba(10 / 255, 132 / 255, 1, 0.59)
                }
                contentItem: Item {
                    Row {
                        id: discloseRow
                        anchors.centerIn: parent
                        spacing: 3 * unit
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Details"
                            color: Qt.rgba(1, 1, 1, 0.74)
                            font.pixelSize: 11 * unit
                        }
                        // The chevron, pointing right while closed and down while open.
                        Canvas {
                            id: chevron
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10 * unit
                            height: 10 * unit
                            property bool open: panel.detailsOpen
                            onOpenChanged: requestPaint()
                            onPaint: {
                                var context = getContext("2d");
                                context.reset();
                                context.scale(width / 10, height / 10);
                                context.strokeStyle = Qt.rgba(1, 1, 1, 0.74);
                                context.lineWidth = 1.4;
                                context.lineCap = "round";
                                context.lineJoin = "round";
                                context.beginPath();
                                if (open) {
                                    context.moveTo(2, 3.5);
                                    context.lineTo(5, 6.5);
                                    context.lineTo(8, 3.5);
                                } else {
                                    context.moveTo(3.5, 2);
                                    context.lineTo(6.5, 5);
                                    context.lineTo(3.5, 8);
                                }
                                context.stroke();
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 112 * unit
                visible: panel.detailsOpen
                radius: 6 * unit
                color: Qt.rgba(0, 0, 0, 0.28)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)

                QQC2.ScrollView {
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    QQC2.TextArea {
                        text: panel.detailedText
                        textFormat: TextEdit.PlainText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: Qt.rgba(1, 1, 1, 0.78)
                        selectionColor: "#0b6bd8"
                        selectedTextColor: "#ffffff"
                        font.pixelSize: 11 * unit
                        topPadding: 7 * unit
                        bottomPadding: 7 * unit
                        leftPadding: 9 * unit
                        rightPadding: 9 * unit
                        background: null
                    }
                }
            }
        }

        Grid {
            width: parent.width
            topPadding: (panel.detailedText !== "" ? 12 : 16) * unit
            visible: panel.buttons.length > 0
            columns: panel.sideBySide ? 2 : 1
            columnSpacing: 8 * unit
            rowSpacing: 8 * unit

            Repeater {
                model: panel.shownButtons
                AlertButton {
                    unit: panel.unit
                    width: panel.sideBySide ? (column.width - 8 * panel.unit) / 2 : column.width
                    text: modelData.text
                    role: modelData.role
                    onClicked: panel.clicked(modelData.value)
                }
            }
        }
    }
}
