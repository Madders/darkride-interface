//
//  UnityYaml.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_UnityYaml_h
#define hifi_UnityYaml_h

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVector>

namespace unity {

// One object of a Unity text file: its "--- !u!<classID> &<fileID>" header and the fields under its top key.
// Values are QVariantMap, QVariantList, QString (scalars stay text), or an invalid QVariant for an empty value.
struct YamlObject {
    int classID { 0 };
    qint64 fileID { 0 };
    bool stripped { false };  // a placeholder for an object that lives in a nested prefab or model
    QString type;             // the top key: "GameObject", "MonoBehaviour", ...
    QVariantMap fields;
};

// The objects of a Unity text-serialized file: prefab, scene, material, animator controller, clip or asset.
QVector<YamlObject> parseObjects(const QByteArray& text, QString* error = nullptr);

// A plain YAML document, such as a .meta file.
QVariant parseDocument(const QByteArray& text, QString* error = nullptr);

// Reading parsed values.
qint64 fileIDOf(const QVariant& reference);  // {fileID: n, guid: g, type: t} -> n
QString guidOf(const QVariant& reference);   // -> g
double toNumber(const QVariant& value, double fallback = 0.0);
// Unity writes property tables (a material's floats, colours and textures) as lists of one-key maps.
QVariantMap mergeKeyList(const QVariant& list);

}

#endif // hifi_UnityYaml_h
