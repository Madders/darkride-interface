//
//  FaceTrackingPlugin.cpp
//  plugins/face-tracking-plugin/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "FaceTrackingPlugin.h"

#include <cmath>
#include <cstring>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <glm/gtc/quaternion.hpp>

#include <controllers/StandardControls.h>
#include <controllers/UserInputMapper.h>
#include <plugins/DisplayPlugin.h>
#include <ui-plugins/PluginContainer.h>
#include <NumericalConstants.h>
#include <PathUtils.h>
#include <SettingHandle.h>
#include <SharedUtil.h>

const char* FaceTrackingPlugin::NAME = "Face Tracking";
const char* FaceTrackingPlugin::ID = "FaceTracking";

namespace {

// the tracker's blendshapes (ARKit's names) as the engine's (High Fidelity's Faceshift names: ARKit's set came from
// Faceshift's); mouth: the jaw and lips, which the mouth strength scales
struct Shape {
    const char* arkit;
    controller::StandardAxisChannel channel;
    const char* engine;
    bool mouth;
};

const Shape SHAPES[] = {
    { "eyeBlinkLeft", controller::EYEBLINK_L, "EyeBlink_L", false },
    { "eyeBlinkRight", controller::EYEBLINK_R, "EyeBlink_R", false },
    { "eyeSquintLeft", controller::EYESQUINT_L, "EyeSquint_L", false },
    { "eyeSquintRight", controller::EYESQUINT_R, "EyeSquint_R", false },
    { "eyeLookDownLeft", controller::EYEDOWN_L, "EyeDown_L", false },
    { "eyeLookDownRight", controller::EYEDOWN_R, "EyeDown_R", false },
    { "eyeLookInLeft", controller::EYEIN_L, "EyeIn_L", false },
    { "eyeLookInRight", controller::EYEIN_R, "EyeIn_R", false },
    { "eyeWideLeft", controller::EYEOPEN_L, "EyeOpen_L", false },
    { "eyeWideRight", controller::EYEOPEN_R, "EyeOpen_R", false },
    { "eyeLookOutLeft", controller::EYEOUT_L, "EyeOut_L", false },
    { "eyeLookOutRight", controller::EYEOUT_R, "EyeOut_R", false },
    { "eyeLookUpLeft", controller::EYEUP_L, "EyeUp_L", false },
    { "eyeLookUpRight", controller::EYEUP_R, "EyeUp_R", false },
    { "browDownLeft", controller::BROWSD_L, "BrowsD_L", false },
    { "browDownRight", controller::BROWSD_R, "BrowsD_R", false },
    { "browInnerUp", controller::BROWSU_C, "BrowsU_C", false },
    { "browOuterUpLeft", controller::BROWSU_L, "BrowsU_L", false },
    { "browOuterUpRight", controller::BROWSU_R, "BrowsU_R", false },
    { "jawForward", controller::JAWFWD, "JawFwd", true },
    { "jawLeft", controller::JAWLEFT, "JawLeft", true },
    { "jawOpen", controller::JAWOPEN, "JawOpen", true },
    { "jawRight", controller::JAWRIGHT, "JawRight", true },
    { "mouthLeft", controller::MOUTHLEFT, "MouthLeft", false },
    { "mouthRight", controller::MOUTHRIGHT, "MouthRight", false },
    { "mouthFrownLeft", controller::MOUTHFROWN_L, "MouthFrown_L", false },
    { "mouthFrownRight", controller::MOUTHFROWN_R, "MouthFrown_R", false },
    { "mouthSmileLeft", controller::MOUTHSMILE_L, "MouthSmile_L", false },
    { "mouthSmileRight", controller::MOUTHSMILE_R, "MouthSmile_R", false },
    { "mouthDimpleLeft", controller::MOUTHDIMPLE_L, "MouthDimple_L", false },
    { "mouthDimpleRight", controller::MOUTHDIMPLE_R, "MouthDimple_R", false },
    { "mouthStretchLeft", controller::LIPSSTRETCH_L, "LipsStretch_L", true },
    { "mouthStretchRight", controller::LIPSSTRETCH_R, "LipsStretch_R", true },
    { "mouthRollUpper", controller::LIPSUPPERCLOSE, "LipsUpperClose", true },
    { "mouthRollLower", controller::LIPSLOWERCLOSE, "LipsLowerClose", true },
    { "mouthFunnel", controller::LIPSFUNNEL, "LipsFunnel", true },
    { "mouthPucker", controller::LIPSPUCKER, "LipsPucker", true },
    { "cheekPuff", controller::PUFF, "Puff", false },
    { "cheekSquintLeft", controller::CHEEKSQUINT_L, "CheekSquint_L", false },
    { "cheekSquintRight", controller::CHEEKSQUINT_R, "CheekSquint_R", false },
    { "mouthClose", controller::MOUTHCLOSE, "MouthClose", false },
    { "mouthUpperUpLeft", controller::MOUTHUPPERUP_L, "MouthUpperUp_L", false },
    { "mouthUpperUpRight", controller::MOUTHUPPERUP_R, "MouthUpperUp_R", false },
    { "mouthLowerDownLeft", controller::MOUTHLOWERDOWN_L, "MouthLowerDown_L", false },
    { "mouthLowerDownRight", controller::MOUTHLOWERDOWN_R, "MouthLowerDown_R", false },
    { "mouthPressLeft", controller::MOUTHPRESS_L, "MouthPress_L", false },
    { "mouthPressRight", controller::MOUTHPRESS_R, "MouthPress_R", false },
    { "mouthShrugLower", controller::MOUTHSHRUGLOWER, "MouthShrugLower", false },
    { "mouthShrugUpper", controller::MOUTHSHRUGUPPER, "MouthShrugUpper", false },
    { "noseSneerLeft", controller::NOSESNEER_L, "NoseSneer_L", false },
    { "noseSneerRight", controller::NOSESNEER_R, "NoseSneer_R", false },
};
const int NUM_SHAPES = sizeof(SHAPES) / sizeof(SHAPES[0]);  // MediaPipe has no tongue: 51 of the engine's 52

// each shape's place in the model's output, found by name
const std::array<int, NUM_SHAPES>& modelIndices() {
    static const std::array<int, NUM_SHAPES> indices = [] {
        std::array<int, NUM_SHAPES> found {};
        for (int s = 0; s < NUM_SHAPES; s++) {
            for (int m = 0; m < FaceLandmarker::NUM_BLENDSHAPES; m++) {
                if (strcmp(FaceLandmarker::BLENDSHAPE_NAMES[m], SHAPES[s].arkit) == 0) {
                    found[s] = m;
                }
            }
        }
        return found;
    }();
    return indices;
}

const int HEAD_TURN_CHANNEL = 0;  // the device's only pose

// the face eases toward each frame's weights (the camera's 30 a second, drawn at the display's rate): about MediaPipe's
// prototype's half-way-each-frame at 30 a second
const float EASE_SECONDS = 0.05f;
const float FACE_LOST_SECONDS = 0.3f;  // the procedural face comes back after this long without one
const int STRAIGHT_AHEAD_FRAMES = 30;
const glm::vec3 HEAD_LIMITS(40.0f, 60.0f, 40.0f);  // degrees of pitch, yaw and roll the head may turn
const float MIN_MOUTH_STRENGTH = 0.5f;
const float MAX_MOUTH_STRENGTH = 3.0f;

const char* SETTING_ENABLED = "enabled";
const char* SETTING_CAMERA = "camera";
const char* SETTING_MOUTH_STRENGTH = "mouthStrength";

// pitch, yaw and roll (degrees) of a rotation taken as pitch · yaw · roll about x, y and z, as MediaPipe's face
// transform is read
glm::vec3 pitchYawRoll(const glm::quat& rotation) {
    const glm::mat3 r = glm::mat3_cast(rotation);  // r[column][row]
    const float pitch = std::atan2(-r[2][1], r[2][2]);
    const float yaw = std::asin(glm::clamp(r[2][0], -1.0f, 1.0f));
    const float roll = std::atan2(-r[1][0], r[0][0]);
    return glm::degrees(glm::vec3(pitch, yaw, roll));
}

QString statusName(FaceTracker::Status status) {
    switch (status) {
        case FaceTracker::Status::Off: return "off";
        case FaceTracker::Status::Starting: return "starting";
        case FaceTracker::Status::AskingPermission: return "askingPermission";
        case FaceTracker::Status::Blocked: return "blocked";
        case FaceTracker::Status::NoCamera: return "noCamera";
        case FaceTracker::Status::CameraFailed: return "cameraFailed";
        case FaceTracker::Status::ModelFailed: return "modelFailed";
        case FaceTracker::Status::LookingForFace: return "lookingForFace";
        case FaceTracker::Status::Tracking: return "tracking";
    }
    return "off";
}

QString statusText(FaceTracker::Status status) {
    switch (status) {
        case FaceTracker::Status::Off: return "Off.";
        case FaceTracker::Status::Starting: return "Starting the camera.";
        case FaceTracker::Status::AskingPermission: return "Waiting for you to let Darkride use the camera.";
        case FaceTracker::Status::Blocked: return "Darkride isn't allowed to use the camera.";
        case FaceTracker::Status::NoCamera: return "No camera found.";
        case FaceTracker::Status::CameraFailed: return "The camera stopped working.";
        case FaceTracker::Status::ModelFailed: return "Face tracking couldn't start.";
        case FaceTracker::Status::LookingForFace: return "Looking for your face.";
        case FaceTracker::Status::Tracking: return "Tracking your face.";
    }
    return QString();
}

}  // namespace

