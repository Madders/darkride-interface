//
//  WebSocketObject.h
//  libraries/script-engine/src
//
//  A WebSocket connection as a plain QObject: the behaviour of WebSocketClass (the WebSocket of the QtScript
//  engine) with signals in place of QScriptValue callbacks, reachable by reflection from the WebAssembly
//  engine. The JavaScript shim wraps one as a WebSocket. WebSocketFactory is the "Sockets" global that makes
//  them; a socket lives until the factory is told to dispose of it or the engine stops.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_WebSocketObject_h
#define hifi_WebSocketObject_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtWebSockets/QWebSocket>

class WebSocketObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString binaryType READ binaryType WRITE setBinaryType)
    Q_PROPERTY(int bufferedAmount READ bufferedAmount)
    Q_PROPERTY(QString extensions READ extensions)
    Q_PROPERTY(QString protocol READ protocol)
    Q_PROPERTY(int readyState READ readyState)
    Q_PROPERTY(QString url READ url)
    Q_PROPERTY(int CONNECTING READ connecting CONSTANT)
    Q_PROPERTY(int OPEN READ open CONSTANT)
    Q_PROPERTY(int CLOSING READ closing CONSTANT)
    Q_PROPERTY(int CLOSED READ closed CONSTANT)
public:
    enum ReadyState { CONNECTING = 0, OPEN = 1, CLOSING = 2, CLOSED = 3 };

    /// Opens the connection to the URL at once, as the constructor of a WebSocket does.
    WebSocketObject(const QString& url, QObject* parent = nullptr);
    /// Wraps a socket that exists already (a server's incoming connection).
    WebSocketObject(QWebSocket* socket, QObject* parent = nullptr);
    ~WebSocketObject() override;

    QString binaryType() const { return _binaryType; }
    void setBinaryType(const QString& type) { _binaryType = type; }
    int bufferedAmount() const { return 0; }
    QString extensions() const { return QString(); }
    QString protocol() const { return QString(); }
    int readyState() const;
    QString url() const { return _socket->requestUrl().toDisplayString(); }
    int connecting() const { return CONNECTING; }
    int open() const { return OPEN; }
    int closing() const { return CLOSING; }
    int closed() const { return CLOSED; }

    Q_INVOKABLE void send(const QString& message);
    Q_INVOKABLE void close(int code = QWebSocketProtocol::CloseCodeNormal, const QString& reason = QString());

signals:
    void opened();
    void messageReceived(const QString& message);
    void errored();
    void closedWith(int code, const QString& reason, bool wasClean);

private:
    void initialize();

    QWebSocket* _socket;
    QString _binaryType { "blob" };
};

/// The "Sockets" global: makes connections for the script that asked.
class WebSocketFactory : public QObject {
    Q_OBJECT
public:
    explicit WebSocketFactory(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QObject* createWebSocket(const QString& url);
    /// Deletes a socket this factory made, once the script is done with it.
    Q_INVOKABLE void dispose(QObject* socket);
};

#endif // hifi_WebSocketObject_h
