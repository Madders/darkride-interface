//
//  ResourceImageProvider.cpp
//  libraries/ui/src/ui
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "ResourceImageProvider.h"

#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtGui/QImage>

#include <DependencyManager.h>
#include <ResourceManager.h>
#include <ResourceRequest.h>

const QString ResourceImageProvider::NAME = "resource";

namespace {

class ResourceImageResponse : public QQuickImageResponse {
public:
    ResourceImageResponse(const QUrl& url, const QSize& requestedSize) : _link(std::make_shared<Link>()) {
        _link->response = this;
        auto link = _link;
        // the loader's requests are made, and answered, on the main thread
        QMetaObject::invokeMethod(QCoreApplication::instance(), [link, url, requestedSize]() {
            ResourceRequest* request = DependencyManager::get<ResourceManager>()->createResourceRequest(nullptr, url);
            if (!request) {
                link->finish(QImage(), "can't load " + url.toString());
                return;
            }
            QObject::connect(request, &ResourceRequest::finished, request, [link, request, requestedSize]() {
                request->deleteLater();
                QImage image;
                if (request->getResult() == ResourceRequest::Success) {
                    image.loadFromData(request->getData());
                }
                if (!image.isNull() && requestedSize.isValid() && !requestedSize.isEmpty()) {
                    image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                link->finish(image, image.isNull() ? "couldn't load the picture: " + request->getResultString() : QString());
            });
            request->send();
        }, Qt::QueuedConnection);
    }

    ~ResourceImageResponse() override {
        std::lock_guard<std::mutex> lock(_link->mutex);
        _link->response = nullptr;   // a picture no longer wanted (the Image went away): the answer goes nowhere
    }

    QQuickTextureFactory* textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(_image);
    }
    QString errorString() const override { return _error; }

private:
    // the response, while it lives: the answer comes back on the main thread, the response lives on the image loader's
    struct Link {
        std::mutex mutex;
        ResourceImageResponse* response { nullptr };

        void finish(const QImage& image, const QString& error) {
            std::lock_guard<std::mutex> lock(mutex);
            if (response) {
                response->_image = image;
                response->_error = error;
                emit response->finished();
            }
        }
    };

    std::shared_ptr<Link> _link;
    QImage _image;
    QString _error;
};

}

QQuickImageResponse* ResourceImageProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    return new ResourceImageResponse(QUrl(QUrl::fromPercentEncoding(id.toUtf8())), requestedSize);
}