controller::Input::NamedVector FaceTrackingPlugin::InputDevice::getAvailableInputs() const {
    controller::Input::NamedVector inputs;
    for (const Shape& shape : SHAPES) {
        inputs.push_back(makePair(shape.channel, shape.engine));
    }
    inputs.push_back(controller::Input::NamedPair(
        controller::Input(_deviceID, HEAD_TURN_CHANNEL, controller::ChannelType::POSE), "HeadTurn"));
    return inputs;
}

QString FaceTrackingPlugin::InputDevice::getDefaultMappingConfig() const {
    static const QString MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/face-tracking.json";
    return MAPPING_JSON;
}

FaceTrackingPlugin::~FaceTrackingPlugin() {
    stopTracking(true);
}

bool FaceTrackingPlugin::activate() {
    InputPlugin::activate();
    loadSettings();
    DependencyManager::get<controller::UserInputMapper>()->registerDevice(_inputDevice);
    // a test's picture in place of the camera (FaceTracker.h): on from the start, whatever the setting
    _stillTest = !qEnvironmentVariable("DARKRIDE_FACE_TRACKING_STILL").isEmpty();
    // pluginUpdate starts the tracker
    return true;
}

void FaceTrackingPlugin::deactivate() {
    stopTracking(true);
    releaseFace();
    DependencyManager::get<controller::UserInputMapper>()->removeDevice(_inputDevice->getDeviceID());
    InputPlugin::deactivate();
}

