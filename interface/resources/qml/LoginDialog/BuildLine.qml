//
//  BuildLine.qml
//  qml/LoginDialog
//
//  The viewer's version and build, small and quiet along the bottom of the sign-in page, selectable so a tester can
//  copy it into a bug report (Caity, 10-25: "we must show the build and version number on the login screen"). The same
//  line on the desktop's page, the headset's and the tablet's (About.buildLine).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.4

TextEdit {
    readOnly: true
    selectByMouse: true
    persistentSelection: false
    text: About.buildLine
    color: "#9aa1bd"                // the website's muted text (--dr-muted), quiet on the navy
    selectionColor: "#00afff"       // Darkride's cyan
    selectedTextColor: "#ffffff"
    font.pixelSize: 12
    horizontalAlignment: TextEdit.AlignHCenter
}
