//
//  EditorCamera.h
//  interface/src/editor
//
//  The scene-view camera for build mode, with Unity's bindings: right-drag looks around
//  (W A S D and Q E fly, Shift is fast, the wheel sets the speed), Alt+left-drag orbits the
//  pivot, middle-drag or Alt+Cmd+left-drag pans, the wheel and Alt+right-drag dolly, and
//  F frames the selection. Runs on CAMERA_MODE_INDEPENDENT, which already parks the avatar.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorCamera_h
#define hifi_EditorCamera_h

#include <set>

#include <QtCore/QElapsedTimer>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <AABox.h>
#include <shared/Camera.h>

class EditorCamera {
public:
    void enter();   // take over from the current camera, where it is
    void leave();   // hand back to the avatar camera
    void update(float deltaTime);

    // each returns true when the event was consumed
    bool mousePress(QMouseEvent* event);
    bool mouseMove(QMouseEvent* event);
    bool mouseRelease(QMouseEvent* event);
    bool wheel(QWheelEvent* event);
    bool keyPress(QKeyEvent* event);
    bool keyRelease(QKeyEvent* event);

    void frame(const AABox& bounds);
    void viewFromAxis(int axis, int sign);   // axis 0..2, sign +1 or -1: look at that face
    void lookAlong(const glm::vec3& direction);

    glm::vec3 position() const;
    glm::quat orientation() const;
    const glm::vec3& pivot() const { return _pivot; }
    float distance() const { return _distance; }
    float flySpeed() const { return _flySpeed; }
    bool isLooking() const { return _drag == Drag::Look; }
    // the right button went down and up without looking around: a click, for the context menu
    bool wasRightClick() const { return _rightClick; }

private:
    enum class Drag { None, Look, Orbit, Pan, Dolly };

    void apply();
    void beginDrag(Drag drag, QMouseEvent* event);
    void endDrag();
    void setYawPitch(float yaw, float pitch);
    float worldPerPixel() const;

    glm::vec3 _pivot;
    float _yaw { 0.0f };
    float _pitch { 0.0f };
    float _distance { 5.0f };
    float _flySpeed { 4.0f };

    Drag _drag { Drag::None };
    Qt::MouseButton _dragButton { Qt::NoButton };
    QPoint _dragOrigin;    // global cursor position where the drag started
    QPointF _lastCursor;   // in points, with the fractions the platform reports (half points on a Retina screen)
    bool _cursorHidden { false };
    std::set<int> _flyKeys;
    // telling a right click from a look: how far the hand travelled, whether it flew, how long
    float _dragTravel { 0.0f };
    bool _flew { false };
    QElapsedTimer _dragTimer;
    float _dragStartYaw { 0.0f };
    float _dragStartPitch { 0.0f };
    glm::vec3 _dragStartPivot;
    bool _rightClick { false };

    CameraMode _previousMode { CAMERA_MODE_LOOK_AT };
};

#endif // hifi_EditorCamera_h
