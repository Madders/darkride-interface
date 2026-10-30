//
//  FriendsService.h
//  libraries/chat/src
//
//  Darkride friends and the privacy setting (CHAT.md 4.6), from the metaverse with the account's token: your friends
//  (online, and the world they're in when they let you see it), friend requests both ways, and "who sees where I am"
//  (the metaverse's availability: all, friends, none). The chat window's Friends page shows them, and scripts and the
//  panels have them as Friends (interface's chat/FriendsScriptingInterface.h); the routes are the metaverse's
//  /api/v1/friends and /api/v1/user/availability, the same the web chat uses. From sign-in it asks for the requests
//  waiting for you once a minute, so the bar's People button can show them before chat or People is opened.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_FriendsService_h
#define hifi_FriendsService_h

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <DependencyManager.h>

class QNetworkReply;

class FriendsService : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    FriendsService();

    // follows the Darkride account from now on: everything asked at sign-in, then the requests every 10 s while the
    // app is in front, once a minute behind it
    void start();
    // the app came to the front (the requests asked at once) or went behind
    void setInFront(bool inFront);

    // [{username, ai, image, online, location: {domainId, worldId, name, path} | null}], online first, then by name;
    // location is null unless the friend lets you see it and you could join them there (the metaverse's rule, 10-28);
    // otherVersion ("older" or "newer", next to a null location): online in the other version's copy of the worlds
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
    // the server's value came, or a change of it was refused (the same value again, for whatever showed the change)
    void availabilityChanged();
    // an ask, accept, decline, withdraw, remove or setAvailability finished: ok, or the metaverse's reason why not
    void actionDone(const QString& action, const QString& username, bool ok, const QString& message);

private slots:
    void replied(QNetworkReply* reply, const QJsonObject& request);
    void failed(QNetworkReply* reply, const QJsonObject& request);
    void onLoggedIn();
    void onLoggedOut();

private:
    void request(const QString& action, const QString& path, int operation, const QString& username = QString(),
                 const QJsonObject& body = QJsonObject());
    void askRequests();

    QJsonArray _friends;
    QJsonArray _incoming;
    QJsonArray _outgoing;
    QString _availability;
    QString _error;
    QTimer _requestsTimer;
    bool _inFront { true };
    qint64 _requestsAskedAt { 0 };
};

#endif // hifi_FriendsService_h
