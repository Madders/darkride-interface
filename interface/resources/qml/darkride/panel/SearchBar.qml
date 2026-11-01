//
//  SearchBar.qml
//  qml/darkride/panel
//
//  The bar above every collection of things in the panels (Explore's worlds, the avatars in My Files, your friends),
//  always the same (Caity, 10-24): a search field, then the view (Cards, Tiles, List) and the order, and any of the
//  page's own buttons after them (extras). A page pins it in its header and shows the things in a CollectionView.
//  view and sort are remembered under settingsKey. arrange(items) filters and sorts the page's items: an item matches
//  when every word typed is in its title, subtitle or keywords; sorts: [{ text, value, by: function (a, b) }], the
//  chosen one's by orders them (none: as they came).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Layouts 1.15
import "."

ColumnLayout {
    id: bar
    objectName: "searchBar"   // a test types in it

    property string settingsKey: ""
    property string placeholder: "Search"
    readonly property alias text: field.text
    property string search: ""          // what's typed, a moment after the typing stops
    property var views: [{ text: "Cards", value: "cards" }, { text: "Tiles", value: "tiles" },
                         { text: "List", value: "list" }]
    property string view: "cards"
    property var sorts: []
    property string sort: sorts.length > 0 ? sorts[0].value : ""
    default property alias extras: extraRow.data

    signal accepted()                   // Return in the field: search at once

    Layout.fillWidth: true
    spacing: 0

    function clear() {
        field.text = "";
        searchDelay.stop();
        search = "";
    }

    // an item whose title, subtitle or keywords hold every word typed
    function matches(item) {
        var words = search.toLowerCase().split(/\s+/).filter(function (w) { return w !== ""; });
        var haystack = [item.title, item.subtitle, item.keywords].join(" ").toLowerCase();
        return words.every(function (word) { return haystack.indexOf(word) >= 0; });
    }

    // the items that match, in the chosen order (ties as they came); filterHere false: the page searches elsewhere
    function arrange(items, filterHere) {
        var list = (items || []).filter(function (item) { return filterHere === false || matches(item); });
        var chosen = sorts.filter(function (s) { return s.value === sort; })[0];
        if (!chosen || !chosen.by) {
            return list;
        }
        var order = list.map(function (item, index) { return { item: item, index: index }; });
        order.sort(function (a, b) {
            var by = chosen.by(a.item, b.item);
            return by !== 0 ? by : a.index - b.index;
        });
        return order.map(function (entry) { return entry.item; });
    }

    // the saved view and order are read once the bar is made; until then a default set while it is being made is not
    // saved over them
    property bool restored: false
    Component.onCompleted: {
        if (settingsKey === "") {
            restored = true;
            return;
        }
        var savedView = Settings.getValue(settingsKey + ".view", view);
        if (views.some(function (v) { return v.value === savedView; })) {
            view = savedView;
        }
        var savedSort = Settings.getValue(settingsKey + ".sort", sort);
        if (sorts.some(function (s) { return s.value === savedSort; })) {
            sort = savedSort;
        }
        restored = true;
    }
    onViewChanged: if (restored && settingsKey !== "") Settings.setValue(settingsKey + ".view", view)
    onSortChanged: if (restored && settingsKey !== "") Settings.setValue(settingsKey + ".sort", sort)

    Timer {
        id: searchDelay
        interval: 300
        onTriggered: bar.search = field.text.trim()
    }

    // the field: a magnifier, the words, and a clear button while there are any
    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 12 * Theme.unit
        implicitHeight: 34 * Theme.unit
        radius: 10 * Theme.unit
        color: searchFrame.shown ? "transparent" : Theme.fill
        border.width: field.activeFocus && !searchFrame.shown ? 1 : 0
        border.color: Theme.tint

        // a skin's field frame, when it gives one ("checked" while typing)
        SkinFrame {
            id: searchFrame
            part: "field"
            look: field.activeFocus ? "checked" : "image"
            anchors.fill: parent
        }

        Canvas {
            id: magnifier
            x: 10 * Theme.unit
            anchors.verticalCenter: parent.verticalCenter
            width: 14 * Theme.unit
            height: width
            property color ink: Theme.secondaryText
            onInkChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = ink;
                ctx.lineWidth = 1.6 * Theme.unit;
                ctx.lineCap = "round";
                ctx.beginPath();
                ctx.arc(width * 0.42, height * 0.42, width * 0.32, 0, 2 * Math.PI);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(width * 0.66, height * 0.66);
                ctx.lineTo(width * 0.92, height * 0.92);
                ctx.stroke();
            }
        }

        TextInput {
            id: field
            objectName: "searchField"
            anchors.left: magnifier.right
            anchors.leftMargin: 8 * Theme.unit
            anchors.right: clearButton.visible ? clearButton.left : parent.right
            anchors.rightMargin: 8 * Theme.unit
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.text
            selectionColor: Theme.tint
            selectedTextColor: "#ffffff"
            font.pixelSize: Theme.bodySize
            clip: true
            selectByMouse: true
            onTextChanged: searchDelay.restart()
            onAccepted: {
                searchDelay.stop();
                bar.search = text.trim();
                bar.accepted();
            }
            Keys.onEscapePressed: bar.clear()

            Text {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                visible: field.text === ""
                text: bar.placeholder
                color: Theme.tertiaryText
                font.pixelSize: Theme.bodySize
            }
        }

        Rectangle {
            id: clearButton
            visible: field.text !== ""
            anchors.right: parent.right
            anchors.rightMargin: 8 * Theme.unit
            anchors.verticalCenter: parent.verticalCenter
            width: 16 * Theme.unit
            height: width
            radius: width / 2
            color: Theme.tertiaryText
            Text {
                anchors.centerIn: parent
                text: "×"
                color: Theme.background
                font.pixelSize: 13 * Theme.unit
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4 * Theme.unit
                onClicked: bar.clear()
            }
        }
    }

    // the view, the order, and the page's own buttons
    Flow {
        id: extraRow
        Layout.fillWidth: true
        Layout.topMargin: 8 * Theme.unit
        spacing: 8 * Theme.unit

        Segmented {
            objectName: "viewPicker"
            visible: bar.views.length > 1
            model: bar.views
            current: bar.view
            onChosen: bar.view = value
        }
        Segmented {
            objectName: "sortPicker"
            visible: bar.sorts.length > 1
            model: bar.sorts.map(function (s) { return { text: s.text, value: s.value }; })
            current: bar.sort
            onChosen: bar.sort = value
        }
    }
}
