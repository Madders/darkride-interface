//
//  WebSocketObject.cpp
//  libraries/script-engine/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WebSocketObject.h"

WebSocketObject::WebSocketObject(const QString& url, QObject* parent) : QObject(parent), _socket(new QWebSocket()) {
    initialize();
    _socket->open(QUrl(url));
}

WebSocketObject::WebSocketObject(QWebSocket* socket, QObject* parent) : QObject(parent), _socket(socket) {
    initialize();
}

WebSocketObject::~WebSocketObject() {
    _socket->deleteLater();
}

void WebSocketObject::initialize() {
    connect(_socket, &QWebSocket::connected, this, &WebSocketObject::opened);
    connect(_socket, &QWebSocket::textMessageReceived, this, &WebSocketObject::messageReceived);
    connect(_socket, static_cast<void (QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error), this,
            [this](QAbstractSocket::SocketError) { emit errored(); });
    connect(_socket, &QWebSocket::disconnected, this, [this] {
        const bool hasError = _socket->error() != QAbstractSocket::UnknownSocketError;
        emit closedWith(hasError ? int(QWebSocketProtocol::CloseCodeAbnormalDisconnection) : int(_socket->closeCode()),
                        _socket->closeReason(), !hasError);
    });
}

int WebSocketObject::readyState() const {
    switch (_socket->state()) {
        case QAbstractSocket::HostLookupState:
        case QAbstractSocket::ConnectingState:
            return CONNECTING;
        case QAbstractSocket::ConnectedState:
        case QAbstractSocket::BoundState:
        case QAbstractSocket::ListeningState:
            return OPEN;
        case QAbstractSocket::ClosingState:
            return CLOSING;
        default:
            return CLOSED;
    }
}

void WebSocketObject::send(const QString& message) {
    _socket->sendTextMessage(message);
}

void WebSocketObject::close(int code, const QString& reason) {
    _socket->close(QWebSocketProtocol::CloseCode(code), reason);
}

// --- the factory ---------------------------------------------------------------------------------------------------

QObject* WebSocketFactory::createWebSocket(const QString& url) {
    return new WebSocketObject(url, this);
}

void WebSocketFactory::dispose(QObject* socket) {
    if (socket && socket->parent() == this) {
        socket->deleteLater();
    }
}
