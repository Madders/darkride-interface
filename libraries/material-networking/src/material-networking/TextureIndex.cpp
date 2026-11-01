//
//  TextureIndex.cpp
//  libraries/material-networking/src/material-networking
//
//  Distributed under the MIT License.
//

#include "TextureIndex.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMutex>
#include <QtCore/QSaveFile>

#include <PathUtils.h>

#include "MaterialNetworkingLogging.h"

namespace {
    struct Stored {
        QString hash;
        uint32_t keyValueBytes;
        qint64 when;   // ms since the epoch, when the texture was last loaded from the network
    };

    const QString FILE_NAME = "ktx_index.json";
    const qint64 SAVE_EVERY_MSECS = 5 * 1000;
    const int MAX_ENTRIES = 100000;

    QMutex mutex;
    QHash<QString, Stored> entries;
    bool loaded = false;
    bool dirty = false;
    qint64 lastSave = 0;

    QString path() {
        return PathUtils::getAppLocalDataPath() + FILE_NAME;
    }

    QString key(const QUrl& url) {
        return url.adjusted(QUrl::RemoveFragment).toString();
    }

    // under the lock
    void load() {
        loaded = true;
        QFile file(path());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        auto object = QJsonDocument::fromJson(file.readAll()).object();
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            auto entry = it.value().toObject();
            qint64 when = (qint64)entry.value("at").toDouble();
            QString hash = entry.value("hash").toString();
            if (!hash.isEmpty() && now - when < TextureIndex::MAX_AGE_MSECS) {
                entries.insert(it.key(), { hash, (uint32_t)entry.value("kv").toDouble(), when });
            }
        }
        qCDebug(materialnetworking) << "Textures remembered from earlier runs:" << entries.size();
    }

    // under the lock
    void saveIfDue(bool force) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!dirty || (!force && now - lastSave < SAVE_EVERY_MSECS)) {
            return;
        }
        QJsonObject object;
        for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
            object.insert(it.key(), QJsonObject{ { "hash", it.value().hash }, { "kv", (double)it.value().keyValueBytes },
                                                 { "at", (double)it.value().when } });
        }
        QSaveFile file(path());
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
            file.commit();
        }
        dirty = false;
        lastSave = now;
    }
}

TextureIndex::Entry TextureIndex::lookup(const QUrl& url) {
    QMutexLocker lock(&mutex);
    if (!loaded) {
        load();
    }
    auto it = entries.find(key(url));
    if (it == entries.end()) {
        return { "", 0 };
    }
    if (QDateTime::currentMSecsSinceEpoch() - it->when >= MAX_AGE_MSECS) {
        entries.erase(it);
        dirty = true;
        return { "", 0 };
    }
    return { it->hash.toStdString(), it->keyValueBytes };
}

void TextureIndex::remember(const QUrl& url, const std::string& hash, uint32_t keyValueBytes) {
    if (url.isEmpty() || hash.empty()) {
        return;
    }
    QMutexLocker lock(&mutex);
    if (!loaded) {
        load();
    }
    QString k = key(url);
    if (entries.size() >= MAX_ENTRIES && !entries.contains(k)) {
        return;
    }
    entries.insert(k, { QString::fromStdString(hash), keyValueBytes, QDateTime::currentMSecsSinceEpoch() });
    dirty = true;
    saveIfDue(false);
}

void TextureIndex::forget(const QUrl& url) {
    QMutexLocker lock(&mutex);
    if (entries.remove(key(url)) > 0) {
        dirty = true;
        saveIfDue(false);
    }
}

void TextureIndex::save() {
    QMutexLocker lock(&mutex);
    saveIfDue(true);
}
