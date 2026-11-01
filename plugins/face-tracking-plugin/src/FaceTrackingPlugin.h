//
//  FaceTrackingPlugin.h
//  plugins/face-tracking-plugin/src
//
//  Webcam face tracking: your camera moves your avatar's face (ARKit's 52 blendshapes, under the engine's names) and,
//  in desktop mode, turns its head. Off unless switched on; the camera and the models run only while it's on, on a
//  thread of their own. The picture never leaves the computer: only the face's weights and the head's turn go on, as
//  any face tracker's do.
//
//  Settings (InputConfiguration's configurationSettings and setConfigurationSettings, for "Face Tracking"):
//    enabled        whether it's on (saved)
//    camera         the camera's id, "" for the system's default (saved)
//    mouthStrength  how strongly the jaw and lips follow, 1 as tracked; small mouths want 2 to 3 (saved)
//    cameras        (read only) [{ id, name }] the cameras there are
//    status         (read only) off, starting, askingPermission, blocked, noCamera, cameraFailed, modelFailed,
//                   lookingForFace, tracking or pausedInVR (a headset hides the face: the camera is off meanwhile);
//                   statusText says it in words, detail what failed
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#pragma once

#include <array>
#include <memory>
#include <mutex>

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QThread>

#include <controllers/InputDevice.h>
#include <plugins/InputPlugin.h>

#include "FaceTracker.h"

class FaceTrackingPlugin : public InputPlugin {
    Q_OBJECT
public:
    static const char* NAME;
    static const char* ID;

    ~FaceTrackingPlugin();

    bool isSupported() const override { return true; }
    const QString getName() const override { return NAME; }
    const QString getID() const override { return ID; }
    bool isRunning() const override { return _active && _enabled; }

    bool activate() override;
    void deactivate() override;

    void pluginFocusOutEvent() override {}
    void pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) override;

    void saveSettings() const override;
    void loadSettings() override;

    QJsonObject configurationSettings() override;
    void setConfigurationSettings(const QJsonObject configurationSettings) override;

private:
    class InputDevice : public controller::InputDevice {
    public:
        InputDevice() : controller::InputDevice("FaceTracking") {}

        controller::Input::NamedVector getAvailableInputs() const override;
        QString getDefaultMappingConfig() const override;

    private:
        friend class FaceTrackingPlugin;
    };

    void startTracking();
    void stopTracking(bool wait);
    void onOutput(quint64 generation, const FaceTracker::Output& output);
    void releaseFace();

    std::shared_ptr<InputDevice> _inputDevice { std::make_shared<InputDevice>() };

    bool _enabled { false };
    QString _camera;
    float _mouthStrength { 1.0f };

    bool _stillTest { false };  // DARKRIDE_FACE_TRACKING_STILL
    bool _inVR { false };
    QJsonArray _cameras;
    QElapsedTimer _camerasListed;

    QThread* _thread { nullptr };
    FaceTracker* _tracker { nullptr };
    std::mutex _outputMutex;
    quint64 _generation { 0 };  // each start's number, under the mutex: a stopped tracker's late frames are dropped
    FaceTracker::Output _output;  // the tracker's latest
    bool _outputFresh { false };

    // the main thread's: the face as given to the avatar, eased toward the tracker's latest
    bool _faceShown { false };
    float _sinceFace { 0.0f };
    std::array<float, FaceLandmarker::NUM_BLENDSHAPES> _targetWeights {};
    std::array<float, FaceLandmarker::NUM_BLENDSHAPES> _weights {};
    glm::vec3 _targetHead;  // pitch, yaw and roll, degrees, from straight ahead
    glm::vec3 _head;
    glm::vec3 _straightAhead;  // the first second's average: that posture counts as straight ahead
    int _straightAheadFrames { 0 };
};