void FaceTrackingPlugin::startTracking() {
    if (!_thread) {
        _thread = new QThread();
        _thread->setObjectName("Face tracking");
        quint64 generation;
        {
            std::lock_guard<std::mutex> lock(_outputMutex);
            generation = ++_generation;
        }
        _tracker = new FaceTracker([this, generation](const FaceTracker::Output& output) { onOutput(generation, output); });
        _tracker->moveToThread(_thread);
        // below the frame's work: a late face is better than a late frame
        _thread->start(QThread::LowPriority);
    }
    _straightAheadFrames = 0;
    QMetaObject::invokeMethod(_tracker, "start", Qt::QueuedConnection, Q_ARG(QString, _camera));
}

void FaceTrackingPlugin::stopTracking(bool wait) {
    if (!_thread) {
        return;
    }
    // the camera closes and the models are freed on the tracker's own thread; then the thread ends
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        ++_generation;
        _output = FaceTracker::Output();
        _outputFresh = true;
    }
    FaceTracker* tracker = _tracker;
    QThread* thread = _thread;
    _tracker = nullptr;
    _thread = nullptr;
    QMetaObject::invokeMethod(tracker, [tracker, thread] {
        delete tracker;
        thread->quit();
    }, Qt::QueuedConnection);
    if (wait) {
        thread->wait();
        delete thread;
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
}

