//
//  AndroidGamepad.h
//  interface/src/android
//
//  A game controller paired with the phone (Bluetooth or USB), as the desktop's SDL2 gamepad: the same "GamePad"
//  device with the standard inputs and controllers/xbox.json, so it moves, turns and jumps the avatar as on the
//  desktop and scripts find it as Controller.Hardware.GamePad. The app has no SDL2: InterfaceActivity hands the
//  controller's sticks, triggers, hat and buttons over (Android's thread), and the device takes them up each update
//  (the main thread). While one is connected the on-screen joystick is hidden.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once

#include <QtCore/QtGlobal>

#if defined(Q_OS_ANDROID)

#include <atomic>

#include <controllers/InputDevice.h>

class AndroidGamepad : public controller::InputDevice {
public:
    AndroidGamepad();

    controller::Input::NamedVector getAvailableInputs() const override;
    QString getDefaultMappingConfig() const override;
    void update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) override;
    void focusOutEvent() override;

    // Android's thread: what the controller does now
    static void setConnected(bool connected);
    static void setAxes(float leftX, float leftY, float rightX, float rightY, float leftTrigger, float rightTrigger,
                        float hatX, float hatY);
    static void setButton(int keyCode, bool down);

private:
    mutable controller::Input::NamedVector _availableInputs;
};

#endif
