//
//  RedirectCache.cpp
//  libraries/networking/src
//
//  Distributed under the MIT License.
//

#include "RedirectCache.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMutex>
#include <QtCore/QSaveFile>

#include <PathUtils.h>

#include "NetworkLogging.h"

namespace {
    struct Entry {
        QString target;
        qint64 when;   // ms since the epoch, when the redirect was last followed
    };

    const QString FILE_NAME = "redirects.json";
    const qint64 SAVE_EVERY_MSECS = 5 * 1000;
    const int MAX_ENTRIES = 50000;

    QMutex mutex;
    QHash<QString, Entry> entries;
    bool loaded = false;
    bool dirty = false;
    qint64 lastSave = 0;

    QString path() {
        return PathUtils::getAppLocalDataPath() + FILE_NAME;
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
            QString target = entry.value("to").toString();
            if (!target.isEmpty() && now - when < RedirectCache::MAX_AGE_MSECS) {
                entries.insert(it.key(), { target, when });
            }
        }
        qCDebug(networking) << "Redirects remembered from earlier runs:" << entries.size();
    }

    // under the lock
    void saveIfDue(bool force) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!dirty || (!force && now - lastSave < SAVE_EVERY_MSECS)) {
            return;
        }
        QJsonObject object;
        for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
            object.insert(it.key(), QJsonObject{ { "to", it.value().target }, { "at", (double)it.value().when } });
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

QUrl RedirectCache::target(const QUrl& url) {
    QMutexLocker lock(&mutex);
    if (!loaded) {
        load();
    }
    auto it = entries.find(url.toString());
    if (it == entries.end()) {
        return QUrl();
    }
    if (QDateTime::currentMSecsSinceEpoch() - it->when >= MAX_AGE_MSECS) {
        entries.erase(it);
        dirty = true;
        return QUrl();
    }
    return QUrl(it->target);
}

void RedirectCache::remember(const QUrl& url, const QUrl& target) {
    if (url.isEmpty() || target.isEmpty() || url == target) {
        return;
    }
    QMutexLocker lock(&mutex);
    if (!loaded) {
        load();
    }
    if (entries.size() >= MAX_ENTRIES && !entries.contains(url.toString())) {
        return;
    }
    entries.insert(url.toString(), { target.toString(), QDateTime::currentMSecsSinceEpoch() });
    dirty = true;
    saveIfDue(false);
}

void RedirectCache::forget(const QUrl& url) {
    QMutexLocker lock(&mutex);
    if (entries.remove(url.toString()) > 0) {
        dirty = true;
        saveIfDue(false);
    }
}

void RedirectCache::save() {
    QMutexLocker lock(&mutex);
    saveIfDue(true);
}
