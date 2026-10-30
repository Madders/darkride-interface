//
//  Display.h
//  interface/src
//
//  The display: the display plug-ins (the desktop and the headsets), found and given the GPU context at start-up,
//  the active one chosen, switched from the Output menu or by name and kept atomic for the other threads, the
//  headset's auto-switch (its stand-by session, and the worn status polled twice a second), the window's geometry
//  for the renderer and the UI (the canvas, UI and device sizes, the HUD rectangle, the resize that follows the
//  plug-in), the window's focus, raising, foreground and minimized states (the plug-in deactivated while minimized,
//  idle events kept coming), the headset's queries (the eye projections and offsets, the sensor pose, the
//  controllers present) and the window's title (the world's name, the client label, the build's branch). A QObject:
//  the window's and the application's state signals connect to it, and it tells a plug-in change with
//  activeDisplayPluginChanged, which Application relays through its own signal of that name, since services connect
//  to Application in the essentials stage. Application owns one, reached by getDisplay(), constructed before that
//  stage; its getActiveDisplayPlugin(), isHMDMode() and the other queries forward here. These were Application's
//  _displayPlugin and its lock, the auto-switch plug-in and the worn status, the render resolution, the foreground
//  flag, the minimized timer and the display members of ApplicationDisplay.cpp; the GL context, the render engine's
//  start and the present callback stay the start-up's and the frame loop's (ARCHITECTURE.md §5.4 step 4, the ninth
//  owner class).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_Display_h
#define hifi_Display_h

#include <memory>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <glm/glm.hpp>

#include <plugins/Forward.h>

class QAbstractNativeEventFilter;

class Display : public QObject {
    Q_OBJECT
public:
    Display();    // on Windows, starts listening for the front app's changes (WM_ACTIVATEAPP)
    ~Display();

    void initializePlugins();   // the plug-ins found and given the GPU context, the default and then the saved one activated, the headset's auto-switch started (was initializeDisplayPlugins, less the first frame)

    DisplayPluginPointer getActiveDisplayPlugin() const;   // under the lock from another thread; on the main thread, the mode made current when there is none yet
    const DisplayPluginPointer& plugin() const { return _displayPlugin; }   // as it is: no lock, nothing made current (the main thread's quick reads)
    bool isHMDMode() const;
    float getTargetRenderFrameRate() const;   // frames/second
    float getPresentPeriod() const;           // the period the presents are paced at, in seconds; 0 while they are not (DisplayPlugin::getPresentPeriod)
    float getRenderResolutionScale() const;
    bool isThrottleRendering() const;
    glm::uvec2 getCanvasSize() const;
    QRect getRenderingGeometry() const;
    glm::uvec2 getUiSize() const;
    QRect getRecommendedHUDRect() const;
    glm::vec2 getDeviceSize() const;
    QRect getDesirableApplicationGeometry() const;   // the window's, or the main screen's when the window is on the headset's
    qreal getDevicePixelRatio() const;
    glm::mat4 getHMDSensorPose() const;
    glm::mat4 getEyeOffset(int eye) const;
    glm::mat4 getEyeProjection(int eye) const;
    bool hasRiftControllers();
    bool hasViveControllers();
    bool hasFocus() const;
    void setFocus();
    void raise();
    void raiseWindow() const;
    bool isForeground() const;
    bool isAppInFront() const;   // the player's front app is this one: on Windows the foreground window's process, elsewhere Qt's application state

    void resizeGL();                  // the render size, the camera's projection and the UI's size follow the plug-in
    void updateWindowTitle() const;   // the world's name, the client label first, the build's branch on a dev build
    void setActiveDisplayPlugin(const QString& pluginName);
    void updateDisplayMode();         // the plug-in checked in the Output menu made the active one
    void setDisplayPlugin(DisplayPluginPointer newPlugin);
    void startHMDStandBySession();
    void endHMDSession();
    void deactivate();      // at quit: the plug-in deactivated, the headset's stand-by session ended
    void releasePlugin();   // at the end of the shutdown: the pointer dropped before the plug-in manager shuts down

public slots:
    void activeChanged(Qt::ApplicationState state);   // the foreground flag and the refresh-rate regime
    void windowMinimizedChanged(bool minimized);      // the plug-in deactivated while minimized, idle events kept coming
    void foregroundChanged(bool inFront);             // Windows' word that this app came to the front or went behind
    void checkForeground();                           // Windows, twice a second: the regime follows whose window is in front

signals:
    void activeDisplayPluginChanged();   // after a successful change; Application relays it

private slots:
    void switchDisplayMode();   // the headset's auto-switch: worn, the headset's plug-in; taken off, the desktop's

private:
    DisplayPluginPointer _displayPlugin;
    mutable std::mutex _displayPluginLock;
    DisplayPluginPointer _autoSwitchDisplayModeSupportedHMDPlugin{ nullptr };
    QString _autoSwitchDisplayModeSupportedHMDPluginName;
    bool _previousHMDWornStatus{ false };
    glm::uvec2 _renderResolution;
    bool _isForeground{ true };   // starts out assumed to be in foreground
    QTimer _minimizedWindowTimer;
    std::unique_ptr<QAbstractNativeEventFilter> _foregroundFilter;   // Windows only: WM_ACTIVATEAPP, which Qt's application state can miss
    QTimer _foregroundCheck;   // Windows only: checkForeground's

    // the window's screen, followed: moved to another screen, or the screen's rate changed, the pacing takes the new
    // rate (until the 10-21 review it was read only when the profile or the regime changed)
    void followScreenRefreshRate();
    QMetaObject::Connection _screenChangedConnection;
    QMetaObject::Connection _screenRateConnection;
};

#endif // hifi_Display_h