void FaceTrackingPlugin::onOutput(quint64 generation, const FaceTracker::Output& output) {
    // the tracker's thread
    std::lock_guard<std::mutex> lock(_outputMutex);
    if (generation != _generation) {
        return;
    }
    _output = output;
    _outputFresh = true;
}

void FaceTrackingPlugin::releaseFace() {
    // the procedural face (blinks, the talking mouth) and the animated head come back
    for (const Shape& shape : SHAPES) {
        _inputDevice->_axisStateMap[shape.channel] = controller::AxisValue(0.0f, 0, false);
    }
    _inputDevice->_poseStateMap[HEAD_TURN_CHANNEL] = controller::Pose();
    _faceShown = false;
}

void FaceTrackingPlugin::pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) {
    // on while it's switched on, except in VR: a headset hides the face, and the camera would run for nothing
    auto display = _container ? _container->getActiveDisplayPlugin() : DisplayPluginPointer();
    _inVR = display && display->isHmd();
    const bool wanted = (_enabled || _stillTest) && !_inVR;
    if (wanted && !_thread) {
        startTracking();
    } else if (!wanted && _thread) {
        stopTracking(false);
    }

    FaceTracker::Output output;
    bool fresh;
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        output = _output;
        fresh = _outputFresh;
        _outputFresh = false;
    }

    _sinceFace = std::min(_sinceFace + deltaTime, 2.0f * FACE_LOST_SECONDS);
    if (fresh && output.status == FaceTracker::Status::Tracking) {
        _sinceFace = 0.0f;
        for (int i = 0; i < FaceLandmarker::NUM_BLENDSHAPES; i++) {
            _targetWeights[i] = output.result.blendshapes[i];
        }
        // the head: the face's turn from the camera's view, from the first second's posture
        const glm::vec3 angles = pitchYawRoll(output.result.rotation);
        if (_straightAheadFrames < STRAIGHT_AHEAD_FRAMES) {
            _straightAhead = (_straightAhead * (float)_straightAheadFrames + angles) / (float)(_straightAheadFrames + 1);
            _straightAheadFrames++;
        }
        _targetHead = glm::clamp(angles - _straightAhead, -HEAD_LIMITS, HEAD_LIMITS);
        if (!_faceShown) {
            _weights = _targetWeights;
            _head = _targetHead;
            _faceShown = true;
        }
    }
    if (!wanted || !_faceShown || _sinceFace > FACE_LOST_SECONDS) {
        if (_faceShown) {
            releaseFace();
        }
        return;
    }

    const float ease = 1.0f - std::exp(-deltaTime / EASE_SECONDS);
    const quint64 now = usecTimestampNow();
    for (int i = 0; i < FaceLandmarker::NUM_BLENDSHAPES; i++) {
        _weights[i] += (_targetWeights[i] - _weights[i]) * ease;
    }
    for (int i = 0; i < NUM_SHAPES; i++) {
        const Shape& shape = SHAPES[i];
        float weight = _weights[modelIndices()[i]];
        if (shape.mouth) {
            weight = std::min(1.0f, weight * _mouthStrength);
        }
        _inputDevice->_axisStateMap[shape.channel] = controller::AxisValue(weight, now);
    }

    // in the avatar's frame, which faces the other way from the camera's: pitch and roll change sign, yaw doesn't
    _head += (_targetHead - _head) * ease;
    const glm::quat turn(glm::radians(glm::vec3(-_head.x, _head.y, -_head.z)));
    _inputDevice->_poseStateMap[HEAD_TURN_CHANNEL] = controller::Pose(glm::vec3(0.0f), turn);
}

