//
//  TouchscreenDevice.cpp
//  input-plugins/src/input-plugins
//
//  Created by Triplelexx on 01/31/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "TouchscreenDevice.h"

#include <cmath>
#include "KeyboardMouseDevice.h"

#include <QtGui/QTouchEvent>
#include <QGestureEvent>
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>

#include <controllers/UserInputMapper.h>
#include <PathUtils.h>
#include <NumericalConstants.h>

const char* TouchscreenDevice::NAME = "Touchscreen";

bool TouchscreenDevice::isSupported() const {
    for (auto touchDevice : QTouchDevice::devices()) {
        if (touchDevice->type() == QTouchDevice::TouchScreen) {
            return true;
        }
    }
    return false;
}

void TouchscreenDevice::pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
    userInputMapper->withLock([&, this]() {
        _inputDevice->update(deltaTime, inputCalibrationData);
    });

    float distanceScaleX, distanceScaleY;
    if (_touchPointCount == 1) {
        if (_firstTouchVec.x < _currentTouchVec.x) {
            distanceScaleX = (_currentTouchVec.x - _firstTouchVec.x) / _screenDPIScale.x;
            _inputDevice->_axisStateMap[_inputDevice->makeInput(TOUCH_AXIS_X_POS).getChannel()].value = distanceScaleX;
        } else if (_firstTouchVec.x > _currentTouchVec.x) {
            distanceScaleX = (_firstTouchVec.x - _currentTouchVec.x) / _screenDPIScale.x;
            _inputDevice->_axisStateMap[_inputDevice->makeInput(TOUCH_AXIS_X_NEG).getChannel()].value = distanceScaleX;
        }
        // Y axis is inverted, positive is pointing up the screen
        if (_firstTouchVec.y > _currentTouchVec.y) {
            distanceScaleY = (_firstTouchVec.y - _currentTouchVec.y) / _screenDPIScale.y;
            _inputDevice->_axisStateMap[_inputDevice->makeInput(TOUCH_AXIS_Y_POS).getChannel()].value = distanceScaleY;
        } else if (_firstTouchVec.y < _currentTouchVec.y) {
            distanceScaleY = (_currentTouchVec.y - _firstTouchVec.y) / _screenDPIScale.y;
            _inputDevice->_axisStateMap[_inputDevice->makeInput(TOUCH_AXIS_Y_NEG).getChannel()].value = distanceScaleY;
        }
    } else  if (_touchPointCount == 2) {
        // zoom by how far the pinch has moved since the last update, not a fixed step every update (which zoomed
        // in while two fingers were held still, and too fast to stop at the right distance)
        qreal pinchDelta = _pinchScale - _lastPinchScale;
        if (_pinchScale != 0 && _lastPinchScale != 0 && pinchDelta != 0) {
            auto channel = pinchDelta > 0 ? TOUCH_GESTURE_PINCH_POS : TOUCH_GESTURE_PINCH_NEG;
            _inputDevice->_axisStateMap[_inputDevice->makeInput(channel).getChannel()].value = (float)std::abs(pinchDelta);
        }
        _lastPinchScale = _pinchScale;
    }
}

void TouchscreenDevice::InputDevice::update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    _axisStateMap.clear();
}

void TouchscreenDevice::InputDevice::focusOutEvent() {
}

void TouchscreenDevice::touchBeginEvent(const QTouchEvent* event) {
    const QTouchEvent::TouchPoint& point = event->touchPoints().at(0);
    _firstTouchVec = glm::vec2(point.pos().x(), point.pos().y());
    KeyboardMouseDevice::enableTouch(false);
    QScreen* eventScreen = event->window()->screen();
    if (_screenDPI != eventScreen->physicalDotsPerInch()) {
        _screenDPIScale.x = (float)eventScreen->physicalDotsPerInchX();
        _screenDPIScale.y = (float)eventScreen->physicalDotsPerInchY();
        _screenDPI = eventScreen->physicalDotsPerInch();
    }
}

void TouchscreenDevice::touchEndEvent(const QTouchEvent* event) {
    _touchPointCount = 0;
    KeyboardMouseDevice::enableTouch(true);
}

void TouchscreenDevice::touchUpdateEvent(const QTouchEvent* event) {
    const QTouchEvent::TouchPoint& point = event->touchPoints().at(0);
    _currentTouchVec = glm::vec2(point.pos().x(), point.pos().y());
    _touchPointCount = event->touchPoints().count();
}

void TouchscreenDevice::touchGestureEvent(const QGestureEvent* event) {
    if (QGesture* gesture = event->gesture(Qt::PinchGesture)) {
        QPinchGesture* pinch = static_cast<QPinchGesture*>(gesture);
        _pinchScale = pinch->totalScaleFactor();
        if (pinch->state() == Qt::GestureStarted) {
            // a new pinch starts from where it is, not from where the last one ended
            _lastPinchScale = _pinchScale;
        }
    }
}

controller::Input TouchscreenDevice::InputDevice::makeInput(TouchscreenDevice::TouchAxisChannel axis) const {
    return controller::Input(_deviceID, axis, controller::ChannelType::AXIS);
}

controller::Input TouchscreenDevice::InputDevice::makeInput(TouchscreenDevice::TouchGestureAxisChannel gesture) const {
    return controller::Input(_deviceID, gesture, controller::ChannelType::AXIS);
}

controller::Input::NamedVector TouchscreenDevice::InputDevice::getAvailableInputs() const {
    using namespace controller;
    static QVector<Input::NamedPair> availableInputs;
    static std::once_flag once;
    std::call_once(once, [&] {
        availableInputs.append(Input::NamedPair(makeInput(TOUCH_AXIS_X_POS), "DragRight"));
        availableInputs.append(Input::NamedPair(makeInput(TOUCH_AXIS_X_NEG), "DragLeft"));
        availableInputs.append(Input::NamedPair(makeInput(TOUCH_AXIS_Y_POS), "DragUp"));
        availableInputs.append(Input::NamedPair(makeInput(TOUCH_AXIS_Y_NEG), "DragDown"));

        availableInputs.append(Input::NamedPair(makeInput(TOUCH_GESTURE_PINCH_POS), "GesturePinchOut"));
        availableInputs.append(Input::NamedPair(makeInput(TOUCH_GESTURE_PINCH_NEG), "GesturePinchIn"));
    });
    return availableInputs;
}

QString TouchscreenDevice::InputDevice::getDefaultMappingConfig() const {
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/touchscreen.json";
    return MAPPING_JSON;
}
