//
//  UnitySpringBones.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "UnitySpringBones.h"

#include <algorithm>

#include <QJsonArray>

#include "UnityTransforms.h"
#include "UnityYaml.h"

namespace avatarimport {

namespace {

double number(const QVariantMap& fields, const char* key, double fallback = 0.0) {
    return unity::toNumber(fields.value(key), fallback);
}

QJsonArray vector3(const QVariant& value, bool flipX) {  // Unity's {x, y, z} -> [x, y, z], X negated into the engine's axes
    const QVariantMap v = value.toMap();
    const double x = unity::toNumber(v.value("x"));
    return QJsonArray { flipX ? -x : x, unity::toNumber(v.value("y")), unity::toNumber(v.value("z")) };
}

QJsonArray quaternion(const glm::quat& q) {
    return QJsonArray { (double)q.x, (double)q.y, (double)q.z, (double)q.w };
}

QJsonArray curve(const QVariant& value) {
    QJsonArray keys;
    for (const QVariant& entry : value.toMap().value("m_Curve").toList()) {
        const QVariantMap k = entry.toMap();
        keys.append(QJsonArray { unity::toNumber(k.value("time")), unity::toNumber(k.value("value")),
                                 unity::toNumber(k.value("inSlope")), unity::toNumber(k.value("outSlope")),
                                 unity::toNumber(k.value("inWeight"), 1.0 / 3.0), unity::toNumber(k.value("outWeight"), 1.0 / 3.0),
                                 unity::toNumber(k.value("weightedMode")) });
    }
    return keys;
}

QString key(const QVariant& canonical) {
    const QVariantMap map = canonical.toMap();
    return map.value("guid").toString() + ":" + map.value("fileID").toString();
}

}  // namespace

SpringBoneData springBoneData(const QVector<PrefabComponent>& components, const QString& modelGuid, const hfm::Model& model,
                          const QStringList& paths, const QHash<QString, QString>& skeletonNames, bool sortChildrenByName) {
    SpringBoneData data;
    const QHash<qint64, int> modelJoints = modelObjectJoints(paths);
    auto jointOf = [&](const QVariant& canonical) {
        const QVariantMap map = canonical.toMap();
        if (map.value("guid").toString() != modelGuid) {
            return -2;
        }
        return modelJoints.value(map.value("fileID").toLongLong(), -2);
    };
    auto nameOf = [&](int joint) {
        const QString name = model.joints[joint].name;
        return skeletonNames.value(name, name);
    };
    // a component's root: its rootTransform, or its own GameObject
    auto rootOf = [&](const PrefabComponent& component) {
        const QVariant root = component.fields.value("rootTransform");
        return root.toMap().value("fileID").toLongLong() != 0 ? jointOf(root) : jointOf(component.gameObject);
    };
    QVector<QVector<int>> children(model.joints.size());
    for (int i = 0; i < (int)model.joints.size(); i++) {
        const int parent = model.joints[i].parentIndex;
        if (parent >= 0 && parent < (int)model.joints.size()) {
            children[parent].append(i);
        }
    }
    if (sortChildrenByName) {
        for (auto& list : children) {
            std::sort(list.begin(), list.end(), [&](int a, int b) { return model.joints[a].name < model.joints[b].name; });
        }
    }

    // colliders first, so the bones can name them by index
    QJsonArray colliders;
    QHash<QString, int> colliderIndex;
    int unplaced = 0;
    for (const PrefabComponent& component : components) {
        if (component.className != "VRCPhysBoneCollider") {
            continue;
        }
        const int joint = rootOf(component);
        if (joint < 0) {
            unplaced++;
            continue;
        }
        const QVariantMap& f = component.fields;
        const QVariantMap r = f.value("rotation").toMap();
        const glm::quat rotation = unity::rotationFromUnity(glm::normalize(glm::quat(
            (float)unity::toNumber(r.value("w"), 1.0), (float)unity::toNumber(r.value("x")),
            (float)unity::toNumber(r.value("y")), (float)unity::toNumber(r.value("z")))));
        colliderIndex.insert(key(QVariantMap { { "guid", component.guid }, { "fileID", QString::number(component.fileID) } }),
                             colliders.size());
        colliders.append(QJsonObject {
            { "joint", nameOf(joint) },
            { "shapeType", number(f, "shapeType") },
            { "radius", number(f, "radius") },
            { "height", number(f, "height") },
            { "position", vector3(f.value("position"), true) },
            { "rotation", quaternion(rotation) },
            { "insideBounds", number(f, "insideBounds") != 0.0 },
            { "bonesAsSpheres", number(f, "bonesAsSpheres") != 0.0 },
        });
    }

    QJsonArray bones;
    static const char* NUMBERS[] = {
        "version", "integrationType", "multiChildType", "pull", "spring", "stiffness", "gravity", "gravityFalloff",
        "immobileType", "immobile", "allowCollision", "radius", "limitType", "maxAngleX", "maxAngleZ", "allowGrabbing",
        "allowPosing", "grabMovement", "snapToHand", "maxStretch", "maxSquish", "stretchMotion", "isAnimated",
        "resetWhenDisabled", "ignoreOtherPhysBones",
    };
    for (const PrefabComponent& component : components) {
        if (component.className != "VRCPhysBone") {
            continue;
        }
        const QVariantMap& f = component.fields;
        const int root = rootOf(component);
        if (root < 0) {
            data.notes << "a spring bone whose root is not one of the model's bones was left out";
            continue;
        }
        QSet<int> ignored;
        QJsonArray ignore;
        for (const QVariant& entry : f.value("ignoreTransforms").toList()) {
            const int joint = jointOf(entry);
            if (joint >= 0) {
                ignored.insert(joint);
                ignore.append(nameOf(joint));
            }
        }
        QJsonArray joints;
        QVector<int> stack { root };
        while (!stack.isEmpty()) {
            const int joint = stack.takeLast();
            if (ignored.contains(joint)) {
                continue;
            }
            joints.append(nameOf(joint));
            for (int i = children[joint].size() - 1; i >= 0; i--) {
                stack.append(children[joint][i]);
            }
        }
        QJsonObject bone {
            { "root", nameOf(root) },
            { "ignore", ignore },
            { "joints", joints },
            { "endpoint", vector3(f.value("endpointPosition"), true) },
            { "parameter", f.value("parameter").toString() },
        };
        for (const char* field : NUMBERS) {
            bone.insert(field, number(f, field));
        }
        const QVariantMap limit = f.value("limitRotation").toMap();
        const glm::vec3 limitDegrees((float)unity::toNumber(limit.value("x")), (float)unity::toNumber(limit.value("y")),
                                     (float)unity::toNumber(limit.value("z")));
        bone.insert("limitRotation", QJsonArray { limitDegrees.x, limitDegrees.y, limitDegrees.z });  // Unity's Euler degrees
        bone.insert("limitRotationQuat", quaternion(unity::rotationFromUnityEuler(limitDegrees)));
        for (const char* filter : { "collisionFilter", "grabFilter", "poseFilter" }) {
            const QVariantMap map = f.value(filter).toMap();
            bone.insert(filter, QJsonObject { { "allowSelf", unity::toNumber(map.value("allowSelf"), 1.0) != 0.0 },
                                              { "allowOthers", unity::toNumber(map.value("allowOthers"), 1.0) != 0.0 } });
        }
        QJsonArray boneColliders;
        for (const QVariant& entry : f.value("colliders").toList()) {
            auto found = colliderIndex.find(key(entry));
            if (found != colliderIndex.end()) {
                boneColliders.append(found.value());
            }
        }
        bone.insert("colliders", boneColliders);
        QJsonObject curves;
        for (auto it = f.begin(); it != f.end(); ++it) {
            if (it.key().endsWith("Curve")) {
                const QJsonArray keys = curve(it.value());
                if (!keys.isEmpty()) {
                    curves.insert(it.key().left(it.key().size() - 5), keys);
                }
            }
        }
        bone.insert("curves", curves);
        bones.append(bone);
        data.joints += joints.size();
    }

    QJsonArray contacts;
    for (const PrefabComponent& component : components) {
        if (component.className != "VRCContactReceiver" && component.className != "VRCContactSender") {
            continue;
        }
        const QVariantMap& f = component.fields;
        const int joint = rootOf(component);
        if (joint < 0) {
            continue;
        }
        const QVariantMap r = f.value("rotation").toMap();
        const glm::quat rotation = unity::rotationFromUnity(glm::normalize(glm::quat(
            (float)unity::toNumber(r.value("w"), 1.0), (float)unity::toNumber(r.value("x")),
            (float)unity::toNumber(r.value("y")), (float)unity::toNumber(r.value("z")))));
        QJsonArray tags;
        for (const QVariant& tag : f.value("collisionTags").toList()) {
            tags.append(tag.toString());
        }
        contacts.append(QJsonObject {
            { "type", component.className == "VRCContactReceiver" ? "receiver" : "sender" },
            { "joint", nameOf(joint) },
            { "shapeType", number(f, "shapeType") },
            { "radius", number(f, "radius") },
            { "height", number(f, "height") },
            { "position", vector3(f.value("position"), true) },
            { "rotation", quaternion(rotation) },
            { "collisionTags", tags },
            { "parameter", f.value("parameter").toString() },
            { "receiverType", number(f, "receiverType") },
            { "allowSelf", number(f, "allowSelf", 1.0) != 0.0 },
            { "allowOthers", number(f, "allowOthers", 1.0) != 0.0 },
            { "localOnly", number(f, "localOnly") != 0.0 },
            { "minVelocity", number(f, "minVelocity") },
        });
    }
    if (unplaced > 0) {
        data.notes << QString("%1 spring-bone colliders on objects that are not the model's bones were left out").arg(unplaced);
    }
    data.bones = bones.size();
    data.colliders = colliders.size();
    data.contacts = contacts.size();
    data.json = QJsonObject { { "version", 1 }, { "bones", bones }, { "colliders", colliders }, { "contacts", contacts } };
    return data;
}

}  // namespace avatarimport
