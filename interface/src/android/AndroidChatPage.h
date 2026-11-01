//
//  AndroidChatPage.h
//  interface/src/android
//
//  Chat on the phone. The chat library isn't built for Android (libraries/chat/android), so, as in VR
//  (chat/ChatTabletPage.h), the Chat panel is Darkride chat's web page, here in Android's WebView full-screen over the
//  world (the app's WebPanel), signed in as the same account: the page asks over its EventBridge, and this answers
//  with a two-minute sign-in token from the metaverse.
//
//  Its messages, {type: "darkride-chat", command, args}, are answered only when they carry the one-time code the page
//  was opened with (args.bridge), a new one each time it opens; and the app passes on only those from the chat
//  server's own page (WebPanel). This world's lines go to the page and yours come back, as on the tablet; a link to
//  travel is taken only as a hifi:// address.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once

#include <QtCore/QtGlobal>

#if defined(Q_OS_ANDROID)

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

class AndroidChatPage : public QObject {
    Q_OBJECT
public:
    // the one there is, made the first time it's asked for (main thread)
    static AndroidChatPage* instance();

    // the page, at a link inside chat ("/dm/<username>") when there is one; or closed
    void open(const QString& link = QString());
    void close();
    // the page has closed (PhonePanels hears it from the app): its code no longer answers
    void closed();

    // from the app's WebPanel, on Android's UI thread: the page's message, as JSON
    static void pageMessage(const QString& json);

private:
    AndroidChatPage();
    void message(const QString& json);
    void signIn();
    void sendWorld();
    void answer(QJsonObject message);

    QString _bridge;      // the code the page was opened with; empty once it has closed
};

#endif
