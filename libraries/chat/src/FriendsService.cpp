//
//  FriendsService.cpp
//  libraries/chat/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "FriendsService.h"

#include <algorithm>

#include <QtCore/QJsonDocument>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <AccountManager.h>

#include "ChatLogging.h"

static QString named(const QString& username) {
    return QString::fromUtf8(QUrl::toPercentEncoding(username));
}

// the metaverse's error: its message (Nest's errors sometimes carry a list of them), else the status
static QString reasonOf(QNetworkReply* reply) {
    QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
    QJsonValue message = answer.value("message");
    if (message.isArray()) {
        QStringList lines;
        for (const QJsonValue& line : message.toArray()) {
            lines.append(line.toString());
        }
        return lines.join("; ");
    }
    if (!message.toString().isEmpty()) {
        return message.toString();
    }
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return status ? QString("Darkride said %1").arg(status) : reply->errorString();
}

FriendsService::FriendsService() {
    connect(DependencyManager::get<AccountManager>().data(), &AccountManager::logoutComplete, this,
            &FriendsService::onLoggedOut);
}

bool FriendsService::isFriend(const QString& username) const {
    for (const QJsonValue& value : _friends) {
        if (value.toObject().value("username").toString().compare(username, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void FriendsService::refresh() {
    if (!DependencyManager::get<AccountManager>()->isLoggedIn()) {
        return;
    }
    request("friends", "/api/v1/friends", QNetworkAccessManager::GetOperation);
    request("requests", "/api/v1/friends/requests", QNetworkAccessManager::GetOperation);
    request("availability", "/api/v1/user/availability", QNetworkAccessManager::GetOperation);
}

void FriendsService::ask(const QString& username) {
    request("ask", "/api/v1/friends/requests", QNetworkAccessManager::PostOperation, username,
            QJsonObject { { "username", username } });
}

void FriendsService::accept(const QString& username) {
    request("accept", "/api/v1/friends/requests/" + named(username) + "/accept", QNetworkAccessManager::PostOperation,
            username);
}

void FriendsService::decline(const QString& username) {
    request("decline", "/api/v1/friends/requests/" + named(username) + "/decline", QNetworkAccessManager::PostOperation,
            username);
}

void FriendsService::withdraw(const QString& username) {
    request("withdraw", "/api/v1/friends/requests/" + named(username), QNetworkAccessManager::DeleteOperation, username);
}

void FriendsService::remove(const QString& username) {
    request("remove", "/api/v1/friends/" + named(username), QNetworkAccessManager::DeleteOperation, username);
}

void FriendsService::setAvailability(const QString& availability) {
    request("setAvailability", "/api/v1/user/availability", QNetworkAccessManager::PutOperation, QString(),
            QJsonObject { { "availability", availability } });
}

void FriendsService::request(const QString& action, const QString& path, int operation, const QString& username,
                             const QJsonObject& body) {
    auto accountManager = DependencyManager::get<AccountManager>();
    if (!accountManager->isLoggedIn()) {
        emit actionDone(action, username, false, "Sign in to Darkride first.");
        return;
    }
    JSONCallbackParameters callback(this, "replied", "failed",
                                    QJsonObject { { "action", action }, { "username", username }, { "body", body } });
    accountManager->sendRequest(path, AccountManagerAuth::Required, (QNetworkAccessManager::Operation)operation, callback,
                                body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void FriendsService::replied(QNetworkReply* reply, const QJsonObject& request) {
    QString action = request.value("action").toString();
    QString username = request.value("username").toString();
    QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object().value("data").toObject();
    if (action == "friends") {
        QJsonArray friends = data.value("friends").toArray();
        QVector<QJsonObject> sorted;
        for (const QJsonValue& value : friends) {
            sorted.append(value.toObject());
        }
        std::sort(sorted.begin(), sorted.end(), [](const QJsonObject& a, const QJsonObject& b) {
            bool aOnline = a.value("online").toBool();
            bool bOnline = b.value("online").toBool();
            if (aOnline != bOnline) {
                return aOnline;
            }
            return a.value("username").toString().compare(b.value("username").toString(), Qt::CaseInsensitive) < 0;
        });
        _friends = QJsonArray();
        for (const QJsonObject& item : sorted) {
            _friends.append(item);
        }
        _error.clear();
        emit changed();
    } else if (action == "requests") {
        _incoming = data.value("incoming").toArray();
        _outgoing = data.value("outgoing").toArray();
        emit changed();
    } else if (action == "availability" || action == "setAvailability") {
        QString availability = data.value("availability").toString();
        if (action == "setAvailability" && availability.isEmpty()) {
            // a PUT that answers without the value: it took the one sent
            availability = request.value("body").toObject().value("availability").toString();
        }
        if (!availability.isEmpty() && availability != _availability) {
            _availability = availability;
            emit availabilityChanged();
        }
        if (action == "setAvailability") {
            emit actionDone(action, username, true, QString());
            refresh();
        }
    } else {
        QString message;
        if (action == "ask") {
            message = data.value("friends").toBool() ? "You're friends with " + username + " now."
                                                     : "Friend request sent to " + username + ".";
        }
        emit actionDone(action, username, true, message);
        // the lists change with every answer: asked again rather than guessed
        refresh();
    }
}

void FriendsService::failed(QNetworkReply* reply, const QJsonObject& request) {
    QString action = request.value("action").toString();
    QString reason = reasonOf(reply);
    if (action == "friends" || action == "requests" || action == "availability") {
        qCDebug(chat_service) << "friends:" << action << "didn't load:" << reason;
        if (action != "availability") {
            _error = "Friends didn't load: " + reason;
            emit changed();
        }
        return;
    }
    emit actionDone(action, request.value("username").toString(), false, reason);
}

void FriendsService::onLoggedOut() {
    _friends = QJsonArray();
    _incoming = QJsonArray();
    _outgoing = QJsonArray();
    _availability.clear();
    _error.clear();
    emit changed();
    emit availabilityChanged();
}
