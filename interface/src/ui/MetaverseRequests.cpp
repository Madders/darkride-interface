//
//  MetaverseRequests.cpp
//  interface/src/ui
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "MetaverseRequests.h"

#include <QtCore/QJsonDocument>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <AccountManager.h>
#include <NetworkAccessManager.h>
#include <NetworkingConstants.h>

#include "InterfaceLogging.h"

void MetaverseRequests::send(const QString& method, const QString& path, const QVariant& body, QJSValue callback) {
    auto answer = [callback](const QJSValue& error, int status, const QVariant& data) mutable {
        if (callback.isCallable()) {
            callback.call({ error, QJSValue(status), callback.engine() ? callback.engine()->toScriptValue(data) : QJSValue() });
        }
    };
    if (!path.startsWith("/api/") || path.contains("..")) {
        answer(QJSValue("not a metaverse API path: " + path), 0, QVariant());
        return;
    }
    if (method != "GET" && method != "POST" && method != "PUT" && method != "DELETE") {
        answer(QJSValue("unsupported method: " + method), 0, QVariant());
        return;
    }

    QUrl url = NetworkingConstants::METAVERSE_SERVER_URL();
    url.setPath(path.section('?', 0, 0));
    if (path.contains('?')) {
        url.setQuery(path.section('?', 1));
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, TIVOLI_CLOUD_VR_USER_AGENT);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto accountManager = DependencyManager::get<AccountManager>();
    if (accountManager->hasValidAccessToken()) {
        request.setRawHeader(ACCESS_TOKEN_AUTHORIZATION_HEADER,
                             accountManager->getAccountInfo().getAccessToken().authorizationHeaderValue());
    }

    auto& networkAccessManager = NetworkAccessManager::getInstance();
    const QByteArray json = QJsonDocument::fromVariant(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = method == "POST" ? networkAccessManager.post(request, json)
        : method == "PUT" ? networkAccessManager.put(request, json)
        : method == "DELETE" ? networkAccessManager.deleteResource(request)
        : networkAccessManager.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, answer]() mutable {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QVariant data = QJsonDocument::fromJson(reply->readAll()).toVariant();
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            answer(QJSValue(QJSValue::NullValue), status, data);
        } else {
            qCDebug(interfaceapp) << "Metaverse request failed:" << reply->url().path() << status << reply->errorString();
            answer(QJSValue(status > 0 ? QString("the metaverse answered %1").arg(status) : reply->errorString()), status, data);
        }
        reply->deleteLater();
    });
}

QString MetaverseRequests::metaverseUrl() const {
    return NetworkingConstants::METAVERSE_SERVER_URL().toString();
}
