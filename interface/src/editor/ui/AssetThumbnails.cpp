//
//  AssetThumbnails.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AssetThumbnails.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtGui/QImageReader>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <NetworkAccessManager.h>
#include <PathUtils.h>

#include "../../InterfaceLogging.h"

namespace {
    const int FETCHES_AT_ONCE = 4;
    const qint64 LARGEST_FETCH = 64 * 1024 * 1024;   // a picture bigger than this is left without a thumbnail
}

AssetThumbnails::AssetThumbnails(QObject* parent) : QObject(parent) {
    _dir = PathUtils::getAppLocalDataPath() + "editor-thumbnails/";
    QDir().mkpath(_dir);
}

QString AssetThumbnails::keyOf(const UserFile& file) {
    return file.id + "-" + QString::number(file.updated.toSecsSinceEpoch());
}

QString AssetThumbnails::cachePath(const UserFile& file) const {
    return _dir + keyOf(file) + ".png";
}

QPixmap AssetThumbnails::get(const UserFile& file) const {
    return _memory.value(keyOf(file));
}

bool AssetThumbnails::isMissing(const UserFile& file) const {
    return _missing.contains(keyOf(file));
}

void AssetThumbnails::want(const QVector<UserFile>& files) {
    _queue.clear();
    for (const auto& file : files) {
        if (file.kind != UserFile::Kind::Picture || file.id.isEmpty() || file.url.isEmpty()) {
            continue;
        }
        QString key = keyOf(file);
        if (_memory.contains(key) || _missing.contains(key) || _inFlight.contains(key)) {
            continue;
        }
        // on disk from an earlier run: read now, no fetch
        QPixmap onDisk(cachePath(file));
        if (!onDisk.isNull()) {
            _memory.insert(key, onDisk);
            emit ready(file.id);
            continue;
        }
        _queue << file;
    }
    startNext();
}

void AssetThumbnails::startNext() {
    while (_inFlight.size() < FETCHES_AT_ONCE && !_queue.isEmpty()) {
        UserFile file = _queue.takeFirst();
        QString key = keyOf(file);
        _inFlight.insert(key);
        QNetworkRequest request{ QUrl(file.url) };
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);   // the files host redirects to the CDN
        QNetworkReply* reply = NetworkAccessManager::getInstance().get(request);
        QPointer<AssetThumbnails> self(this);
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64, qint64 total) {
            if (total > LARGEST_FETCH) {
                reply->abort();
            }
        });
        connect(reply, &QNetworkReply::finished, reply, [self, reply, file] {
            reply->deleteLater();
            if (!self) {
                return;
            }
            QPixmap pixmap;
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray bytes = reply->readAll();
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer);
                reader.setAutoTransform(true);
                // decoded small where the format allows it (JPEG), else decoded whole and scaled
                QSize size = reader.size();
                if (size.isValid() && (size.width() > PIXELS || size.height() > PIXELS)) {
                    reader.setScaledSize(size.scaled(PIXELS, PIXELS, Qt::KeepAspectRatio));
                }
                QImage image = reader.read();
                if (!image.isNull()) {
                    if (image.width() > PIXELS || image.height() > PIXELS) {
                        image = image.scaled(PIXELS, PIXELS, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    pixmap = QPixmap::fromImage(image);
                }
            } else {
                qCDebug(interfaceapp) << "thumbnail not fetched" << file.path << reply->errorString();
            }
            self->finish(file, pixmap);
        });
    }
}

void AssetThumbnails::finish(const UserFile& file, const QPixmap& pixmap) {
    QString key = keyOf(file);
    _inFlight.remove(key);
    if (pixmap.isNull()) {
        _missing.insert(key);
    } else {
        _memory.insert(key, pixmap);
        // the file's earlier versions go, this one stays for the next run
        for (const auto& old : QDir(_dir).entryList({ file.id + "-*.png" }, QDir::Files)) {
            QFile::remove(_dir + old);
        }
        pixmap.save(cachePath(file), "PNG");
    }
    emit ready(file.id);
    startNext();
}