void FaceTrackingPlugin::saveSettings() const {
    Settings settings;
    settings.beginGroup(getID());
    settings.setValue(SETTING_ENABLED, _enabled);
    settings.setValue(SETTING_CAMERA, _camera);
    settings.setValue(SETTING_MOUTH_STRENGTH, _mouthStrength);
    settings.endGroup();
}

void FaceTrackingPlugin::loadSettings() {
    Settings settings;
    settings.beginGroup(getID());
    _enabled = settings.value(SETTING_ENABLED, false).toBool();
    _camera = settings.value(SETTING_CAMERA, QString()).toString();
    _mouthStrength = glm::clamp(settings.value(SETTING_MOUTH_STRENGTH, 1.0f).toFloat(), MIN_MOUTH_STRENGTH,
                                MAX_MOUTH_STRENGTH);
    settings.endGroup();
}

QJsonObject FaceTrackingPlugin::configurationSettings() {
    FaceTracker::Output output;
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        output = _output;
    }
    const bool on = _enabled || _stillTest;
    FaceTracker::Status status = on ? output.status : FaceTracker::Status::Off;
    if (on && status == FaceTracker::Status::Off) {
        status = FaceTracker::Status::Starting;  // switched on; the tracker hasn't said anything yet
    }
    // listing the cameras opens none and asks nothing; the list is kept a few seconds, as a settings panel polls
    const int CAMERA_LIST_MS = 5000;
    if (!_camerasListed.isValid() || _camerasListed.elapsed() > CAMERA_LIST_MS) {
        _cameras = QJsonArray();
        for (const auto& camera : FaceTracker::cameras()) {
            _cameras.append(QJsonObject { { "id", camera.first }, { "name", camera.second } });
        }
        _camerasListed.start();
    }
    const bool paused = on && _inVR;
    return QJsonObject {
        { SETTING_ENABLED, on },
        { SETTING_CAMERA, _camera },
        { SETTING_MOUTH_STRENGTH, _mouthStrength },
        { "cameras", _cameras },
        { "status", paused ? QString("pausedInVR") : statusName(status) },
        { "statusText", paused ? QString("Paused in VR.") : statusText(status) },
        { "detail", status == FaceTracker::Status::Off || paused ? QString() : output.detail },
    };
}

void FaceTrackingPlugin::setConfigurationSettings(const QJsonObject configurationSettings) {
    const bool wasEnabled = _enabled;
    const QString wasCamera = _camera;
    if (configurationSettings.contains(SETTING_ENABLED)) {
        _enabled = configurationSettings[SETTING_ENABLED].toBool();
        _stillTest = false;  // the switch decides from now on, a test's picture or not
    }
    if (configurationSettings.contains(SETTING_CAMERA)) {
        _camera = configurationSettings[SETTING_CAMERA].toString();
    }
    if (configurationSettings.contains(SETTING_MOUTH_STRENGTH)) {
        _mouthStrength = glm::clamp((float)configurationSettings[SETTING_MOUTH_STRENGTH].toDouble(1.0),
                                    MIN_MOUTH_STRENGTH, MAX_MOUTH_STRENGTH);
    }
    saveSettings();

    if (!_enabled && _thread) {
        stopTracking(false);  // at once, so the camera's light goes out with the switch
    } else if (_thread && _camera != wasCamera) {
        startTracking();  // the new camera; pluginUpdate starts the tracker when it's switched on
    }
    if (_enabled != wasEnabled) {
        emit deviceStatusChanged(getName(), isRunning());
    }
}
