//
//  AndroidHelper.cpp
//  interface/src
//
//  Created by Gabriel Calero & Cristian Duarte on 3/30/18.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "AndroidHelper.h"
#include <QDebug>
#include <AccountManager.h>
#include <AudioClient.h>
#include <src/ui/LoginDialog.h>
#include "Application.h"
#include "LODManager.h"
#include "PerformanceManager.h"
#include "scripting/RenderScriptingInterface.h"
#include <gl/GLHelpers.h>
#include "Constants.h"
#include "avatar/AvatarManager.h"

#if defined(qApp)
#undef qApp
#endif
#define qApp (static_cast<Application*>(QCoreApplication::instance()))

AndroidHelper& AndroidHelper::instance() {
    static AndroidHelper instance;
    return instance;
}

AndroidHelper::AndroidHelper() {
    qRegisterMetaType<QAudio::Mode>("QAudio::Mode");
}

AndroidHelper::~AndroidHelper() {
}

void AndroidHelper::requestActivity(const QString &activityName, const bool backToScene, QMap<QString, QString> args) {
    emit androidActivityRequested(activityName, backToScene, args);
}

int AndroidHelper::getGraphicsPreset() const {
    return (int)qApp->getPerformanceManager().getPerformancePreset();
}

void AndroidHelper::setGraphicsPreset(int preset) {
    QMetaObject::invokeMethod(qApp, [preset] {
        qApp->getPerformanceManager().setPerformancePreset((PerformanceManager::PerformancePreset)preset);
    }, Qt::QueuedConnection);
}

bool AndroidHelper::isDeferredRenderingAvailable() const {
    // (as PerformanceManager decides it)
    return gl::getAvailableVersion() >= GL_MAKE_VERSION(3, 2);
}


// Shadows and ambient occlusion are the deferred renderer's: switching one on switches to it
bool AndroidHelper::getShadowsEnabled() const {
    auto render = RenderScriptingInterface::getInstance();
    return render->getShadowsEnabled() && render->getRenderMethod() == RenderScriptingInterface::RenderMethod::DEFERRED;
}

void AndroidHelper::setShadowsEnabled(bool enabled) {
    bool deferred = isDeferredRenderingAvailable();
    QMetaObject::invokeMethod(qApp, [enabled, deferred] {
        auto render = RenderScriptingInterface::getInstance();
        if (enabled && deferred) {
            render->setRenderMethod(RenderScriptingInterface::RenderMethod::DEFERRED);
        }
        render->setShadowsEnabled(enabled);
    }, Qt::QueuedConnection);
}

bool AndroidHelper::getAmbientOcclusionEnabled() const {
    auto render = RenderScriptingInterface::getInstance();
    return render->getAmbientOcclusionEnabled() && render->getRenderMethod() == RenderScriptingInterface::RenderMethod::DEFERRED;
}

void AndroidHelper::setAmbientOcclusionEnabled(bool enabled) {
    bool deferred = isDeferredRenderingAvailable();
    QMetaObject::invokeMethod(qApp, [enabled, deferred] {
        auto render = RenderScriptingInterface::getInstance();
        if (enabled && deferred) {
            render->setRenderMethod(RenderScriptingInterface::RenderMethod::DEFERRED);
        }
        render->setAmbientOcclusionEnabled(enabled);
    }, Qt::QueuedConnection);
}

float AndroidHelper::getResolutionScale() const {
    return RenderScriptingInterface::getInstance()->getViewportResolutionScale();
}

void AndroidHelper::setResolutionScale(float scale) {
    QMetaObject::invokeMethod(qApp, [scale] {
        RenderScriptingInterface::getInstance()->setViewportResolutionScale(scale);
    }, Qt::QueuedConnection);
}

// RefreshRateManager's profiles: INTERACTIVE draws at 30 frames a second, REALTIME at 60, DISPLAY_BASED at the screen's rate
int AndroidHelper::getFrameRateProfile() const {
    return (int)qApp->getRefreshRateManager().getRefreshRateProfile();
}

void AndroidHelper::setFrameRateProfile(int profile) {
    QMetaObject::invokeMethod(qApp, [profile] {
        qApp->getRefreshRateManager().setRefreshRateProfile((RefreshRateManager::RefreshRateProfile)profile);
    }, Qt::QueuedConnection);
}

// how much small and distant detail is drawn (LODManager lowers it to keep up its frame rate target)
int AndroidHelper::getWorldDetail() const {
    return (int)DependencyManager::get<LODManager>()->getWorldDetailQuality();
}

void AndroidHelper::setWorldDetail(int quality) {
    QMetaObject::invokeMethod(qApp, [quality] {
        DependencyManager::get<LODManager>()->setWorldDetailQuality((WorldDetailQuality)quality);
    }, Qt::QueuedConnection);
}

float AndroidHelper::getDrawDistance() const {
    return RenderScriptingInterface::getInstance()->getFarClip();
}

