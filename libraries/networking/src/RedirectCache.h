//
//  RedirectCache.h
//  libraries/networking/src
//
//  Where a URL's redirect led the last time, kept across runs (ARCHITECTURE.md §3.12, phase 2b). The files host
//  answers every asset URL with a 302 to its storage and no cache headers, so Qt's HTTP cache holds the redirect as
//  stale and the client paid one round trip per asset on every visit, warm or cold, before the asset's own request;
//  GitHub's ?raw=true links are a two-hop redirect marked no-cache. An HTTPResourceRequest asks here first and requests
//  the remembered target directly; a target that fails is forgotten and the original URL asked once more, so a file
//  that moved costs one extra round trip, once. Entries expire after a day. The map lives in redirects.json in the
//  application's local data, written at most every few seconds and at exit (ResourceManager::cleanup).
//
//  Distributed under the MIT License.
//

#ifndef hifi_RedirectCache_h
#define hifi_RedirectCache_h

#include <QtCore/QUrl>

class RedirectCache {
public:
    /// The remembered final URL for url, or an empty QUrl.
    static QUrl target(const QUrl& url);
    /// url's request ended at target after one or more redirects.
    static void remember(const QUrl& url, const QUrl& target);
    /// The remembered target failed: ask the original again next time.
    static void forget(const QUrl& url);
    /// Writes the map if it changed since the last write.
    static void save();

    static const qint64 MAX_AGE_MSECS = 24 * 60 * 60 * 1000;
};

#endif // hifi_RedirectCache_h
