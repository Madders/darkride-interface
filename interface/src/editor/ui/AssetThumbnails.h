//
//  AssetThumbnails.h
//  interface/src/editor/ui
//
//  Thumbnails of the pictures in My Files, for the Assets panel: fetched by id URL on demand (the visible ones),
//  a few at a time, scaled down and kept on disk under the app data directory, keyed by id and date, so they
//  survive restarts and renames.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AssetThumbnails_h
#define hifi_AssetThumbnails_h

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtGui/QPixmap>

#include "UserFiles.h"

class AssetThumbnails : public QObject {
    Q_OBJECT
public:
    static const int PIXELS = 128;   // the stored size; the grid shows 64 (128 on a Retina display)

    explicit AssetThumbnails(QObject* parent = nullptr);

    // the thumbnail if it is in memory; null otherwise (ask with want)
    QPixmap get(const UserFile& file) const;
    // true once a fetch has been tried and gave no picture (not a picture, private, or unreadable)
    bool isMissing(const UserFile& file) const;
    // the pictures wanted now, nearest first: those on disk are read at once, the rest fetched a few at a time;
    // earlier wants not yet started are dropped
    void want(const QVector<UserFile>& files);

signals:
    void ready(const QString& id);

private:
    static QString keyOf(const UserFile& file);
    QString cachePath(const UserFile& file) const;
    void startNext();
    void finish(const UserFile& file, const QPixmap& pixmap);

    QString _dir;
    QHash<QString, QPixmap> _memory;   // by key
    QSet<QString> _missing;            // keys that gave nothing
    QList<UserFile> _queue;
    QSet<QString> _inFlight;           // keys being fetched
};

#endif // hifi_AssetThumbnails_h
