//
//  EditorCamera.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorCamera.h"

#include <algorithm>
#include <cmath>

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include <gl/GLWidget.h>
#include <GLMHelpers.h>
#include <NumericalConstants.h>

#include "Application.h"

namespace {
    const float LOOK_RADIANS_PER_PIXEL = glm::radians(0.2f);
    // a slow hand looks finely: a look step under this many points turns the camera less per point, down to a quarter
    // at a quarter of it, so the view's centre can be put on something small far away; a sweep turns as before
    const float LOOK_PRECISION_POINTS = 4.0f;
    const float LOOK_MIN_GAIN = 0.25f;
    // holding Shift while looking (and not flying, where Shift flies faster) turns the camera this much slower still
    const float LOOK_PRECISION_FACTOR = 0.25f;
    const float ORBIT_RADIANS_PER_PIXEL = glm::radians(0.3f);
    const float DOLLY_PER_PIXEL = 0.01f;
    const float WHEEL_DOLLY_PER_NOTCH = 0.12f;
    const float WHEEL_SPEED_PER_NOTCH = 0.25f;
    const float FAST_FACTOR = 4.0f;
    const float MIN_DISTANCE = 0.05f;
    const float MAX_DISTANCE = 10000.0f;
    const float MIN_FLY_SPEED = 0.1f;
    const float MAX_FLY_SPEED = 500.0f;
    const float MAX_PITCH = glm::radians(89.5f);
    const float FRAME_PADDING = 1.15f;
    const int MAX_DELTA_PIXELS = 400;
    const int CLICK_TRAVEL_PIXELS = 4;     // a right press that moved less than this was a click
    const int CLICK_MILLISECONDS = 600;   // and was not held for long

    // one wheel notch, or about one finger-width on a trackpad
    float wheelNotches(QWheelEvent* event) {
        if (!event->pixelDelta().isNull()) {
            return event->pixelDelta().y() / 50.0f;
        }
        return event->angleDelta().y() / 120.0f;
    }

    bool isFlyKey(int key) {
        switch (key) {
            case Qt::Key_W:
            case Qt::Key_A:
            case Qt::Key_S:
            case Qt::Key_D:
            case Qt::Key_Q:
            case Qt::Key_E:
                return true;
            default:
                return false;
        }
    }
}

glm::quat EditorCamera::orientation() const {
    return glm::angleAxis(_yaw, Vectors::UP) * glm::angleAxis(_pitch, Vectors::RIGHT);
}

glm::vec3 EditorCamera::position() const {
    return _pivot - orientation() * Vectors::FRONT * _distance;
}

void EditorCamera::enter() {
    auto& camera = qApp->getCamera();
    _previousMode = camera.getMode();

    glm::vec3 forward = camera.getOrientation() * Vectors::FRONT;
    _pitch = glm::clamp(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)), -MAX_PITCH, MAX_PITCH);
    _yaw = std::atan2(-forward.x, -forward.z);
    _pivot = camera.getPosition() + forward * _distance;

    camera.setMode(CAMERA_MODE_INDEPENDENT);
    apply();
}

void EditorCamera::leave() {
    endDrag();
    qApp->getCamera().setMode(_previousMode);
}

void EditorCamera::apply() {
    auto& camera = qApp->getCamera();
    camera.setOrientation(orientation());
    camera.setPosition(position());
}

void EditorCamera::setYawPitch(float yaw, float pitch) {
    _yaw = yaw;
    _pitch = glm::clamp(pitch, -MAX_PITCH, MAX_PITCH);
}

void EditorCamera::update(float deltaTime) {
    if (_drag == Drag::Look && !_flyKeys.empty()) {
        glm::vec3 move(0.0f);
        if (_flyKeys.count(Qt::Key_W)) { move += Vectors::FRONT; }
        if (_flyKeys.count(Qt::Key_S)) { move -= Vectors::FRONT; }
        if (_flyKeys.count(Qt::Key_D)) { move += Vectors::RIGHT; }
        if (_flyKeys.count(Qt::Key_A)) { move -= Vectors::RIGHT; }
        if (_flyKeys.count(Qt::Key_E)) { move += Vectors::UP; }
        if (_flyKeys.count(Qt::Key_Q)) { move -= Vectors::UP; }
        if (glm::length(move) > 0.0f) {
            bool fast = QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
            float speed = _flySpeed * (fast ? FAST_FACTOR : 1.0f);
            _pivot += orientation() * glm::normalize(move) * speed * deltaTime;
        }
    }
    apply();
}

bool EditorCamera::mousePress(QMouseEvent* event) {
    if (_drag != Drag::None) {
        return true;
    }
    bool alt = event->modifiers().testFlag(Qt::AltModifier);
    bool command = event->modifiers().testFlag(Qt::ControlModifier);   // Cmd on macOS
    switch (event->button()) {
        case Qt::RightButton:
            beginDrag(alt ? Drag::Dolly : Drag::Look, event);
            return true;
        case Qt::MiddleButton:
            beginDrag(Drag::Pan, event);
            return true;
        case Qt::LeftButton:
            if (alt) {
                beginDrag(command ? Drag::Pan : Drag::Orbit, event);
                return true;
            }
            return false;   // a plain click is selection, which the editor handles
        default:
            return false;
    }
}

