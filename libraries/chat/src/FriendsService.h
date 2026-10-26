//
//  FriendsService.h
//  libraries/chat/src
//
//  Darkride friends and the privacy setting (CHAT.md 4.6), from the metaverse with the account's token: your friends
//  (online, and the world they're in when they let you see it), friend requests both ways, and "who sees where I am"
//  (the metaverse's availability: all, friends, none). The chat window's Friends page shows them; the routes are the
//  metaverse's /api/v1/friends and /api/v1/user/availability, the same the web chat uses.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_FriendsService_h
#define hifi_FriendsService_h

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <DependencyManager.h>

class QNetworkReply;

class FriendsService : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    FriendsService();

    // [{username, ai, image, online, location: {domainId, worldId, name, path} | null}], online first, then by name;
    // location is null unless the friend lets you see it
    QJsonArray friends() const { return _friends; }
    // [{username, image, createdAt}]: asked of you, and asked by you
    QJsonArray incoming() const { return _incoming; }
    QJsonArray outgoing() const { return _outgoing; }
    // all, friends or none; empty until the metaverse has said
    QString availability() const { return _availability; }
    // why the lists didn't load, if they didn't
    QString error() const { return _error; }
    bool isFriend(const QString& username) const;

    // asks again (the lists and the setting); nothing while signed out
    void refresh();
    // a friend request (if they had asked you, you're friends at once), answered by actionDone
    void ask(const QString& username);
    void accept(const QString& username);
    void decline(const QString& username);
    // your own request, taken back
    void withdraw(const QString& username);
    // no longer friends
    void remove(const QString& username);
    void setAvailability(const QString& availability);

signals:
    void changed();
    void availabilityChanged();
    // an ask, accept, decline, withdraw, remove or setAvailability finished: ok, or the metaverse's reason why not
    void actionDone(const QString& action, const QString& username, bool ok, const QString& message);

private slots:
    void replied(QNetworkReply* reply, const QJsonObject& request);
    void failed(QNetworkReply* reply, const QJsonObject& request);
    void onLoggedOut();

private:
    void request(const QString& action, const QString& path, int operation, const QString& username = QString(),
                 const QJsonObject& body = QJsonObject());

    QJsonArray _friends;
    QJsonArray _incoming;
    QJsonArray _outgoing;
    QString _availability;
    QString _error;
};

#endif // hifi_FriendsService_h
