//
//  AndroidGamepad.cpp
//  interface/src/android
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "AndroidGamepad.h"

#if defined(Q_OS_ANDROID)

#include <array>
#include <cmath>
#include <memory>

#include <jni.h>

#include <QtCore/QCoreApplication>

#include <controllers/StandardControls.h>
#include <controllers/UserInputMapper.h>
#include <DependencyManager.h>
#include <PathUtils.h>
#include <VirtualPadManager.h>

#include "InterfaceLogging.h"

namespace {

// below this a stick counts as centred, as the desktop gamepad's
const float CONTROLLER_THRESHOLD = 0.3f;

enum Axis { LEFT_X, LEFT_Y, RIGHT_X, RIGHT_Y, LEFT_TRIGGER, RIGHT_TRIGGER, HAT_X, HAT_Y, AXIS_COUNT };

// Android's state of the controller, written on its thread and read on the main one
std::array<std::atomic<float>, AXIS_COUNT> axes {};
// one bit per StandardButtonChannel, from the controller's keys
std::atomic<uint32_t> buttons { 0 };

// the device while a controller is connected (main thread only)
std::shared_ptr<AndroidGamepad> device;

// Android's KeyEvent codes for a controller's buttons, as the standard ones; -1 for none
int buttonForKeyCode(int keyCode) {
    switch (keyCode) {
        case 96: return controller::A;      // KEYCODE_BUTTON_A
        case 97: return controller::B;      // KEYCODE_BUTTON_B
        case 4: return controller::B;       // KEYCODE_BACK: what some controllers send for B
        case 99: return controller::X;      // KEYCODE_BUTTON_X
        case 100: return controller::Y;     // KEYCODE_BUTTON_Y
        case 102: return controller::LB;    // KEYCODE_BUTTON_L1
        case 103: return controller::RB;    // KEYCODE_BUTTON_R1
        case 106: return controller::LS;    // KEYCODE_BUTTON_THUMBL
        case 107: return controller::RS;    // KEYCODE_BUTTON_THUMBR
        case 108: return controller::START; // KEYCODE_BUTTON_START
        case 109: return controller::BACK;  // KEYCODE_BUTTON_SELECT
        case 110: return controller::GUIDE; // KEYCODE_BUTTON_MODE
        case 19: return controller::DU;     // KEYCODE_DPAD_UP: a D-pad that sends keys, not a hat
        case 20: return controller::DD;
        case 21: return controller::DL;
        case 22: return controller::DR;
        default: return -1;
    }
}

// triggers that only send keys (L2/R2 with no analogue axis)
int triggerAxisForKeyCode(int keyCode) {
    switch (keyCode) {
        case 104: return LEFT_TRIGGER;  // KEYCODE_BUTTON_L2
        case 105: return RIGHT_TRIGGER; // KEYCODE_BUTTON_R2
        default: return -1;
    }
}

float centred(float value) {
    return std::fabs(value) < CONTROLLER_THRESHOLD ? 0.0f : value;
}

} // namespace

AndroidGamepad::AndroidGamepad() : InputDevice("GamePad") {
}

controller::Input::NamedVector AndroidGamepad::getAvailableInputs() const {
    using namespace controller;
    if (_availableInputs.length() == 0) {
        // the desktop gamepad's names (plugins/sdl2-plugin/src/Joystick.cpp), so its mappings and scripts work here
        _availableInputs = {
            makePair(A, "A"), makePair(B, "B"), makePair(X, "X"), makePair(Y, "Y"),
            makePair(DU, "DU"), makePair(DD, "DD"), makePair(DL, "DL"), makePair(DR, "DR"),
            makePair(LB, "LB"), makePair(RB, "RB"), makePair(LS, "LS"), makePair(RS, "RS"),
            makePair(START, "Start"), makePair(BACK, "Back"),
            makePair(LX, "LX"), makePair(LY, "LY"), makePair(RX, "RX"), makePair(RY, "RY"),
            makePair(LT, "LT"), makePair(RT, "RT"),
            makePair(LB, "L1"), makePair(RB, "R1"), makePair(LT, "L2"), makePair(RT, "R2"),
            makePair(LS, "L3"), makePair(RS, "R3"), makePair(BACK, "Select"),
            makePair(A, "Cross"), makePair(B, "Circle"), makePair(X, "Square"), makePair(Y, "Triangle"),
            makePair(DU, "Up"), makePair(DD, "Down"), makePair(DL, "Left"), makePair(DR, "Right"),
        };
    }
    return _availableInputs;
}