bool EditorCamera::mouseMove(QMouseEvent* event) {
    if (_drag == Drag::None) {
        return false;
    }
    QPointF cursor = event->screenPos();   // not globalPos(), which rounds to whole points
    QPointF delta = cursor - _lastCursor;
    _lastCursor = cursor;
    // a jump this big is the cursor being moved by something other than the hand; skip it
    if (std::abs(delta.x()) > MAX_DELTA_PIXELS || std::abs(delta.y()) > MAX_DELTA_PIXELS) {
        return true;
    }
    float dx = (float)delta.x();
    float dy = (float)delta.y();
    _dragTravel += std::abs(dx) + std::abs(dy);

    switch (_drag) {
        case Drag::Look: {
            // turn in place: keep the eye where it is and move the pivot
            glm::vec3 eye = position();
            float step = std::sqrt(dx * dx + dy * dy);
            float gain = glm::clamp(step / LOOK_PRECISION_POINTS, LOOK_MIN_GAIN, 1.0f);
            if (event->modifiers().testFlag(Qt::ShiftModifier) && _flyKeys.empty()) {
                gain *= LOOK_PRECISION_FACTOR;
            }
            float perPoint = LOOK_RADIANS_PER_PIXEL * gain;
            setYawPitch(_yaw - dx * perPoint, _pitch - dy * perPoint);
            _pivot = eye + orientation() * Vectors::FRONT * _distance;
            break;
        }
        case Drag::Orbit:
            setYawPitch(_yaw - dx * ORBIT_RADIANS_PER_PIXEL, _pitch - dy * ORBIT_RADIANS_PER_PIXEL);
            break;
        case Drag::Pan: {
            // the point under the cursor stays under the cursor
            float perPixel = worldPerPixel();
            glm::quat q = orientation();
            _pivot += q * Vectors::RIGHT * (-dx * perPixel) + q * Vectors::UP * (dy * perPixel);
            break;
        }
        case Drag::Dolly:
            _distance = glm::clamp(_distance * std::exp(-dx * DOLLY_PER_PIXEL), MIN_DISTANCE, MAX_DISTANCE);
            break;
        default:
            break;
    }
    apply();
    return true;
}

bool EditorCamera::mouseRelease(QMouseEvent* event) {
    if (_drag == Drag::None) {
        return false;
    }
    if (event->button() == _dragButton) {
        // a right click is a short press that hardly moved and did not fly: it opens the context
        // menu, and the few pixels of looking it did are undone
        _rightClick = _drag == Drag::Look && _dragTravel <= CLICK_TRAVEL_PIXELS && !_flew
            && _dragTimer.elapsed() < CLICK_MILLISECONDS;
        if (_rightClick) {
            _yaw = _dragStartYaw;
            _pitch = _dragStartPitch;
            _pivot = _dragStartPivot;
            apply();
        }
        endDrag();
    }
    return true;
}

bool EditorCamera::wheel(QWheelEvent* event) {
    float notches = wheelNotches(event);
    if (notches == 0.0f) {
        return true;
    }
    if (_drag == Drag::Look) {
        _flySpeed = glm::clamp(_flySpeed * std::exp(notches * WHEEL_SPEED_PER_NOTCH), MIN_FLY_SPEED, MAX_FLY_SPEED);
    } else {
        _distance = glm::clamp(_distance * std::exp(-notches * WHEEL_DOLLY_PER_NOTCH), MIN_DISTANCE, MAX_DISTANCE);
        apply();
    }
    return true;
}

bool EditorCamera::keyPress(QKeyEvent* event) {
    if (_drag == Drag::Look && isFlyKey(event->key())) {
        _flyKeys.insert(event->key());
        _flew = true;
        return true;
    }
    return false;
}

bool EditorCamera::keyRelease(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        return _flyKeys.count(event->key()) > 0;
    }
    return _flyKeys.erase(event->key()) > 0;
}

void EditorCamera::beginDrag(Drag drag, QMouseEvent* event) {
    _drag = drag;
    _dragButton = event->button();
    _dragOrigin = event->globalPos();
    _lastCursor = event->screenPos();
    _dragTravel = 0.0f;
    _flew = false;
    _rightClick = false;
    _dragTimer.start();
    _dragStartYaw = _yaw;
    _dragStartPitch = _pitch;
    _dragStartPivot = _pivot;
    if (drag == Drag::Look && !_cursorHidden) {
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);
        _cursorHidden = true;
    }
}

void EditorCamera::endDrag() {
    if (_cursorHidden) {
        // the hidden cursor travelled with the hand; put it back where the look started
        QCursor::setPos(_dragOrigin);
        QGuiApplication::restoreOverrideCursor();
        _cursorHidden = false;
    }
    _drag = Drag::None;
    _dragButton = Qt::NoButton;
    _flyKeys.clear();
}

float EditorCamera::worldPerPixel() const {
    const glm::mat4& projection = qApp->getCamera().getProjection();
    float tanHalfFov = 1.0f / projection[1][1];
    int heightPixels = std::max(1, qApp->getPrimaryWidget()->height());
    return 2.0f * _distance * tanHalfFov / (float)heightPixels;
}

void EditorCamera::frame(const AABox& bounds) {
    _pivot = bounds.calcCenter();
    float radius = std::max(0.5f, glm::length(bounds.getScale()) * 0.5f);
    float tanHalfFov = 1.0f / qApp->getCamera().getProjection()[1][1];
    _distance = glm::clamp(radius / tanHalfFov * FRAME_PADDING, MIN_DISTANCE, MAX_DISTANCE);
    apply();
}

void EditorCamera::viewFromAxis(int axis, int sign) {
    // stand on the +axis side (or -axis) and look back at the pivot
    glm::vec3 direction(0.0f);
    direction[glm::clamp(axis, 0, 2)] = sign >= 0 ? -1.0f : 1.0f;
    lookAlong(direction);
}

void EditorCamera::lookAlong(const glm::vec3& direction) {
    glm::vec3 d = glm::normalize(direction);
    float pitch = std::asin(glm::clamp(d.y, -1.0f, 1.0f));
    float yaw = (std::abs(d.y) > 0.9999f) ? _yaw : std::atan2(-d.x, -d.z);
    setYawPitch(yaw, pitch);
    apply();
}
