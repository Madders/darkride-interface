//
//  RequestFilters.cpp
//  interface/src/networking
//
//  Created by Kunal Gosar on 2017-03-10.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RequestFilters.h"

#include <mutex>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
// #include <QWebEngineProfile>

#include <SettingHandle.h>
#include <NetworkingConstants.h>
#include <AccountManager.h>

#include "ContextAwareProfile.h"
#include <BuildInfo.h>

#if !defined(Q_OS_ANDROID)

#include <QtWebEngine/QQuickWebEngineProfile>
#include <QtWebEngine/QQuickWebEngineScript>

namespace {

    bool isAuthableTivoliURL(const QUrl& url) {
        auto metaverseServerURL = NetworkingConstants::METAVERSE_SERVER_URL();
        static const QStringList TIVOLI_HOSTS = {
            metaverseServerURL.toString(),
            "tivolicloud.com",
        };
        const auto& scheme = url.scheme();
        const auto& host = url.host();

        return (
            (scheme == "https" && TIVOLI_HOSTS.contains(host)) ||
            (
                scheme == metaverseServerURL.scheme() &&
                host == metaverseServerURL.host()
            )
        );
    }

    bool isFirstPartySafe(const QUrl& url) {
        return (
            url.isLocalFile() ||
            url.host().compare("things.tivolicloud.com") == 0
        );
    }

    bool isScript(const QString filename) {
        return filename.endsWith(".js", Qt::CaseInsensitive);
    }

    bool isJSON(const QString filename) {
        return filename.endsWith(".json", Qt::CaseInsensitive);
    }

    bool blockLocalFiles(QWebEngineUrlRequestInfo& info) {
        auto requestUrl = info.requestUrl();
        if (!requestUrl.isLocalFile()) {
            // Not a local file, do not block
            return false;
        }

        // We can potentially add whitelisting logic or development environment variables that
        // will allow people to override this setting on a per-client basis here.
        QString targetFilePath = QFileInfo(requestUrl.toLocalFile()).canonicalFilePath();

        // If we get here, we've determined it's a local file and we have no reason not to block it
        qWarning() << "Blocking web access to local file path" << targetFilePath;
        info.block(true);
        return true;
    }
}

// Sites in Darkride get no Cache Storage and no service workers. Qt 5.15's Chromium can crash the whole client in
// Cache Storage: while it adds up a site's storage, a cache that's being replaced can be missing, and
// LegacyCacheStorage::SizeImpl calls Size() on it unchecked (Chromium 87 and 90 too). YouTube's player uses Cache
// Storage, so a web entity showing it took Darkride down. Sites check for both APIs before using them, as plain
// http pages and older browsers don't have them; they only cache for offline use and speed.
void RequestFilters::disableServiceWorkers(QQuickWebEngineProfile* profile) {
    // before the page's own scripts, in every frame (the YouTube player is in an iframe), so 'caches' in window
    // and 'serviceWorker' in navigator are false
    static const QString NO_CACHE_STORAGE = QStringLiteral(
        "(function () {"
        "  try { delete Window.prototype.caches; delete window.caches; } catch (e) {}"
        "  try { delete Navigator.prototype.serviceWorker; } catch (e) {}"
        "})();");
    auto script = new QQuickWebEngineScript(profile);
    script->setName("darkride-no-cache-storage");
    script->setSourceCode(NO_CACHE_STORAGE);
    script->setInjectionPoint(QQuickWebEngineScript::DocumentCreation);
    script->setWorldId(QQuickWebEngineScript::MainWorld);
    script->setRunOnSubframes(true);
    auto scripts = profile->userScripts();
    scripts.append(&scripts, script);

    // The service workers and Cache Storage that sites stored in earlier runs
    static std::once_flag once;
    std::call_once(once, [profile] {
        QDir serviceWorkers(profile->persistentStoragePath() + "/Service Worker");
        if (serviceWorkers.exists()) {
            qDebug() << "Deleting the web views' service workers and Cache Storage in" << serviceWorkers.absolutePath()
                     << ":" << (serviceWorkers.removeRecursively() ? "done" : "failed");
        }
    });
}

void RequestFilters::interceptTivoliWebEngineRequest(QWebEngineUrlRequestInfo& info, bool restricted) {
    // A service worker's script is refused too, in case a page registers one anyway (see disableServiceWorkers)
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeServiceWorker) {
        qDebug() << "Blocked a service worker:" << info.requestUrl().toString();
        info.block(true);
        return;
    }

    if (restricted && blockLocalFiles(info)) {
        return;
    }

    // check if this is a request to a Tivoli URL coming from file:// page
    if (
        isAuthableTivoliURL(info.requestUrl()) &&
        isFirstPartySafe(info.firstPartyUrl())
    ) {
        // if we have an access token, add it to the right HTTP header for authorization
        auto accountManager = DependencyManager::get<AccountManager>();
        if (accountManager->hasValidAccessToken()) {
            static const QString OAUTH_AUTHORIZATION_HEADER = "Authorization";

            QString bearerTokenString = "Bearer " + accountManager->getAccountInfo().getAccessToken().token;
            info.setHttpHeader(OAUTH_AUTHORIZATION_HEADER.toLocal8Bit(), bearerTokenString.toLocal8Bit());
        }
    }

    // line below causes crashes so i set it to what windows 10 would use
    // const QString defaultUserAgent = QWebEngineProfile::defaultProfile()->httpUserAgent();
    const QString defaultUserAgent = "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) QtWebEngine/5.15.0 Chrome/80.0.3987.163 Safari/537.36";

    info.setHttpHeader(
        "User-Agent",
        (
            defaultUserAgent + " Darkride/" +
            (BuildInfo::BUILD_TYPE == BuildInfo::BuildType::Stable ? BuildInfo::VERSION : "dev")
        ).toLocal8Bit()
    );
}

void RequestFilters::interceptFileType(QWebEngineUrlRequestInfo& info) {
    QString filename = info.requestUrl().fileName();
    if (isScript(filename) || isJSON(filename)) {
        static const QString CONTENT_HEADER = "Accept";
        static const QString TYPE_VALUE = "text/plain,text/html";
        info.setHttpHeader(CONTENT_HEADER.toLocal8Bit(), TYPE_VALUE.toLocal8Bit());
    }
}
#endif
