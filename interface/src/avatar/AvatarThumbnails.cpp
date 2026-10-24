//
//  AvatarThumbnails.cpp
//  interface/src/avatar
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "AvatarThumbnails.h"

#include <memory>

#include <QtCore/QBuffer>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <AccountManager.h>
#include <GLMHelpers.h>
#include <NetworkingConstants.h>
#include <ViewFrustum.h>

#include "Application.h"
#include "AvatarManager.h"
#include "MyAvatar.h"
#include "SecondaryCamera.h"
#include "editor/EditorCreate.h"

namespace {
    const float FIELD_OF_VIEW = 30.0f;       // degrees, vertical: a portrait lens
    const float FRAME = 0.6f;                // of the avatar's height in the picture: the top of the head to the waist
    const float NEAR_CLIP = 0.05f;
    const int LOAD_POLL_MS = 250;
    const int LOAD_LIMIT_MS = 20000;
    const int SETTLE_MS = 1500;              // after an avatar just put on has loaded: its textures
    const int CAMERA_SETTLE_MS = 300;        // the camera's new place and size reach the render
    const int JPEG_QUALITY = 88;

    void answer(QJSValue& callback, const QString& error, const QString& url) {
        if (callback.isCallable()) {
            callback.call({ error.isEmpty() ? QJSValue(QJSValue::NullValue) : QJSValue(error), QJSValue(url) });
        }
    }

    // the avatar worn has loaded the model it asks for
    bool avatarLoaded() {
        auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();
        auto skeleton = myAvatar ? myAvatar->getSkeletonModel() : nullptr;
        return skeleton && skeleton->getURL() == myAvatar->getSkeletonModelURL() && skeleton->isLoaded();
    }
}

QString AvatarThumbnails::pictureName(const QString& name) {
    return EditorUpload::safeFileName(name + ".preview.jpg");
}

