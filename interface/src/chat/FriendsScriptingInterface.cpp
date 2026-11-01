//
//  FriendsScriptingInterface.cpp
//  interface/src/chat
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "FriendsScriptingInterface.h"

#include <ChatService.h>
#include <FriendsService.h>

#include "DiscoverabilityManager.h"
#include "InterfaceLogging.h"
#include "chat/ChatWindow.h"

FriendsScriptingInterface::FriendsScriptingInterface() {
    auto friends = DependencyManager::get<FriendsService>();
    connect(friends.data(), &FriendsService::changed, this, &FriendsScriptingInterface::updateRequests);
    connect(friends.data(), &FriendsService::availabilityChanged, this, &FriendsScriptingInterface::updateAvailability);
    auto chat = DependencyManager::get<ChatService>();
    connect(chat.data(), &ChatService::stateChanged, this, &FriendsScriptingInterface::updateCanMessage);
    updateRequests();
    updateAvailability();
    updateCanMessage();
}

int FriendsScriptingInterface::getRequestCount() const {
    QMutexLocker lock(&_mutex);
    return _requestCount;
}

QString FriendsScriptingInterface::getAvailability() const {
    QMutexLocker lock(&_mutex);
    return _availability;
}

bool FriendsScriptingInterface::getCanMessage() const {
    QMutexLocker lock(&_mutex);
    return _canMessage;
}

void FriendsScriptingInterface::setAvailability(const QString& availability) {
    if (availability != "all" && availability != "friends" && availability != "none") {
        qCWarning(interfaceapp) << "Friends.setAvailability: not all, friends or none:" << availability;
        return;
    }
    QMetaObject::invokeMethod(this, [availability] {
        DependencyManager::get<FriendsService>()->setAvailability(availability);
    });
}

void FriendsScriptingInterface::openDirectMessage(const QString& username) {
    if (username.trimmed().isEmpty()) {
        return;
    }
    QMetaObject::invokeMethod(this, [username] { ChatWindow::openDirectMessage(username.trimmed()); });
}

void FriendsScriptingInterface::refresh() {
    QMetaObject::invokeMethod(this, [] { DependencyManager::get<FriendsService>()->refresh(); });
}

void FriendsScriptingInterface::updateRequests() {
    int count = DependencyManager::get<FriendsService>()->incoming().size();
    {
        QMutexLocker lock(&_mutex);
        if (count == _requestCount) {
            return;
        }
        _requestCount = count;
    }
    emit requestCountChanged();
}

void FriendsScriptingInterface::updateAvailability() {
    QString availability = DependencyManager::get<FriendsService>()->availability();
    {
        QMutexLocker lock(&_mutex);
        _availability = availability;
    }
    // the app's own copy (High Fidelity's Discoverability) kept in step with the server's, so the location updates
    // that carry it never send an old one, and with "No one" the app doesn't send where you are at all
    auto discoverability = DependencyManager::get<DiscoverabilityManager>();
    Discoverability::Mode mode = discoverability->getDiscoverabilityMode();
    QString local = mode == Discoverability::Connections ? "friends" : DiscoverabilityManager::findableByString(mode);
    if (!availability.isEmpty() && availability != local) {
        discoverability->setDiscoverabilityMode(availability == "all"       ? Discoverability::All
                                                : availability == "none"    ? Discoverability::None
                                                                            : Discoverability::Friends);
    }
    // also when the value is the same: a refused change, for whatever showed it to be put back
    emit availabilityChanged();
}

void FriendsScriptingInterface::updateCanMessage() {
    auto state = DependencyManager::get<ChatService>()->state();
    bool canMessage = state == ChatService::State::Ready || state == ChatService::State::SigningIn;
    {
        QMutexLocker lock(&_mutex);
        if (canMessage == _canMessage) {
            return;
        }
        _canMessage = canMessage;
    }
    emit canMessageChanged();
}
