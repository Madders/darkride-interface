//
//  ResourceImageProvider.h
//  libraries/ui/src/ui
//
//  "image://resource/<address, percent-encoded>": a picture a QML panel shows, fetched through the engine's own
//  loader (ResourceManager), as models and textures are, instead of the QML engine's plain web request. So a panel
//  can show a private (tea-only) file in My Files, which the files host passes only to a signed-in Darkride client
//  (the loader asks again with the login when it refuses), and tea:// and atp: addresses. The Avatar panel's
//  pictures of your avatars use it (Caity, 10-24: a private avatar's picture stays private).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_ResourceImageProvider_h
#define hifi_ResourceImageProvider_h

#include <QtQuick/QQuickAsyncImageProvider>

class ResourceImageProvider : public QQuickAsyncImageProvider {
public:
    static const QString NAME;   // "resource"

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;
};

#endif // hifi_ResourceImageProvider_h
