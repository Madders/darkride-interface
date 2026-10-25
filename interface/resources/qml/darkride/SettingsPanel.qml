//
//  SettingsPanel.qml
//  qml/darkride
//
//  Darkride's Settings panel, in the panels' iOS look (darkride/panel), with four tabs named as Apple's System Settings
//  names them (Caity, 10-23): General (start-up, privacy, controls, statistics, the cache),
//  Displays (quality, rendering with Forward+ and MSAA, the view, nametags), Sound (devices, the microphone, volumes)
//  and Appearance (the panels' theme, built in or loaded from a file). The bar's Settings button opens it in a window
//  on the desktop and on the tablet in a headset (tivoli-scripts' ButtonManager.addQmlButton). It reads and sets the
//  engine's own values through its context properties (Render, Performance, LODManager, AudioScriptingInterface,
//  MyAvatar, Users, AccountServices, MenuInterface, DiskCache) and the theme through PanelTheme; the nametag is the
//  Avatar panel's (AvatarPanel.qml). Every number scrubs from its label, as in the build tools.
//  It replaces the web page of the same name (ARCHITECTURE.md §5.2.1: the first of Darkride's panels in QML).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Layouts 1.15
import DarkridePanels 1.0
import "panel"

Page {
    id: root

    property real unit: 1
    signal sendToScript(var message)   // the tablet's loader connects it; nothing is sent

    // the tablet's loader sizes the page; a desktop window parents it to its content area
    anchors.fill: parent
    title: "Settings"
    tabs: ["General", "Displays", "Sound", "Appearance"]

    // (the file is SettingsPanel.qml, not Settings.qml: in a file of that name, "Settings" would be the file's own
    // type rather than the settings object)
    readonly property string tabSetting: "io.darkride.settings.tab"
    Component.onCompleted: currentTab = Math.max(0, Math.min(3, Settings.getValue(tabSetting, 0)))
    onCurrentTabChanged: Settings.setValue(tabSetting, currentTab)

    // a desktop window keeps the page between openings: read the values without a change signal again on each
    onVisibleChanged: {
        if (visible) {
            general.refresh();
            cache.refresh();
        }
    }

    readonly property real defaultFieldOfView: 120
    readonly property real defaultFarClip: 16384
    readonly property bool inHeadset: HMD.active

    // ==== General ====================================================================================================

    QtObject {
        id: general

        // values without a change signal: read when the page opens or shows
        property bool startAtLastPlace: false
        property bool personalSpace: false
        property bool snapTurn: true
        property bool statistics: false
        property real yawSpeed: 100   // MyAvatar.cpp YAW_SPEED_DEFAULT
        property real pitchSpeed: 75   // PITCH_SPEED_DEFAULT
        property bool collisions: true

        function refresh() {
            startAtLastPlace = MenuInterface.isOptionChecked("Last Location");
            personalSpace = Users.getIgnoreRadiusEnabled();
            snapTurn = MyAvatar.getSnapTurn();
            statistics = MenuInterface.isOptionChecked("Show Statistics");
            yawSpeed = MyAvatar.yawSpeed;
            pitchSpeed = MyAvatar.pitchSpeed;
            collisions = MyAvatar.collisionsEnabled;
        }

        Component.onCompleted: refresh()
    }

    Tab {
        visible: root.currentTab === 0

        Section {
            title: "Start-up"
            ListRow {
                label: "Start in"
                Segmented {
                    model: [{ text: "Home", value: false }, { text: "Last Place", value: true }]
                    current: general.startAtLastPlace
                    onChosen: {
                        MenuInterface.setIsOptionChecked(value ? "Last Location" : "Home ", true);
                        general.refresh();
                    }
                }
            }
        }

        Section {
            title: "Privacy"
            footer: "Personal space keeps other avatars from coming closer than about a metre and a half."

            PickerRow {
                visible: AccountServices.loggedIn
                label: "Who sees you're online"
                current: AccountServices.findableBy === "connections" ? "friends" : AccountServices.findableBy
                // the metaverse's availability: everyone, friends or no one ("connections", gone, meant friends)
                model: [{ text: "Everyone", value: "all" }, { text: "Friends", value: "friends" },
                        { text: "No one", value: "none" }]
                onChosen: AccountServices.findableBy = value
            }
            ListRow {
                label: "Personal space"
                Toggle {
                    checked: general.personalSpace
                    onToggled: {
                        if (checked) {
                            Users.enableIgnoreRadius();
                        } else {
                            Users.disableIgnoreRadius();
                        }
                        general.refresh();
                    }
                }
            }
        }

        Section {
            title: "Controls"

            ListRow {
                label: "Look sideways"
                detail: "degrees a second"
                fill: true
                labelWidth: 130 * Theme.unit
                scrubbable: true
                scrubValue: general.yawSpeed
                scrubFrom: 5
                scrubTo: 300
                scrubStep: 1
                onScrubbed: { MyAvatar.yawSpeed = Math.round(value); general.refresh(); }

                ValueSlider {
                    from: 5; to: 300; stepSize: 1; decimals: 0
                    value: general.yawSpeed
                    onMoved: { MyAvatar.yawSpeed = value; general.refresh(); }
                }
                ResetButton {
                    needed: Math.round(general.yawSpeed) !== 100
                    onClicked: { MyAvatar.yawSpeed = 100; general.refresh(); }
                }
            }
            ListRow {
                label: "Look up and down"
                detail: "degrees a second"
                fill: true
                labelWidth: 130 * Theme.unit
                scrubbable: true
                scrubValue: general.pitchSpeed
                scrubFrom: 5
                scrubTo: 300
                scrubStep: 1
                onScrubbed: { MyAvatar.pitchSpeed = Math.round(value); general.refresh(); }

                ValueSlider {
                    from: 5; to: 300; stepSize: 1; decimals: 0
                    value: general.pitchSpeed
                    onMoved: { MyAvatar.pitchSpeed = value; general.refresh(); }
                }
                ResetButton {
                    needed: Math.round(general.pitchSpeed) !== 75
                    onClicked: { MyAvatar.pitchSpeed = 75; general.refresh(); }
                }
            }
            ListRow {
                // noclip (Caity, 10-25: "should be" in Settings); also the Avatar panel's, and Navigate > Walk Through
                // Walls with its key
                label: "Collide with the world"
                // the menu's Ctrl is the Command key on a Mac
                detail: "off: walk through walls (noclip). Key: " + (Qt.platform.os === "osx" ? "⌘ Shift N" : "Ctrl+Shift+N")
                Toggle {
                    checked: general.collisions
                    onToggled: { MyAvatar.collisionsEnabled = checked; general.refresh(); }
                }
            }
            ListRow {
                label: "Snap turning in VR"
                detail: "turn in steps rather than smoothly"
                Toggle {
                    checked: general.snapTurn
                    onToggled: { MyAvatar.setSnapTurn(checked); general.refresh(); }
                }
            }
            ListRow {
                label: "Statistics"
                detail: "frame rate, ping and loading over the world"
                Toggle {
                    checked: general.statistics
                    onToggled: { MenuInterface.setIsOptionChecked("Show Statistics", checked); general.refresh(); }
                }
            }
        }

        Section {
            title: "Cache"
            footer: "The cache keeps the models, sounds and pictures of the worlds you visit, so a second visit loads "
                + "from this computer. Clearing it makes every world download again."

            ListRow {
                label: "In use"
                Text {
                    text: cache.used.toFixed(2) + " of " + cache.maximum.toFixed(1) + " GB"
                    color: Theme.secondaryText
                    font.pixelSize: Theme.bodySize
                }
            }
            ListRow {
                label: "Largest size"
                detail: "GB"
                scrubbable: true
                scrubValue: cache.maximum
                scrubFrom: 0.5
                scrubTo: 100
                scrubStep: 0.1
                onScrubbed: {
                    DiskCache.maximumCacheSize = Math.round(value * 10) / 10;
                    cache.refresh();
                }

                Field {
                    text: cache.maximum.toFixed(1)
                    validator: DoubleValidator { bottom: 0.5; top: 100; decimals: 1 }
                    onEditingFinished: {
                        DiskCache.maximumCacheSize = parseFloat(text);
                        cache.refresh();
                    }
                }
                ResetButton {
                    onClicked: {
                        DiskCache.restoreDefaultMaximumCacheSize();
                        cache.refresh();
                    }
                }
            }
            ListRow {
                label: "Folder"
                fill: true
                Field {
                    Layout.fillWidth: true
                    text: cache.directory
                    horizontalAlignment: TextInput.AlignLeft
                    onEditingFinished: {
                        DiskCache.cacheDirectory = text;
                        cache.refresh();
                    }
                }
                ResetButton {
                    onClicked: {
                        DiskCache.restoreDefaultCacheDirectory();
                        cache.refresh();
                    }
                }
            }
            ListRow {
                visible: !cache.confirmClear
                action: true
                destructive: true
                label: "Clear Cache…"
                onClicked: cache.confirmClear = true
            }
            // the confirmation, in the popups' manner: Cancel first, the deletion in red and never the default
            ListRow {
                visible: cache.confirmClear
                action: true
                label: "Cancel"
                onClicked: cache.confirmClear = false
            }
            ListRow {
                visible: cache.confirmClear
                action: true
                destructive: true
                label: "Clear the Cache Now"
                onClicked: {
                    DiskCache.clearCache();
                    cache.confirmClear = false;
                    cache.refresh();
                }
            }
        }
    }

    // ==== Displays ===================================================================================================

    Tab {
        visible: root.currentTab === 1

        Section {
            title: "Quality"
            footer: "A preset sets the frame rate, the renderer, its antialiasing, texture memory, shadows and ambient "
                + "occlusion at once. Custom leaves them as they are. World detail is kept apart for the desktop and "
                + "for VR."

            ListRow {
                label: "Preset"
                Segmented {
                    model: [{ text: "Potato", value: 1 }, { text: "Low", value: 2 }, { text: "Medium", value: 3 },
                            { text: "High", value: 4 }, { text: "Custom", value: 0 }]
                    current: Performance.performancePreset
                    onChosen: Performance.performancePreset = value
                }
            }
            ListRow {
                label: "World detail"
                detail: root.inHeadset ? "in VR" : "on the desktop"
                Segmented {
                    model: [{ text: "Low", value: 0 }, { text: "Medium", value: 1 }, { text: "High", value: 2 }]
                    current: LODManager.worldDetail
                    onChosen: LODManager.worldDetail = value
                }
            }
            ListRow {
                label: "Frame rate"
                detail: "frames a second"
                Segmented {
                    model: [{ text: "Display", value: 3 }, { text: "60", value: 2 }, { text: "30", value: 1 },
                            { text: "20", value: 0 }]
                    current: Performance.refreshRateProfile
                    onChosen: Performance.refreshRateProfile = value
                }
            }
            ListRow {
                label: "Texture memory"
                detail: "MB"
                Segmented {
                    model: [{ text: "512", value: 512 }, { text: "1024", value: 1024 }, { text: "2048", value: 2048 },
                            { text: "4096", value: 4096 }, { text: "8192", value: 8192 }]
                    current: Render.maximumTextureMemory
                    onChosen: Render.maximumTextureMemory = value
                }
            }
        }

        Section {
            title: "Rendering"
            readonly property bool forward: Render.renderMethod === 1
            footer: forward
                ? "Forward+ shades every light in one multisampled pass, with point and spot light shadows. "
                  + "MSAA smooths edges: more samples, smoother and slower."
                : "Deferred has ambient occlusion and temporal antialiasing, which Forward+ doesn't."

            ListRow {
                label: "Renderer"
                Segmented {
                    model: [{ text: "Forward+", value: 1 }, { text: "Deferred", value: 0 }]
                    current: Render.renderMethod
                    onChosen: Render.renderMethod = value
                }
            }
            ListRow {
                visible: Render.renderMethod === 1
                label: "Antialiasing"
                detail: "MSAA"
                Segmented {
                    model: [{ text: "Off", value: 1 }, { text: "2×", value: 2 }, { text: "4×", value: 4 },
                            { text: "8×", value: 8 }]
                    current: Render.msaaSamples
                    onChosen: Render.msaaSamples = value
                }
            }
            ListRow {
                visible: Render.renderMethod !== 1
                label: "Antialiasing"
                Segmented {
                    model: [{ text: "None", value: 0 }, { text: "TAA", value: 1 }, { text: "FXAA", value: 2 }]
                    current: Render.antialiasingMethod
                    onChosen: Render.antialiasingMethod = value
                }
            }
            ListRow {
                label: "Shadows"
                Toggle { checked: Render.shadowsEnabled; onToggled: Render.shadowsEnabled = checked }
            }
            ListRow {
                label: "Ambient occlusion"
                detail: Render.renderMethod === 1 ? "Deferred only" : ""
                enabled: Render.renderMethod !== 1
                Toggle {
                    enabled: Render.renderMethod !== 1
                    checked: Render.ambientOcclusionEnabled
                    onToggled: Render.ambientOcclusionEnabled = checked
                }
            }
            ListRow {
                label: "Vsync"
                detail: "wait for the display; off paces frames evenly"
                Toggle { checked: Render.vsyncEnabled; onToggled: Render.vsyncEnabled = checked }
            }
        }

        Section {
            title: "View"
            footer: "Drag a label sideways to change its number: Shift for big steps, Alt for small ones. A resolution "
                + "scale above 1× draws more pixels than the window has: sharper, and slower in proportion."

            ListRow {
                label: "Resolution scale"
                fill: true
                labelWidth: 120 * Theme.unit
                scrubbable: true
                scrubValue: Render.viewportResolutionScale
                scrubFrom: 0.1
                scrubTo: 2
                scrubStep: 0.01
                onScrubbed: Render.viewportResolutionScale = Math.round(value * 100) / 100

                ValueSlider {
                    from: 0.1; to: 2; stepSize: 0.05; decimals: 2; units: "×"
                    value: Render.viewportResolutionScale
                    onMoved: Render.viewportResolutionScale = value
                }
                ResetButton {
                    needed: Math.abs(Render.viewportResolutionScale - 1) > 0.001
                    onClicked: Render.viewportResolutionScale = 1
                }
            }
            ListRow {
                label: "Field of view"
                fill: true
                labelWidth: 120 * Theme.unit
                scrubbable: true
                scrubValue: Render.fieldOfView
                scrubFrom: 30
                scrubTo: 150
                scrubStep: 0.5
                onScrubbed: Render.fieldOfView = Math.round(value)

                ValueSlider {
                    from: 30; to: 150; stepSize: 1; decimals: 0; units: "°"
                    value: Render.fieldOfView
                    onMoved: Render.fieldOfView = value
                }
                ResetButton {
                    needed: Math.round(Render.fieldOfView) !== root.defaultFieldOfView
                    onClicked: Render.fieldOfView = root.defaultFieldOfView
                }
            }
            ListRow {
                label: "Draw distance"
                detail: "metres"
                scrubbable: true
                scrubValue: Render.farClip
                scrubFrom: 10
                scrubTo: 100000
                scrubStep: 10
                onScrubbed: Render.farClip = Math.round(value)

                Field {
                    text: Math.round(Render.farClip)
                    validator: IntValidator { bottom: 10; top: 100000 }
                    onEditingFinished: Render.farClip = parseInt(text)
                }
                ResetButton {
                    needed: Math.round(Render.farClip) !== root.defaultFarClip
                    onClicked: Render.farClip = root.defaultFarClip
                }
            }
        }

        Section {
            title: "Nametags"
            ListRow {
                label: "Show nametags"
                Toggle { checked: Render.nametagsEnabled; onToggled: Render.nametagsEnabled = checked }
            }
            ListRow {
                label: "Show my own"
                Toggle { checked: Render.nametagsShowSelf; onToggled: Render.nametagsShowSelf = checked }
            }
        }
    }

    // ==== Sound ======================================================================================================

    // a list of the audio devices of one direction (input or output), the one in use for this mode ticked
    component DevicePicker: Column {
        id: devicePicker
        property string label: ""
        property var devices                 // AudioScriptingInterface.devices.input or .output
        property bool isInput: true
        property bool open: false
        property string currentName: ""

        width: parent ? parent.width : 0

        // finds the device in use: the model has no "selected" lookup, so each entry says whether it is the one
        Repeater {
            model: devicePicker.devices
            delegate: Item {
                visible: false
                readonly property bool selected: root.inHeadset ? model.selectedHMD : model.selectedDesktop
                onSelectedChanged: if (selected) { devicePicker.currentName = model.devicename; }
                Component.onCompleted: if (selected) { devicePicker.currentName = model.devicename; }
            }
        }

        ListRow {
            label: devicePicker.label
            cardItem: devicePicker
            clickable: true
            onClicked: devicePicker.open = !devicePicker.open
            Text {
                text: devicePicker.currentName
                color: Theme.secondaryText
                font.pixelSize: Theme.bodySize
                elide: Text.ElideMiddle
                Layout.maximumWidth: 260 * Theme.unit
            }
            Text {
                text: devicePicker.open ? "⌃" : "⌄"
                color: Theme.tertiaryText
                font.pixelSize: Theme.bodySize
            }
        }

        Repeater {
            model: devicePicker.open ? devicePicker.devices : null
            delegate: ListRow {
                readonly property bool selected: root.inHeadset ? model.selectedHMD : model.selectedDesktop
                label: model.devicename
                clickable: true
                onClicked: {
                    if (!selected) {
                        if (devicePicker.isInput) {
                            AudioScriptingInterface.setStereoInput(false);   // the next device might not do stereo
                            AudioScriptingInterface.setInputDevice(model.info, root.inHeadset);
                        } else {
                            AudioScriptingInterface.setOutputDevice(model.info, root.inHeadset);
                        }
                    }
                    devicePicker.open = false;
                }
                Text {
                    text: "✓"
                    color: Theme.tint
                    font.pixelSize: Theme.bodySize
                    font.bold: true
                    opacity: selected ? 1 : 0
                }
            }
        }
    }

    // a volume in decibels, 0 the default
    component VolumeRow: ListRow {
        id: volumeRow
        property real gain: 0
        signal setGain(real gain)
        fill: true
        labelWidth: 120 * Theme.unit
        scrubbable: true
        scrubValue: gain
        scrubFrom: -60
        scrubTo: 20
        scrubStep: 0.5
        onScrubbed: setGain(Math.round(value))

        ValueSlider {
            from: -60; to: 20; stepSize: 1; decimals: 0; units: " dB"
            value: volumeRow.gain
            onMoved: volumeRow.setGain(value)
        }
        ResetButton {
            needed: Math.abs(volumeRow.gain) > 0.01
            onClicked: volumeRow.setGain(0)
        }
    }

    // everyone in the world but you, for the mixer (panel/NearbyPeople, shared with the People panel)
    QtObject {
        id: mixer
        property bool open: false
    }

    NearbyPeople {
        id: people
        active: root.visible && root.currentTab === 2
    }

    Tab {
        visible: root.currentTab === 2

        Section {
            title: "Devices"
            footer: "Devices are chosen apart for the desktop and for VR; these are " + (root.inHeadset ? "VR's." : "the desktop's.")

            DevicePicker {
                label: "Microphone"
                devices: AudioScriptingInterface.devices.input
                isInput: true
            }
            DevicePicker {
                label: "Speakers"
                devices: AudioScriptingInterface.devices.output
                isInput: false
            }
        }

        Section {
            title: "Microphone"
            footer: "Push to talk: hold T to speak."

            ListRow {
                label: "Mute"
                Toggle { checked: AudioScriptingInterface.muted; onToggled: AudioScriptingInterface.muted = checked }
            }
            ListRow {
                label: "Level"
                fill: true
                labelWidth: 120 * Theme.unit
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 6 * Theme.unit
                    radius: height / 2
                    color: Theme.sliderTrack
                    Rectangle {
                        width: parent.width * Math.min(1, AudioScriptingInterface.inputLevel)
                        height: parent.height
                        radius: parent.radius
                        color: AudioScriptingInterface.clipping ? Theme.destructive : Theme.on
                    }
                }
            }
            ListRow {
                label: "Input volume"
                fill: true
                labelWidth: 120 * Theme.unit
                scrubbable: true
                scrubValue: AudioScriptingInterface.inputVolume * 100
                scrubFrom: 0
                scrubTo: 100
                scrubStep: 0.5
                onScrubbed: AudioScriptingInterface.inputVolume = Math.round(value) / 100

                ValueSlider {
                    from: 0; to: 100; stepSize: 1; decimals: 0; units: " %"
                    value: AudioScriptingInterface.inputVolume * 100
                    onMoved: AudioScriptingInterface.inputVolume = value / 100
                }
                ResetButton {
                    needed: Math.abs(AudioScriptingInterface.inputVolume - 1) > 0.005
                    onClicked: AudioScriptingInterface.inputVolume = 1
                }
            }
            ListRow {
                label: "Noise reduction"
                Toggle {
                    checked: AudioScriptingInterface.noiseReduction
                    onToggled: AudioScriptingInterface.noiseReduction = checked
                }
            }
            ListRow {
                label: "Echo cancellation"
                Toggle {
                    checked: AudioScriptingInterface.acousticEchoCancellation
                    onToggled: AudioScriptingInterface.acousticEchoCancellation = checked
                }
            }
            ListRow {
                label: "Push to talk"
                detail: root.inHeadset ? "in VR" : "on the desktop"
                Toggle {
                    checked: AudioScriptingInterface.pushToTalk
                    onToggled: AudioScriptingInterface.pushToTalk = checked
                }
            }
            ListRow {
                label: "Warn when muted"
                detail: "when you speak with the microphone off"
                Toggle {
                    checked: AudioScriptingInterface.warnWhenMuted
                    onToggled: AudioScriptingInterface.warnWhenMuted = checked
                }
            }
        }

        Section {
            title: "Volume"
            footer: mixer.open
                ? "A person's volume lasts for this visit. Muting someone is mutual: neither of you hears the other."
                : "People: other avatars' voices. World: the sounds of the world and its scripts. Interface: "
                  + "Darkride's own sounds."

            VolumeRow {
                label: "People"
                gain: AudioScriptingInterface.avatarGain
                onSetGain: AudioScriptingInterface.avatarGain = gain
            }
            // Each person: the mixer, opened under People
            ListRow {
                objectName: "eachPersonRow"   // a test opens the mixer by it
                label: "Each person"
                detail: mixer.open ? "" : "a volume and a mute for everyone here"
                clickable: true
                onClicked: mixer.open = !mixer.open
                Text {
                    text: String(people.count)
                    color: Theme.secondaryText
                    font.pixelSize: Theme.bodySize
                }
                Text {
                    text: mixer.open ? "\u2303" : "\u2304"
                    color: Theme.tertiaryText
                    font.pixelSize: Theme.bodySize
                }
            }
            Repeater {
                model: mixer.open ? people.model : null
                delegate: PersonRow {
                    nearby: people
                    index: model.index
                    name: model.name
                    distance: model.distance
                    level: model.level
                    gain: model.gain
                    muted: model.muted
                    blocked: model.blocked
                }
            }
            ListRow {
                visible: mixer.open && people.count === 0
                label: "No one else is here."
                enabled: false
            }
            VolumeRow {
                label: "World"
                gain: AudioScriptingInterface.serverInjectorGain
                onSetGain: {
                    AudioScriptingInterface.serverInjectorGain = gain;   // sounds played by the world's server
                    AudioScriptingInterface.localInjectorGain = gain;    // and by scripts here
                }
            }
            VolumeRow {
                label: "Interface"
                gain: AudioScriptingInterface.systemInjectorGain
                onSetGain: AudioScriptingInterface.systemInjectorGain = gain
            }
        }
    }

    // ==== Appearance =================================================================================================

    QtObject {
        id: appearance
        property bool awaitingFile: false   // Window.browseChanged answers anyone's browse; only ours is taken
        property bool awaitingSkin: false
        property string status: ""
        property string skinStatus: ""
    }

    Connections {
        target: Window
        function onBrowseChanged(filename) {
            if (appearance.awaitingSkin) {
                appearance.awaitingSkin = false;
                if (filename !== "") {
                    var trouble = Skins.install(filename);
                    appearance.skinStatus = trouble === "" ? "The skin is on." : "Couldn't load the skin: " + trouble + ".";
                }
                return;
            }
            if (!appearance.awaitingFile) {
                return;
            }
            appearance.awaitingFile = false;
            if (filename === "") {
                return;
            }
            var problem = PanelTheme.loadFile(filename);
            appearance.status = problem === "" ? "Loaded " + Theme.schemeName + "." : "Couldn't load it: " + problem;
        }
    }

    Tab {
        visible: root.currentTab === 3

        // where the bar of app buttons sits (ui/SystemToolbar.h): over the bottom of the view, or on a side of the
        // window with the view beside it (a player's ask, 10-24)
        Section {
            title: "Layout"
            footer: SystemToolbar.docked ? "On a side, the view sits beside the bar. Drag the bar by its handle to move it."
                : "Floating, the bar sits over the bottom of the view."

            ListRow {
                label: "Bar"
                Segmented {
                    model: [{ text: "Left", value: "left" }, { text: "Top", value: "top" },
                            { text: "Right", value: "right" }, { text: "Bottom", value: "bottom" },
                            { text: "Floating", value: "floating" }]
                    current: SystemToolbar.place
                    onChosen: SystemToolbar.place = value
                }
            }
        }

        // skins (ui/Skins.h, SKINS.md): Darkride's own look, or one installed; Load Skin... takes a .zip or a skin.json
        Section {
            title: "Skin"
            footer: appearance.skinStatus !== "" ? appearance.skinStatus
                : "A skin is pictures, colours and fonts only: nothing in it runs, so a skin from anyone is safe to load."

            PickerRow {
                label: "Skin"
                current: Skins.current
                model: [{ text: "Darkride", value: "" }].concat(Skins.installed.map(function (skin) {
                    return { text: skin.name + (skin.author ? " · " + skin.author : ""), value: skin.id };
                }))
                onChosen: {
                    var trouble = Skins.setCurrent(value);
                    appearance.skinStatus = trouble === "" ? "" : "Couldn't put it on: " + trouble + ".";
                }
            }
            ListRow {
                action: true
                label: "Load Skin…"
                onClicked: {
                    appearance.awaitingSkin = true;
                    Window.browseAsync("Load a Skin", "", "Skins (*.zip *.json)");
                }
            }
            ListRow {
                visible: Skins.current !== ""
                action: true
                destructive: true
                label: "Remove This Skin"
                onClicked: appearance.skinStatus = Skins.remove(Skins.current)
            }
        }

        Section {
            title: "Theme"
            footer: appearance.status !== "" ? appearance.status
                : "A theme file holds colours only, so a theme from anyone is safe to load."

            PickerRow {
                label: "Colours"
                current: PanelTheme.scheme
                model: {
                    var choices = Theme.schemeOrder.map(function (key) {
                        var scheme = Theme.schemes[key];
                        return { text: scheme.name, value: key,
                                 swatches: [scheme.background, scheme.card, scheme.tint, scheme.on] };
                    });
                    if (PanelTheme.custom && PanelTheme.custom.name) {
                        var custom = PanelTheme.custom;
                        var base = Theme.schemes[custom.base] || Theme.schemes.graphite;
                        choices.push({ text: custom.name, value: "custom",
                                       swatches: [custom.background || base.background, custom.card || base.card,
                                                  custom.tint || base.tint, custom.on || base.on] });
                    }
                    return choices;
                }
                onChosen: {
                    PanelTheme.setScheme(value);
                    appearance.status = "";
                }
            }
            ListRow {
                action: true
                label: "Load Theme…"
                onClicked: {
                    appearance.awaitingFile = true;
                    Window.browseAsync("Load a Theme", "", "Themes (*.json)");
                }
            }
        }

        Section {
            title: "Theme files"
            footer: "A theme is a JSON file: a name, the built-in scheme it starts from, and any of the colours "
                + "background, card, cardPressed, separator, text, secondaryText, tertiaryText, tint, on, destructive, "
                + "fill, segmentThumb and sliderTrack, as #rrggbb or #aarrggbb. For example: "
                + "{ \"name\": \"Sunset\", \"base\": \"dusk\", \"tint\": \"#ff9f0a\", \"on\": \"#ff9f0a\" }"
        }
    }

    // ==== the cache, read by General =================================================================================

    QtObject {
        id: cache

        // the disk cache's values carry no change signal: read again when the page opens and after each change;
        // both sizes are in GB
        property real used: 0
        property real maximum: 0
        property string directory: ""
        property bool confirmClear: false

        function refresh() {
            used = DiskCache.usedCacheSize;
            maximum = DiskCache.maximumCacheSize;
            directory = DiskCache.cacheDirectory;
        }

        Component.onCompleted: refresh()
    }
}
