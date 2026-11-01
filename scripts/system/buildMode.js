//
//  buildMode.js
//  scripts/system
//
//  The bar's Build button: it turns the native editor's build mode on and off (Build > Build Mode, Ctrl+B), and
//  shows whether it is on. It takes the place of the old Create app's button (ARCHITECTURE.md §5.2.1).
//
//  Distributed under the MIT License.
//

(function () {
    var BUILD_MODE = "Build Mode";
    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    var button = tablet.addButton({
        icon: "icons/tablet-icons/edit-i.svg",
        activeIcon: "icons/tablet-icons/edit-a.svg",
        backgroundColor: "#ff9800", // orange 500, as the old Create button
        text: "Build",   // Caity, 10-24: "new one should say Build"
        sortOrder: 10
    });

    function refresh() {
        button.editProperties({ isActive: Menu.isOptionChecked(BUILD_MODE) });
    }

    function onClicked() {
        Menu.setIsOptionChecked(BUILD_MODE, !Menu.isOptionChecked(BUILD_MODE));
        refresh();
    }

    function onMenuItemEvent(menuItem) {
        if (menuItem === BUILD_MODE) {
            refresh();
        }
    }

    button.clicked.connect(onClicked);
    Menu.menuItemEvent.connect(onMenuItemEvent);
    refresh();

    Script.scriptEnding.connect(function () {
        button.clicked.disconnect(onClicked);
        Menu.menuItemEvent.disconnect(onMenuItemEvent);
        tablet.removeButton(button);
    });
}());
