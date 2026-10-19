//
//  PrefabOverrides.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_PrefabOverrides_h
#define hifi_PrefabOverrides_h

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <hfm/HFM.h>

namespace unity {
class Package;
}

namespace avatarimport {

// What an avatar's prefab changes on its model (Unity's prefab overrides on the model's objects, found by their
// model-object IDs), with room for what the effects layer's starting animations set on top: each joint's local transform
// in Unity's frame, which objects show, and the renderers' shape-key weights (by index) and materials (by slot).
struct ModelState {
    struct Pose {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale { 1.0f };
        bool changed { false };
    };
    QVector<Pose> poses;   // per joint, starting from the model file's
    Pose root;             // the model's root object: the avatar's own transform
    QHash<int, bool> active;                     // joint -> shown, where the prefab or an animation says
    QHash<int, bool> drawn;                      // renderer joint -> enabled, where the prefab says
    QHash<int, QMap<int, float>> shapeWeights;   // renderer joint -> shape-key index -> weight (0 to 100)
    QHash<int, QMap<int, QString>> materials;    // renderer joint -> slot -> material GUID
    int stale { 0 };       // overrides of objects the model no longer has
};

// The model's objects by model-object ID (UnityIds.h): the root (-1), and each joint's GameObject, Transform and
// renderers, by the joint's Unity path (unity::jointPaths).
QHash<qint64, int> modelObjectJoints(const QStringList& paths);

// The model's state as its file has it (paths from unity::jointPaths), then the prefab's overrides of it: the
// PrefabInstance of the model in the avatar's prefab.
ModelState prefabModelState(const unity::Package& package, const QString& prefabGuid, const QString& modelGuid,
                            const hfm::Model& model, const QStringList& paths, float metersPerUnit);

// A script component of the avatar (its spring bones, colliders, contacts, constraints), from its prefab or a prefab
// nested in it, with every outer prefab's overrides applied. Object references in its fields are canonical, the file
// that defines the object and its ID there: {"guid": g, "fileID": n} (stripped placeholders followed through).
struct PrefabComponent {
    QString className;          // the component's class name, from the script's ID; empty for others
    QVariantMap fields;
    QString guid;               // the file that defines it
    qint64 fileID { 0 };
    QVariantMap gameObject;     // its GameObject, canonical
};
QVector<PrefabComponent> prefabComponents(const unity::Package& package, const QString& prefabGuid);

// An object, followed through stripped placeholders (a prefab's stand-ins for objects of nested prefabs and models) to
// the file that defines it: {"guid": g, "fileID": n}.
QVariantMap canonicalObject(const unity::Package& package, const QString& guid, qint64 fileID);

// Each mesh's shape keys in Unity's order (a renderer's m_BlendShapeWeights index), by the mesh's node name: the
// FBX's blend shape channels as their deformers list them.
QHash<QString, QStringList> shapeKeysByMesh(const QByteArray& fbx);

}

#endif // hifi_PrefabOverrides_h
