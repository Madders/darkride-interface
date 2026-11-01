//
//  AndroidChatPage.cpp
//  interface/src/android
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "AndroidChatPage.h"

#if defined(Q_OS_ANDROID)

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QPointer>
#include <QtCore/QRandomGenerator>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkReply>
#include <QtAndroidExtras/QAndroidJniObject>

#include <AccountManager.h>
#include <AddressManager.h>
#include <ChatService.h>
#include <DependencyManager.h>
#include <NetworkAccessManager.h>
#include <NetworkingConstants.h>

#include "InterfaceLogging.h"
#include "chat/WorldChat.h"

namespace {
    // the web chat, as the tablet's (chat/ChatTabletPage.cpp): ChatService names the chat server once its library has
    // started, which on the phone it never does
    const QString DEFAULT_CHAT_URL = "https://chat.darkride.ai/";
    const QString CHAT_TOKEN_PATH = "/api/v1/user/chat-token";
    const QString MESSAGE_TYPE = "darkride-chat";
    const QString TITLE = "Chat";
    const char* WEB_PANEL_CLASS = "io/highfidelity/hifiinterface/web/WebPanel";

    QPointer<AndroidChatPage> thePage;
}

AndroidChatPage* AndroidChatPage::instance() {
    if (!thePage) {
        thePage = new AndroidChatPage();
    }
    return thePage;
}

AndroidChatPage::AndroidChatPage() : QObject(qApp) {
    // this world's lines to the page as they change, while it's open
    connect(WorldChat::instance(), &WorldChat::itemsChanged, this, [this] {
        if (!_bridge.isEmpty()) {
            sendWorld();
        }
    });
}

void AndroidChatPage::open(const QString& link) {
    QString server = DependencyManager::get<ChatService>()->homeserver();
    QUrl url(server.isEmpty() ? DEFAULT_CHAT_URL : server);
    url.setPath("/");
    // a new code each time the page opens, from the system's random source: 128 bits
    QByteArray code(16, 0);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(code.data()), code.size() / sizeof(quint32));
    _bridge = QString::fromLatin1(code.toHex());
    QUrlQuery query;
    query.addQueryItem("metaverseUrl", NetworkingConstants::METAVERSE_SERVER_URL().toString());
    query.addQueryItem("bridge", _bridge);
    url.setQuery(query);
    if (!link.isEmpty()) {
        url.setFragment(link);   // the page opens it once it has synced (the web chat's deep links)
    }
    // only the chat server's own page may talk to the app (WebPanel)
    QString origin = url.scheme() + "://" + url.host() + (url.port() > 0 ? ":" + QString::number(url.port()) : QString());
    qCDebug(interfaceapp) << "chat on the phone:" << url.toString(QUrl::RemoveQuery);
    QAndroidJniObject::callStaticMethod<void>(WEB_PANEL_CLASS, "openChat",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QAndroidJniObject::fromString(TITLE).object<jstring>(),
        QAndroidJniObject::fromString(url.toString()).object<jstring>(),
        QAndroidJniObject::fromString(origin).object<jstring>());
}

void AndroidChatPage::close() {
    _bridge.clear();
    QAndroidJniObject::callStaticMethod<void>(WEB_PANEL_CLASS, "close", "(Ljava/lang/String;)V",
        QAndroidJniObject::fromString(TITLE).object<jstring>());
}

void AndroidChatPage::closed() {
    _bridge.clear();
}

void AndroidChatPage::pageMessage(const QString& json) {
    QMetaObject::invokeMethod(qApp, [json] {
        if (thePage) {
            thePage->message(json);
        }
    }, Qt::QueuedConnection);
}

void AndroidChatPage::message(const QString& json) {
    // EventBridge.emitWebEvent's message, as JSON text (WebPanel's bridge sends an object as its JSON)
    QJsonObject event = QJsonDocument::fromJson(json.toUtf8()).object();
    if (event.value("type").toString() != MESSAGE_TYPE) {
        return;
    }
    QJsonObject args = event.value("args").toObject();
    QString command = event.value("command").toString();
    if (_bridge.isEmpty() || args.value("bridge").toString() != _bridge) {
        qCWarning(interfaceapp) << "chat on the phone: a" << command << "without the page's code, ignored";
        return;
    }
    if (command == "signIn") {
        signIn();
    } else if (command == "world") {
        sendWorld();
    } else if (command == "worldSend") {
        WorldChat::instance()->send(args.value("text").toString());
    } else if (command == "worldRead") {
        WorldChat::instance()->markRead();
    } else if (command == "goToAddress") {
        // a friend's Join (hifi://@username), or a place link: the world shows, not chat
        static const QRegularExpression USER("^hifi://@([^/?#]+)$", QRegularExpression::CaseInsensitiveOption);
        QString address = args.value("address").toString();
        auto user = USER.match(address);
        if (user.hasMatch()) {
            // the metaverse checks again that they let you
            DependencyManager::get<AddressManager>()->goToUser(user.captured(1));
            close();
        } else if (address.startsWith("hifi://", Qt::CaseInsensitive)) {
            DependencyManager::get<AddressManager>()->handleLookupString(address);
            close();
        }
    }
    // "notify" (a direct message or a mention): nothing here
}

void AndroidChatPage::signIn() {
    auto accountManager = DependencyManager::get<AccountManager>();
    if (!accountManager->isLoggedIn()) {
        answer({ { "event", "signIn" }, { "status", 401 }, { "error", "Sign in to Darkride first." } });
        return;
    }
    QNetworkRequest request = accountManager->createRequest(CHAT_TOKEN_PATH, AccountManagerAuth::Required);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = NetworkAccessManager::getInstance().post(request, QByteArray());
    QString bridge = _bridge;
    connect(reply, &QNetworkReply::finished, this, [this, reply, bridge] {
        reply->deleteLater();
        if (bridge != _bridge) {
            return;   // the page went meanwhile: the token goes to no other
        }
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(interfaceapp) << "chat on the phone: no chat token:" << status << reply->errorString();
            answer({ { "event", "signIn" }, { "status", status }, { "error", body.value("message").toString() } });
            return;
        }
        QJsonObject data = body.value("data").toObject();
        answer({ { "event", "signIn" },
                 { "token", data.value("token") },
                 { "homeserver", data.value("homeserver") },
                 { "user_id", data.value("user_id") },
                 { "display_name", data.value("display_name") } });
    });
}

void AndroidChatPage::sendWorld() {
    answer({ { "event", "world" }, { "items", WorldChat::instance()->items() } });
}

void AndroidChatPage::answer(QJsonObject message) {
    if (_bridge.isEmpty()) {
        return;
    }
    message.insert("type", MESSAGE_TYPE);
    QString json = QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact));
    // to the chat page's main frame only, the one that asked (WebPanel)
    QAndroidJniObject::callStaticMethod<void>(WEB_PANEL_CLASS, "sendToChat", "(Ljava/lang/String;)V",
        QAndroidJniObject::fromString(json).object<jstring>());
}

extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_web_WebPanel_nativeChatMessage(JNIEnv*, jclass, jstring json) {
    AndroidChatPage::pageMessage(QAndroidJniObject(json).toString());
}

#endif
