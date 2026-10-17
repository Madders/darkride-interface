//
//  AndroidWebView.h
//  interface/src/android
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <QtCore/QtGlobal>

#if defined(Q_OS_ANDROID)

#include <atomic>

#include <QtCore/QString>
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtQuick/QQuickItem>

// A web page in a QML scene on Android, for web entities (qml/+android_interface/Web3DSurface.qml). QtWebEngine,
// which desktop's web views use, doesn't exist for Android: Android's WebView draws into a SurfaceTexture (the Java
// side, io.highfidelity.hifiinterface.web.DarkrideWebView), and each new frame is copied into a texture the scene
// draws, on the scene's render thread.
class AndroidWebView : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QString url READ getUrl WRITE setUrl NOTIFY urlChanged)

public:
    AndroidWebView(QQuickItem* parent = nullptr);
    ~AndroidWebView() override;

    QString getUrl() const { return _url; }
    void setUrl(const QString& url);

    // DarkrideWebView has a new frame (on Android's UI thread)
    static void frameAvailable(jlong id);

signals:
    void urlChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    // the web entity's pointer (Web3DSurface passes it on as touches, or as a mouse for an entity in mouse mode)
    void touchEvent(QTouchEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QSize pixelSize() const;
    // to the WebView as an Android MotionEvent action (ACTION_DOWN, ACTION_MOVE, ACTION_UP or ACTION_CANCEL)
    void sendTouch(int action, const QPointF& position);

    const jlong _id;
    QString _url;
    QAndroidJniObject _javaView;
    std::atomic<bool> _frameAvailable { false };
};

#endif