void AndroidHelper::setDrawDistance(float meters) {
    QMetaObject::invokeMethod(qApp, [meters] {
        RenderScriptingInterface::getInstance()->setFarClip(meters);
    }, Qt::QueuedConnection);
}

void AndroidHelper::notifyLoadComplete() {
    emit qtAppLoadComplete();
}

void AndroidHelper::notifyEnterForeground() {
    emit enterForeground();
}

void AndroidHelper::notifyBeforeEnterBackground() {
    emit beforeEnterBackground();
}

void AndroidHelper::notifyToggleAwayMode() {
    emit toggleAwayMode();
}

void AndroidHelper::notifyEnterBackground() {
    emit enterBackground();
}

void AndroidHelper::performHapticFeedback(int duration) {
    emit hapticFeedbackRequested(duration);
}

void AndroidHelper::showLoginDialog(QUrl url) {
    QMap<QString, QString> args;
    args["url"] = url.toString();
    emit androidActivityRequested("Login", true, args);
}

void AndroidHelper::processURL(const QString &url) {
    if (qApp->canAcceptURL(url)) {
        qApp->acceptURL(url);
    }
}

void AndroidHelper::notifyHeadsetOn(bool pluggedIn) {
#if defined (Q_OS_ANDROID)
    auto audioClient = DependencyManager::get<AudioClient>();
    if (audioClient) {
        QMetaObject::invokeMethod(audioClient.data(), "setHeadsetPluggedIn", Q_ARG(bool, pluggedIn));
    }
#endif
}

void AndroidHelper::muteMic() {
    auto audioClient = DependencyManager::get<AudioClient>();
    if (audioClient) {
        QMetaObject::invokeMethod(audioClient.data(), "setMuted", Q_ARG(bool, true), Q_ARG(bool, true));
    }
}


void AndroidHelper::signup(QString email, QString username, QString password) {
    JSONCallbackParameters callbackParams;
    callbackParams.callbackReceiver = this;
    callbackParams.jsonCallbackMethod = "signupCompleted";
    callbackParams.errorCallbackMethod = "signupFailed";

    QJsonObject payload;

    QJsonObject userObject;
    userObject.insert("email", email);
    userObject.insert("username", username);
    userObject.insert("password", password);

    payload.insert("user", userObject);

    auto accountManager = DependencyManager::get<AccountManager>();

    accountManager->sendRequest(API_SIGNUP_PATH, AccountManagerAuth::None,
                                QNetworkAccessManager::PostOperation, callbackParams,
                                QJsonDocument(payload).toJson());
}

void AndroidHelper::signupCompleted(QNetworkReply* reply) {
    emit handleSignupCompleted();
}

QString AndroidHelper::errorStringFromAPIObject(const QJsonValue& apiObject) {
    if (apiObject.isArray()) {
        return apiObject.toArray()[0].toString();
    } else if (apiObject.isString()) {
        return apiObject.toString();
    } else {
        return "is invalid";
    }
}

void AndroidHelper::signupFailed(QNetworkReply* reply) {
    // parse the returned JSON to see what the problem was
    auto jsonResponse = QJsonDocument::fromJson(reply->readAll());

    static const QString RESPONSE_DATA_KEY = "data";

    auto dataJsonValue = jsonResponse.object()[RESPONSE_DATA_KEY];

    if (dataJsonValue.isObject()) {
        auto dataObject = dataJsonValue.toObject();

        static const QString EMAIL_DATA_KEY = "email";
        static const QString USERNAME_DATA_KEY = "username";
        static const QString PASSWORD_DATA_KEY = "password";

        QStringList errorStringList;

        if (dataObject.contains(EMAIL_DATA_KEY)) {
            errorStringList.append(QString("Email %1.").arg(errorStringFromAPIObject(dataObject[EMAIL_DATA_KEY])));
        }

        if (dataObject.contains(USERNAME_DATA_KEY)) {
            errorStringList.append(QString("Username %1.").arg(errorStringFromAPIObject(dataObject[USERNAME_DATA_KEY])));
        }

        if (dataObject.contains(PASSWORD_DATA_KEY)) {
            errorStringList.append(QString("Password %1.").arg(errorStringFromAPIObject(dataObject[PASSWORD_DATA_KEY])));
        }

        emit handleSignupFailed(errorStringList.join('\n'));
    } else {
        static const QString DEFAULT_SIGN_UP_FAILURE_MESSAGE = "There was an unknown error while creating your account. Please try again later.";
        emit handleSignupFailed(DEFAULT_SIGN_UP_FAILURE_MESSAGE);
    }
}

QString AndroidHelper::getDisplayName() {
    return DependencyManager::get<AvatarManager>()->getMyAvatar()->getDisplayName();
}

void AndroidHelper::setDisplayName(const QString &displayName) {
    DependencyManager::get<AvatarManager>()->getMyAvatar()->setDisplayName(displayName);
}

void AndroidHelper::setMyAvatarUrl(const QString &avatarUrl) {
    QUrl url = QUrl(avatarUrl);
    DependencyManager::get<AvatarManager>()->getMyAvatar()->useFullAvatarURL(url);
}
