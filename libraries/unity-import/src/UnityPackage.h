//
//  UnityPackage.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_UnityPackage_h
#define hifi_UnityPackage_h

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <QVector>

#include "UnityYaml.h"

namespace unity {

// A .unitypackage read into memory. The file is a gzipped tar holding, for each asset, a folder named by
// the asset's GUID with "asset" (the file), "asset.meta" (its import settings) and "pathname" (where it goes).
class Package {
public:
    struct Asset {
        QString guid;
        QString path;          // "Assets/..." as Unity would place it
        QByteArray data;       // empty for a folder
        QByteArray meta;
        bool hasData { false };
    };

    bool open(const QString& fileName, QString* error = nullptr);

    const QVector<Asset>& assets() const { return _assets; }
    const Asset* byGuid(const QString& guid) const;
    const Asset* byPath(const QString& path) const;
    // Assets whose path ends with this extension (".prefab"), in path order.
    QList<const Asset*> withExtension(const QString& extension) const;

    // The parsed objects of a text asset, and the parsed .meta of any asset; both cached.
    const QVector<YamlObject>& objects(const QString& guid) const;
    const QVariantMap& meta(const QString& guid) const;
    // The first parse error met so far, if any.
    const QString& parseError() const { return _parseError; }

private:
    bool readTar(const QByteArray& tar, QString* error);

    QVector<Asset> _assets;
    QHash<QString, int> _byGuid;
    QHash<QString, int> _byPath;
    mutable QHash<QString, QVector<YamlObject>> _objects;
    mutable QHash<QString, QVariantMap> _metas;
    mutable QString _parseError;
};

// Inflates gzip data (one or more members).
bool gunzip(const QByteArray& compressed, QByteArray& out, QString* error = nullptr);

}

#endif // hifi_UnityPackage_h
