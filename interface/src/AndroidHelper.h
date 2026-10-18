//
//  AndroidHelper.h
//  interface/src
//
//  Created by Gabriel Calero & Cristian Duarte on 3/30/18.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Android_Helper_h
#define hifi_Android_Helper_h

#include <QObject>
#include <QMap>
#include <QUrl>

#include <QNetworkReply>
#include <QtCore/QEventLoop>

class AndroidHelper : public QObject {
    Q_OBJECT
public:
    // defined in AndroidHelper.cpp, so the app's native library shares the interface library's instance rather
    // than resolving a copy of its own
    static AndroidHelper& instance();
    void requestActivity(const QString &activityName, const bool backToScene, QMap<QString, QString> args = QMap<QString, QString>());
    void notifyLoadComplete();
    void notifyEnterForeground();
    void notifyBeforeEnterBackground();
    void notifyEnterBackground();
    void notifyToggleAwayMode();

    // Graphics settings, for the app's Settings screen: read from any thread, changed on the app's thread
    int getGraphicsPreset() const;
    void setGraphicsPreset(int preset);
    bool isDeferredRenderingAvailable() const;
    bool getShadowsEnabled() const;
    void setShadowsEnabled(bool enabled);
    bool getAmbientOcclusionEnabled() const;
    void setAmbientOcclusionEnabled(bool enabled);
    float getResolutionScale() const;
    void setResolutionScale(float scale);
    int getFrameRateProfile() const;
    void setFrameRateProfile(int profile);
    int getWorldDetail() const;
    void setWorldDetail(int quality);
    float getDrawDistance() const;
    void setDrawDistance(float meters);

    void performHapticFeedback(int duration);
    void processURL(const QString &url);
    void notifyHeadsetOn(bool pluggedIn);
    void muteMic();

    AndroidHelper(AndroidHelper const&)  = delete;
    void operator=(AndroidHelper const&) = delete;

    void signup(QString email, QString username, QString password);
    QString getDisplayName();
    void setDisplayName(const QString &displayName);
    void setMyAvatarUrl(const QString &avatarUrl);

public slots:
    void showLoginDialog(QUrl url);
    void signupCompleted(QNetworkReply* reply);
    void signupFailed(QNetworkReply* reply);
signals:
    void androidActivityRequested(const QString &activityName, const bool backToScene, QMap<QString, QString> args = QMap<QString, QString>());
    void qtAppLoadComplete();
    void enterForeground();
    void beforeEnterBackground();
    void enterBackground();
    void toggleAwayMode();
    void hapticFeedbackRequested(int duration);

    void handleSignupCompleted();
    void handleSignupFailed(QString errorString);

private:
    AndroidHelper();
    ~AndroidHelper();

    QString errorStringFromAPIObject(const QJsonValue& apiObject);
};

#endif
