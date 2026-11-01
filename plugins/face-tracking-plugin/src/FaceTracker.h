//
//  FaceTracker.h
//  plugins/face-tracking-plugin/src
//
//  The camera and the face landmarker, on a thread of their own: started when face tracking is switched on, stopped
//  (camera off, models freed) when it's switched off. Each frame's result goes to a listener on that thread.
//
//  For tests on a computer without a camera: DARKRIDE_FACE_TRACKING_STILL=<a picture file> gives that picture as every
//  frame instead (FaceTrackingPlugin then also starts switched on).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#pragma once

#include <functional>
#include <memory>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QVideoFrame>

#include "FaceLandmarker.h"

class FrameSurface;

class FaceTracker : public QObject {
    Q_OBJECT
public:
    enum class Status {
        Off,
        Starting,
        AskingPermission,  // macOS is asking the user whether this app may use the camera
        Blocked,           // the user said no, or the system's settings don't allow it
        NoCamera,
        CameraFailed,
        ModelFailed,
        LookingForFace,
        Tracking,
    };

    struct Output {
        Status status { Status::Off };
        QString detail;  // what failed, for CameraFailed and ModelFailed
        FaceLandmarker::Result result;
        quint64 frame { 0 };  // counts the frames processed since the start
    };

    using Listener = std::function<void(const Output&)>;

    explicit FaceTracker(Listener listener);
    ~FaceTracker();

    // the cameras there are: each one's id (QCameraInfo::deviceName) and its name for people
    static QList<QPair<QString, QString>> cameras();

public slots:
    void start(const QString& cameraId);  // an empty id: the system's default camera
    void stop();

private:
    void report(Status status, const QString& detail = QString());
    void checkPermission();
    void openCamera();
    void closeCamera();
    void onCameraStatus(QCamera::Status status);
    void onFrame(const QVideoFrame& frame, bool bottomToTop);
    void processLatest();
    void processStill();
    void logFrame(const Output& output);

    Listener _listener;
    QString _cameraId;
    std::unique_ptr<FaceLandmarker> _landmarker;
    QCamera* _camera { nullptr };
    FrameSurface* _surface { nullptr };
    bool _cameraStarted { false };
    QVideoFrame _latest;
    bool _latestBottomToTop { false };
    bool _processQueued { false };
    QElapsedTimer _sinceProcessed;
    quint64 _frames { 0 };
    QTimer* _permissionCheck { nullptr };  // while macOS asks the user, whether they have answered
    QImage _still;  // DARKRIDE_FACE_TRACKING_STILL's picture, given as every frame in place of the camera's
    QTimer* _stillFrames { nullptr };
};
