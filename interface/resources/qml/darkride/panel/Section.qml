//
//  Section.qml
//  qml/darkride/panel
//
//  A group of rows on one rounded card, with a small heading above it and a note below it, as iOS's grouped lists.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Layouts 1.15
import "."

ColumnLayout {
    id: section

    property string title: ""
    property string footer: ""
    property bool rightsLine: false    // the rights line under the note: the card's rows put something where others see it
    default property alias rows: rowColumn.data

    Layout.fillWidth: true
    spacing: 0

    Text {
        Layout.fillWidth: true
        visible: section.title !== ""
        text: section.title.toUpperCase()
        textFormat: Text.PlainText
        color: Theme.secondaryText
        font.pixelSize: Theme.sectionSize
        font.letterSpacing: 0.3 * Theme.unit
        leftPadding: Theme.cardPadding
        topPadding: 22 * Theme.unit
        bottomPadding: 6 * Theme.unit
    }

    // a card without a heading still stands apart from the one above
    Item {
        visible: section.title === ""
        Layout.preferredHeight: 18 * Theme.unit
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: rowColumn.implicitHeight
        radius: Theme.cardRadius
        color: cardFrame.shown ? "transparent" : Theme.card

        // a skin's card frame, when it gives one (ui/Skins.h)
        SkinFrame {
            id: cardFrame
            part: "card"
            anchors.fill: parent
        }

        Column {
            id: rowColumn
            width: parent.width
        }
    }

    Text {
        Layout.fillWidth: true
        visible: section.footer !== ""
        text: section.footer
        textFormat: Text.PlainText
        color: Theme.secondaryText
        font.pixelSize: Theme.detailSize
        wrapMode: Text.WordWrap
        leftPadding: Theme.cardPadding
        rightPadding: Theme.cardPadding
        topPadding: 6 * Theme.unit
        lineHeight: 1.15
    }

    RightsLine {
        Layout.fillWidth: true
        visible: section.rightsLine
        leftPadding: Theme.cardPadding
        rightPadding: Theme.cardPadding
        topPadding: (section.footer !== "" ? 4 : 6) * Theme.unit
    }
}
