//
//  PhonePanels.h
//  interface/src/android
//
//  Darkride's panels on the phone, which has no docks (ui/Panels.h): one at a time, over the world. A QML panel
//  (Settings, Avatar, People, Explore) shows full-screen in the app's own QML UI, the desktop the bar's buttons are on,
//  in a frame with a close box that takes every touch (qml/darkride/+android_interface/PanelHost.qml). Its page is
//  made in the desktop's context, so it finds Settings, MetaverseRequests, MyAvatar, Panels and the rest by the same
//  names as in a dock, and is hidden with the frame, so it reads its values again on each showing, as in a dock.
//  The virtual pad is hidden while one shows. The phone's back closes it (InterfaceActivity).
//  Chat is a web page, in Android's WebView over the world (AndroidChatPage.h, and the app's WebPanel); it can open
//  over a QML panel (People's Message), which shows again when it closes.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once

#include <QtCore/QtGlobal>

#if defined(Q_OS_ANDROID)

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QUrl>

class QQuickItem;

class PhonePanels : public QObject {
    Q_OBJECT
public:
    explicit PhonePanels(QObject* parent = nullptr);
    ~PhonePanels() override;

    // main thread, all
    bool isOpen(const QString& name) const;
    // a QML panel, full-screen over the world, in place of the one showing
    void openQml(const QString& name, const QUrl& source);
    // chat, at a link inside it ("/dm/<username>") when there is one
    void openChat(const QString& link = QString());
    void close(const QString& name);
    // a QML panel's page, while it's the one made
    QQuickItem* page(const QString& name) const;
    // at quit, before the QML engine goes (Panels::releaseQml)
    void release();

    // the phone's back (on Android's UI thread): closes the QML panel showing, and says whether there was one
    static bool back();
    // the app's web panel has closed (on Android's UI thread)
    static void webPanelClosed(const QString& name);

signals:
    void openChanged(const QString& name, bool open);

private:
    void makeHost();
    void showWanted();
    void setShowing(const QString& name);
    void setWebShowing(const QString& name);

    QPointer<QQuickItem> _host;
    QString _showing;          // the QML panel showing, if any
    QString _webShowing;       // the web panel over it, if any
    QString _wanted;           // the one to show once the frame is made
    QUrl _wantedSource;
    bool _padWasHidden { false };
};

#endif
