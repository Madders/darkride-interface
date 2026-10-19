//
//  HttpRequestObject.h
//  libraries/script-engine/src
//
//  An HTTP request as a plain QObject: the behaviour of XMLHttpRequestClass (the XMLHttpRequest of the
//  QtScript engine) with QVariant values and signals in place of QScriptValue callbacks, so that a script
//  on the WebAssembly engine reaches it by reflection. The JavaScript shim wraps one of these as an
//  XMLHttpRequest; a native module can use it as it is. HttpRequestFactory is the "Http" global that
//  makes them; a request lives until the factory is told to dispose of it or the engine stops.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_HttpRequestObject_h
#define hifi_HttpRequestObject_h

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

class HttpRequestObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariant response READ response)
    Q_PROPERTY(QString responseText READ responseText)
    Q_PROPERTY(QString responseType READ responseType WRITE setResponseType)
    Q_PROPERTY(int status READ status)
    Q_PROPERTY(QString statusText READ statusText)
    Q_PROPERTY(int readyState READ readyState)
    Q_PROPERTY(int errorCode READ errorCode)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout)
    Q_PROPERTY(int UNSENT READ unsent CONSTANT)
    Q_PROPERTY(int OPENED READ opened CONSTANT)
    Q_PROPERTY(int HEADERS_RECEIVED READ headersReceived CONSTANT)
    Q_PROPERTY(int LOADING READ loading CONSTANT)
    Q_PROPERTY(int DONE READ done CONSTANT)
public:
    enum ReadyState { UNSENT = 0, OPENED = 1, HEADERS_RECEIVED = 2, LOADING = 3, DONE = 4 };

    explicit HttpRequestObject(QObject* parent = nullptr);
    ~HttpRequestObject() override;

    QVariant response() const { return _response; }
    QString responseText() const { return QString::fromUtf8(_rawResponse); }
    QString responseType() const { return _responseType; }
    void setResponseType(const QString& type) { _responseType = type; }
    int status() const;
    QString statusText() const;
    int readyState() const { return _readyState; }
    int errorCode() const { return _errorCode; }
    int timeout() const { return _timeout; }
    void setTimeout(int timeout) { _timeout = timeout; }
    int unsent() const { return UNSENT; }
    int opened() const { return OPENED; }
    int headersReceived() const { return HEADERS_RECEIVED; }
    int loading() const { return LOADING; }
    int done() const { return DONE; }

    Q_INVOKABLE void open(const QString& method, const QString& url, bool async = true, const QString& username = QString(),
                          const QString& password = QString());
    Q_INVOKABLE void send();
    /// Text is sent as UTF-8; bytes as they are.
    Q_INVOKABLE void send(const QVariant& data);
    Q_INVOKABLE void abort();
    Q_INVOKABLE void setRequestHeader(const QString& name, const QString& value);
    Q_INVOKABLE QString getAllResponseHeaders() const;
    /// The header's value, or null when the reply has no such header.
    Q_INVOKABLE QVariant getResponseHeader(const QString& name) const;

signals:
    void readyStateChanged();
    void timedOut();
    void requestComplete();

private slots:
    void onFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onTimeout();

private:
    void setReadyState(ReadyState state);
    void doSend();
    void abortRequest();

    QNetworkRequest _request;
    QNetworkReply* _reply { nullptr };
    QUrl _url;
    QString _method;
    QByteArray _sendData;
    QByteArray _rawResponse;
    QVariant _response;
    QString _responseType;
    QList<QNetworkReply::RawHeaderPair> _headers;  // kept after the reply is gone
    int _lastStatus { 0 };
    QString _lastStatusText;
    ReadyState _readyState { UNSENT };
    int _errorCode { QNetworkReply::NoError };
    int _timeout { 0 };
    bool _async { true };
    QTimer _timer;
};

/// The "Http" global: makes requests for the script that asked.
class HttpRequestFactory : public QObject {
    Q_OBJECT
public:
    explicit HttpRequestFactory(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QObject* createRequest();
    /// Deletes a request this factory made, once the script is done with it.
    Q_INVOKABLE void dispose(QObject* request);
};

#endif // hifi_HttpRequestObject_h
