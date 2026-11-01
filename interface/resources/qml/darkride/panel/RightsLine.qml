//
//  RightsLine.qml
//  qml/darkride/panel
//
//  The rights line (ui/RightsLine.h), under the buttons that put something where others see it: one grey sentence,
//  the same everywhere, and "Terms", which opens the Terms' "Your content" paragraph in the browser.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import "."

Text {
    // rich text for an ununderlined link, as the alerts' and Build mode's
    text: MetaverseRequests.rightsLine() + " <a href=\"" + MetaverseRequests.termsUrl() + "\" style=\"color: " + Theme.tint +
          "; text-decoration: none;\">Terms</a>"
    textFormat: Text.RichText
    color: Theme.secondaryText
    font.pixelSize: Theme.detailSize
    wrapMode: Text.WordWrap
    lineHeight: 1.15
    onLinkActivated: Qt.openUrlExternally(link)

    HoverHandler {
        cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