QString AndroidGamepad::getDefaultMappingConfig() const {
    // the phone's own: the right stick looks around, A goes up and B down, the left trigger runs (xbox.json is for
    // the desktop and VR, where the triggers click the hands and the right stick only rises and sinks)
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/android_gamepad.json";
    return MAPPING_JSON;
}

void AndroidGamepad::update(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    using namespace controller;
    _axisStateMap[makeInput(LX).getChannel()].value = centred(axes[LEFT_X]);
    _axisStateMap[makeInput(LY).getChannel()].value = centred(axes[LEFT_Y]);
    _axisStateMap[makeInput(RX).getChannel()].value = centred(axes[RIGHT_X]);
    _axisStateMap[makeInput(RY).getChannel()].value = centred(axes[RIGHT_Y]);
    _axisStateMap[makeInput(LT).getChannel()].value = axes[LEFT_TRIGGER];
    _axisStateMap[makeInput(RT).getChannel()].value = axes[RIGHT_TRIGGER];

    uint32_t pressed = buttons;
    // a hat D-pad (most controllers) as the D-pad's buttons
    const float hatX = axes[HAT_X];
    const float hatY = axes[HAT_Y];
    if (hatX < -0.5f) {
        pressed |= 1u << DL;
    } else if (hatX > 0.5f) {
        pressed |= 1u << DR;
    }
    if (hatY < -0.5f) {
        pressed |= 1u << DU;
    } else if (hatY > 0.5f) {
        pressed |= 1u << DD;
    }
    _buttonPressedMap.clear();
    for (int button = A; button <= DR; button++) {
        if (pressed & (1u << button)) {
            _buttonPressedMap.insert(makeInput((StandardButtonChannel)button).getChannel());
        }
    }
}

void AndroidGamepad::focusOutEvent() {
    _axisStateMap.clear();
    _buttonPressedMap.clear();
}

void AndroidGamepad::updateConnected(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    if (device) {
        device->update(deltaTime, inputCalibrationData);
    }
}

void AndroidGamepad::setConnected(bool connected) {
    if (!connected) {
        for (auto& axis : axes) {
            axis = 0.0f;
        }
        buttons = 0;
    }
    QMetaObject::invokeMethod(qApp, [connected] {
        auto userInputMapper = DependencyManager::get<controller::UserInputMapper>();
        if (connected && !device) {
            device = std::make_shared<AndroidGamepad>();
            userInputMapper->registerDevice(device);
            qCDebug(interfaceapp) << "Game controller connected";
        } else if (!connected && device) {
            userInputMapper->removeDevice(device->getDeviceID());
            device.reset();
            qCDebug(interfaceapp) << "Game controller disconnected";
        }
        VirtualPad::Manager::instance().hideForController(connected);
    }, Qt::QueuedConnection);
}

void AndroidGamepad::setAxes(float leftX, float leftY, float rightX, float rightY, float leftTrigger,
                             float rightTrigger, float hatX, float hatY) {
    axes[LEFT_X] = leftX;
    axes[LEFT_Y] = leftY;
    axes[RIGHT_X] = rightX;
    axes[RIGHT_Y] = rightY;
    axes[LEFT_TRIGGER] = leftTrigger;
    axes[RIGHT_TRIGGER] = rightTrigger;
    axes[HAT_X] = hatX;
    axes[HAT_Y] = hatY;
}

void AndroidGamepad::setButton(int keyCode, bool down) {
    const int trigger = triggerAxisForKeyCode(keyCode);
    if (trigger >= 0) {
        axes[trigger] = down ? 1.0f : 0.0f;
        return;
    }
    const int button = buttonForKeyCode(keyCode);
    if (button < 0) {
        return;
    }
    if (down) {
        buttons |= 1u << button;
    } else {
        buttons &= ~(1u << button);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeGamepadConnected(JNIEnv*, jobject, jboolean connected) {
    AndroidGamepad::setConnected(connected);
}

extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeGamepadAxes(JNIEnv*, jobject, jfloat leftX, jfloat leftY,
                                                                       jfloat rightX, jfloat rightY,
                                                                       jfloat leftTrigger, jfloat rightTrigger,
                                                                       jfloat hatX, jfloat hatY) {
    AndroidGamepad::setAxes(leftX, leftY, rightX, rightY, leftTrigger, rightTrigger, hatX, hatY);
}

extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeGamepadButton(JNIEnv*, jobject, jint keyCode,
                                                                         jboolean down) {
    AndroidGamepad::setButton(keyCode, down);
}

#endif
