//
//  HTTPResourceRequest.h
//  libraries/networking/src
//
//  Created by Ryan Huffman on 2015/07/23
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_HTTPResourceRequest_h
#define hifi_HTTPResourceRequest_h

#include <QNetworkReply>
#include <QUrl>
#include <QTimer>

#include "ResourceRequest.h"

class HTTPResourceRequest : public ResourceRequest {
    Q_OBJECT
public:
    HTTPResourceRequest(
        const QUrl& url,
        const bool isObservable = true,
        const qint64 callerId = -1,
        const QString& = ""
    ) : ResourceRequest(url, isObservable, callerId) { }
    ~HTTPResourceRequest();

protected:
    virtual void doSend() override;

private slots:
    void onTimeout();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onRequestFinished();

private:
    void setupTimer();
    void cleanupTimer();

    QTimer* _sendTimer { nullptr };
    QNetworkReply* _reply { nullptr };
    QUrl _requestUrl;                     // _url, or the target RedirectCache remembers for it, or _accessTokenUrl
    bool _viaRedirectCache { false };
    bool _triedOriginal { false };        // the remembered target failed and the original URL was asked
    bool _withAccessToken { false };      // our files host refused it without the player's login: asked again with it
    QUrl _accessTokenUrl;                 // the files-host URL that refused it: the only one asked with the login
};

#endif
