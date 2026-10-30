//
//  MetaverseRequests.h
//  interface/src/ui
//
//  Calls to the metaverse's API for Darkride's own QML panels (ARCHITECTURE.md §5.2.1): what the web pages did with an
//  HTTP request that the browser's request filter signed with the account's token. A panel names a path under /api/;
//  the request goes to the metaverse this client signs in to, signed when an account is signed in. QML panels only:
//  scripts never see the object, so no script can send a request in the account's name or read its token.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_MetaverseRequests_h
#define hifi_MetaverseRequests_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtQml/QJSEngine>
#include <QtQml/QJSValue>

#include <DependencyManager.h>

class MetaverseRequests : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    /// method "GET", "POST", "PUT" or "DELETE"; path under "/api/"; body sent as JSON with a POST or a PUT;
    /// callback(error, status, data):
    /// error null on a 2xx answer, otherwise a short description; data the answer parsed as JSON (null if it is not)
    Q_INVOKABLE void send(const QString& method, const QString& path, const QVariant& body, QJSValue callback);

    /// the metaverse's address, for links a panel shows or opens (a user's settings page, a nametag picture)
    Q_INVOKABLE QString metaverseUrl() const;

    /// the rights line (ui/RightsLine.h), shown under a panel's buttons that put something where others see it, and
    /// the Terms paragraph its "Terms" link opens
    Q_INVOKABLE QString rightsLine() const;
    Q_INVOKABLE QString termsUrl() const;

private:
    MetaverseRequests() = default;
};

#endif // hifi_MetaverseRequests_h