QByteArray AvatarThumbnails::fit(const QImage& image) {
    if (image.isNull()) {
        return QByteArray();
    }
    const QImage scaled = image.scaled(WIDTH, HEIGHT, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QImage cropped = scaled.copy((scaled.width() - WIDTH) / 2, (scaled.height() - HEIGHT) / 2, WIDTH, HEIGHT);
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    cropped.convertToFormat(QImage::Format_RGB888).save(&buffer, "JPG", JPEG_QUALITY);
    return jpeg;
}

void AvatarThumbnails::capture(Done done) {
    if (avatarLoaded()) {
        shoot(done);
        return;
    }
    // an avatar just put on (the importer's Wear & Keep): its model, then a moment for its textures
    auto waited = std::make_shared<int>(0);
    auto timer = new QTimer(this);
    timer->setInterval(LOAD_POLL_MS);
    connect(timer, &QTimer::timeout, this, [this, timer, waited, done]() {
        *waited += LOAD_POLL_MS;
        const bool loaded = avatarLoaded();
        if (!loaded && *waited < LOAD_LIMIT_MS) {
            return;
        }
        timer->stop();
        timer->deleteLater();
        if (!loaded) {
            done(QByteArray(), "the avatar didn't finish loading");
            return;
        }
        QTimer::singleShot(SETTLE_MS, this, [this, done]() { shoot(done); });
    });
    timer->start();
}

void AvatarThumbnails::shoot(Done done) {
    auto renderConfig = qApp->getRenderEngine()->getConfiguration();
    auto camera = renderConfig ? dynamic_cast<SecondaryCameraJobConfig*>(renderConfig->getConfig("SecondaryCamera")) : nullptr;
    auto myAvatar = DependencyManager::get<AvatarManager>()->getMyAvatar();
    if (_taking || !camera || !myAvatar) {
        done(QByteArray(), _taking ? "a picture is being taken" : "there's no camera");
        return;
    }
    _taking = true;

    // what the camera was doing (the spectator camera may be on), put back after the picture
    struct Was {
        bool enabled;
        QUuid attached;
        QUuid portalEntrance;
        glm::vec3 position;
        glm::quat orientation;
        float vFoV;
        float nearClip;
        float farClip;
        int width;
        int height;
        bool mirror;
        bool portal;
    };
    auto was = std::make_shared<Was>(Was { camera->isEnabled(), camera->attachedEntityId, camera->portalEntranceEntityId,
                                           camera->position, camera->orientation, camera->vFoV,
                                           camera->nearClipPlaneDistance, camera->farClipPlaneDistance,
                                           camera->textureWidth, camera->textureHeight, camera->mirrorProjection,
                                           camera->portalProjection });

    // in front of you, facing you, a little above the chest, the head to the waist in the frame: the head joint sits
    // at the neck, the top of the head about an eighth of the height above it, so the frame's middle is a tenth below
    const float height = myAvatar->getHeight();
    glm::vec3 forward = myAvatar->getWorldOrientation() * Vectors::FRONT;
    forward.y = 0.0f;
    forward = glm::length(forward) > 0.01f ? glm::normalize(forward) : Vectors::FRONT;
    const glm::vec3 target = myAvatar->getHeadPosition() - Vectors::UNIT_Y * (0.1f * height);
    const float distance = (FRAME * height * 0.5f) / tanf(glm::radians(FIELD_OF_VIEW * 0.5f));
    const glm::vec3 position = target + forward * distance + Vectors::UNIT_Y * (0.05f * height);
    const glm::vec3 look = glm::normalize(target - position);
    // a camera looks down -z: turn it to the look's heading, then tilt it to the look's pitch
    const glm::quat orientation = glm::angleAxis(atan2f(-look.x, -look.z), Vectors::UNIT_Y) *
                                  glm::angleAxis(asinf(glm::clamp(look.y, -1.0f, 1.0f)), Vectors::UNIT_X);

    camera->attachedEntityId = QUuid();
    camera->portalEntranceEntityId = QUuid();
    camera->mirrorProjection = false;
    camera->portalProjection = false;
    camera->vFoV = FIELD_OF_VIEW;
    camera->nearClipPlaneDistance = NEAR_CLIP;
    camera->farClipPlaneDistance = DEFAULT_FAR_CLIP;
    camera->setPosition(position);
    camera->setOrientation(orientation);
    camera->resetSizeSpectatorCamera(WIDTH, HEIGHT);
    camera->enableSecondaryCameraRenderConfigs(true);

    QTimer::singleShot(CAMERA_SETTLE_MS, this, [this, camera, was, done]() {
        qApp->getSnapshotRequests().add(std::make_tuple([this, camera, was, done](const QImage& image) {
            // on the display's thread: the rest on the main one
            const QImage picture = image.copy();
            qApp->postLambdaEvent([this, camera, was, done, picture]() {
                camera->attachedEntityId = was->attached;
                camera->portalEntranceEntityId = was->portalEntrance;
                camera->mirrorProjection = was->mirror;
                camera->portalProjection = was->portal;
                camera->vFoV = was->vFoV;
                camera->nearClipPlaneDistance = was->nearClip;
                camera->farClipPlaneDistance = was->farClip;
                camera->setPosition(was->position);
                camera->setOrientation(was->orientation);
                camera->resetSizeSpectatorCamera(was->width, was->height);
                camera->enableSecondaryCameraRenderConfigs(was->enabled);
                _taking = false;
                const QByteArray jpeg = fit(picture);
                done(jpeg, jpeg.isEmpty() ? "the camera gave no picture" : QString());
            });
        }, 0.0f, false));
    });
}

QString AvatarThumbnails::keep(const QByteArray& jpeg, QString& error) const {
    QDir folder(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/avatar-thumbnails");
    if (!folder.mkpath(".")) {
        error = "couldn't make a folder for the picture";
        return QString();
    }
    const QString path = folder.filePath(QString::number(QDateTime::currentMSecsSinceEpoch()) + ".jpg");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(jpeg) != jpeg.size()) {
        error = "couldn't keep the picture";
        return QString();
    }
    return path;
}

void AvatarThumbnails::take(QJSValue callback) {
    capture([this, callback](const QByteArray& jpeg, const QString& error) mutable {
        if (!error.isEmpty()) {
            answer(callback, error, QString());
            return;
        }
        QString problem;
        const QString path = keep(jpeg, problem);
        answer(callback, problem, path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString());
    });
}

void AvatarThumbnails::fromFile(const QString& path, QJSValue callback) {
    const QUrl url(path);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : path;
    const QImage image(localPath);
    if (image.isNull()) {
        answer(callback, "that file isn't a picture this can read", QString());
        return;
    }
    QString problem;
    const QString kept = keep(fit(image), problem);
    answer(callback, problem, kept.isEmpty() ? QString() : QUrl::fromLocalFile(kept).toString());
}

QVariantMap AvatarThumbnails::myFilesPlace(const QString& fstUrl) const {
    auto accountManager = DependencyManager::get<AccountManager>();
    const QString me = accountManager->getAccountInfo().getUsername().toLower();
    if (me.isEmpty() || !accountManager->isLoggedIn()) {
        return QVariantMap();
    }
    // tea://<user>/<path>, or the files host's https://files.<metaverse host>/<user>/<path>
    const QUrl url(fstUrl);
    QString path;
    if (url.scheme() == "tea") {
        if (url.host().toLower() != me) {
            return QVariantMap();
        }
        path = url.path();
    } else if (url.scheme() == "https" || url.scheme() == "http") {
        const QString filesHost = "files." + NetworkingConstants::METAVERSE_SERVER_URL().host();
        if (url.host().toLower() != filesHost.toLower() || url.path().section('/', 1, 1).toLower() != me) {
            return QVariantMap();
        }
        path = "/" + url.path().section('/', 2);
    } else {
        return QVariantMap();
    }
    if (!path.endsWith(".fst", Qt::CaseInsensitive)) {
        return QVariantMap();
    }
    QVariantMap place;
    place["folder"] = path.section('/', 0, -2);
    place["name"] = path.section('/', -1).chopped(4);
    return place;
}

QString AvatarThumbnails::pictureKey(const QString& fstUrl) const {
    const QVariantMap place = myFilesPlace(fstUrl);
    return place.isEmpty() ? QString() : place["folder"].toString() + "/" + pictureName(place["name"].toString());
}

void AvatarThumbnails::upload(const QString& pictureUrl, const QString& fstUrl, QJSValue callback) {
    const QVariantMap place = myFilesPlace(fstUrl);
    if (place.isEmpty()) {
        answer(callback, "this avatar isn't in your My Files", QString());
        return;
    }
    const QUrl url(pictureUrl);
    QFile file(url.isLocalFile() ? url.toLocalFile() : pictureUrl);
    if (!file.open(QIODevice::ReadOnly)) {
        answer(callback, "couldn't read the picture", QString());
        return;
    }
    auto upload = new EditorUpload(this);
    QString error;
    if (!upload->startData(file.readAll(), pictureName(place["name"].toString()), place["folder"].toString(), "image/jpeg",
                           error)) {
        delete upload;
        answer(callback, error, QString());
        return;
    }
    connect(upload, &EditorUpload::failed, this, [upload, callback](const QString& error) mutable {
        upload->deleteLater();
        answer(callback, error, QString());
    });
    connect(upload, &EditorUpload::finished, this, [upload, callback](const QString& url) mutable {
        upload->deleteLater();
        answer(callback, QString(), url);
    });
}
