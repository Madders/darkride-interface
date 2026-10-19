//
//  UnitySpringBones.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_UnitySpringBones_h
#define hifi_UnitySpringBones_h

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <hfm/HFM.h>

#include "PrefabOverrides.h"

namespace avatarimport {

// An avatar's spring bones, their colliders and its contacts, as data for the engine: the components'
// own fields and values, with Unity's side translated. The .fst line is "springBones = <this object as JSON>":
//   {"version": 1, "bones": [...], "colliders": [...], "contacts": [...]}
// - bones: one per VRCPhysBone: "root" and "ignore" as joint names (after jointMap), "joints" every joint it moves in
//   Unity's order (depth first, children by name when the importer sorts them so), the forces, limits, collision,
//   stretch, grab and options fields under the components' names, "colliders" as indices into "colliders", "endpoint" in the
//   tip's frame, "limitRotation" also as a quaternion ("limitRotationQuat"), and "curves" as Unity keyframes [time,
//   value, inSlope, outSlope, inWeight, outWeight, weightedMode] by field ("pull", "radius", ...).
// - colliders: "joint", "shapeType" (0 sphere, 1 capsule, 2 plane), "radius", "height", "position" and "rotation"
//   (x, y, z, w) in the joint's frame, "insideBounds", "bonesAsSpheres".
// - contacts: VRCContactReceiver and VRCContactSender, "joint", shape, "position", "rotation", tags and parameter.
// Positions are in meters in the joint's frame with the engine's axes (Unity's X negated), rotations likewise
// (x, -y, -z, w); lengths in meters, as the components give them.
struct SpringBoneData {
    QJsonObject json;
    int bones { 0 };
    int joints { 0 };
    int colliders { 0 };
    int contacts { 0 };
    QStringList notes;
};

SpringBoneData springBoneData(const QVector<PrefabComponent>& components, const QString& modelGuid, const hfm::Model& model,
                          const QStringList& paths, const QHash<QString, QString>& skeletonNames, bool sortChildrenByName);

}

#endif // hifi_UnitySpringBones_h
