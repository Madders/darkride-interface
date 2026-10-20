//
//  PrefabOverrides.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "PrefabOverrides.h"

#include <QBuffer>
#include <QSet>
#include <QRegularExpression>

#include <FBX.h>
#include <FBXSerializer.h>

#include "UnityIds.h"
#include "UnityPackage.h"
#include "UnityTransforms.h"
#include "UnityYaml.h"

namespace avatarimport {

namespace {

void setComponent(glm::vec3& v, const QString& axis, float value) {
    if (axis == "x") {
        v.x = value;
    } else if (axis == "y") {
        v.y = value;
    } else if (axis == "z") {
        v.z = value;
    }
}

void setComponent(glm::quat& q, const QString& axis, float value) {
    if (axis == "x") {
        q.x = value;
    } else if (axis == "y") {
        q.y = value;
    } else if (axis == "z") {
        q.z = value;
    } else if (axis == "w") {
        q.w = value;
    }
}

int arrayIndex(const QString& property) {  // "m_Materials.Array.data[1]" -> 1
    static const QRegularExpression INDEX("\\.Array\\.data\\[(\\d+)\\]$");
    const auto match = INDEX.match(property);
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

}  // namespace

QHash<qint64, int> modelObjectJoints(const QStringList& paths) {
    QHash<qint64, int> joints;
    auto add = [&](int joint, const QString& nodePath) {
        joints.insert(unity::modelGameObjectID(nodePath), joint);
        for (const char* className : { "Transform", "SkinnedMeshRenderer", "MeshRenderer", "MeshFilter" }) {
            joints.insert(unity::modelComponentID(className, nodePath), joint);
        }
    };
    add(-1, unity::modelNodePath({}));
    for (int i = 0; i < paths.size(); i++) {
        add(i, unity::modelNodePath(paths[i].split('/')));
    }
    return joints;
}

ModelState prefabModelState(const unity::Package& package, const QString& prefabGuid, const QString& modelGuid,
                            const hfm::Model& model, const QStringList& paths, float metersPerUnit) {
    ModelState state;
    state.poses.resize((int)model.joints.size());
    for (int i = 0; i < (int)model.joints.size(); i++) {
        glm::vec3 translation, scale;
        glm::quat rotation;
        unity::jointLocalPose(model.joints[i], translation, rotation, scale);
        // the engine's frame back to Unity's: the same X flip, and meters
        state.poses[i].position = glm::vec3(-translation.x, translation.y, translation.z) * metersPerUnit;
        state.poses[i].rotation = glm::quat(rotation.w, rotation.x, -rotation.y, -rotation.z);
        state.poses[i].scale = scale;
    }

    // The model's objects by their IDs: the root, and each joint's GameObject, Transform and renderers.
    struct Target {
        int joint;
        QString className;
    };
    QHash<qint64, Target> targets;
    auto add = [&](int joint, const QString& nodePath) {
        targets.insert(unity::modelGameObjectID(nodePath), { joint, "GameObject" });
        for (const char* className : { "Transform", "SkinnedMeshRenderer", "MeshRenderer", "MeshFilter" }) {
            targets.insert(unity::modelComponentID(className, nodePath), { joint, className });
        }
    };
    add(-1, unity::modelNodePath({}));
    for (int i = 0; i < paths.size(); i++) {
        add(i, unity::modelNodePath(paths[i].split('/')));
    }

    for (const auto& object : package.objects(prefabGuid)) {
        if (object.classID != 1001 || unity::guidOf(object.fields.value("m_SourcePrefab")) != modelGuid) {
            continue;
        }
        const QVariantMap modification = object.fields.value("m_Modification").toMap();
        for (const QVariant& entry : modification.value("m_Modifications").toList()) {
            const QVariantMap m = entry.toMap();
            const QVariant target = m.value("target");
            const QString property = m.value("propertyPath").toString();
            if (unity::guidOf(target) != modelGuid || property.startsWith("m_AABB") ||
                property.startsWith("m_ProbeAnchor") || property.startsWith("m_LocalEulerAnglesHint") ||
                property == "m_Name") {
                continue;  // bounds, light-probe anchors and editor hints change nothing anyone sees
            }
            auto found = targets.find(unity::fileIDOf(target));
            if (found == targets.end()) {
                state.stale++;
                continue;
            }
            const int joint = found->joint;
            const float value = (float)unity::toNumber(m.value("value"));
            ModelState::Pose& pose = joint < 0 ? state.root : state.poses[joint];
            if (property.startsWith("m_LocalPosition.")) {
                setComponent(pose.position, property.mid(16), value);
                pose.changed = true;
            } else if (property.startsWith("m_LocalRotation.")) {
                setComponent(pose.rotation, property.mid(16), value);
                pose.changed = true;
            } else if (property.startsWith("m_LocalScale.")) {
                setComponent(pose.scale, property.mid(13), value);
                pose.changed = true;
            } else if (joint < 0) {
                continue;
            } else if (property == "m_IsActive") {
                state.active[joint] = value != 0.0f;
            } else if (property == "m_Enabled" && found->className.endsWith("Renderer")) {
                state.drawn[joint] = value != 0.0f;
            } else if (property.startsWith("m_BlendShapeWeights.Array.data[")) {
                state.shapeWeights[joint][arrayIndex(property)] = value;
            } else if (property.startsWith("m_Materials.Array.data[")) {
                state.materials[joint][arrayIndex(property)] = unity::guidOf(m.value("objectReference"));
            }
        }
    }
    for (auto& pose : state.poses) {
        pose.rotation = glm::normalize(pose.rotation);
    }
    state.root.rotation = glm::normalize(state.root.rotation);
    return state;
}

QVariantMap canonicalObject(const unity::Package& package, const QString& guid, qint64 fileID) {
    QString g = guid;
    qint64 f = fileID;
    for (int depth = 0; depth < 16 && f != 0; depth++) {
        const unity::YamlObject* found = nullptr;
        for (const auto& object : package.objects(g)) {
            if (object.fileID == f) {
                found = &object;
                break;
            }
        }
        if (!found || !found->stripped) {
            break;
        }
        const QVariant source = found->fields.value("m_CorrespondingSourceObject");
        if (unity::guidOf(source).isEmpty() || unity::fileIDOf(source) == 0) {
            break;
        }
        g = unity::guidOf(source);
        f = unity::fileIDOf(source);
    }
    return QVariantMap { { "guid", g }, { "fileID", QString::number(f) } };
}

namespace {

bool isReference(const QVariantMap& map) {
    return map.contains("fileID") && map.size() <= 3 && (map.size() == 1 || map.contains("guid") || map.contains("type"));
}

// Object references written in file `guid` become canonical; references to other assets (a material, a mesh) stay.
QVariant canonicalFields(const unity::Package& package, const QString& guid, const QVariant& value) {
    if (value.type() == QVariant::Map) {
        const QVariantMap map = value.toMap();
        if (isReference(map)) {
            const QString target = unity::guidOf(map);
            const qint64 fileID = unity::fileIDOf(map);
            if (fileID != 0 && (target.isEmpty() || target == guid)) {
                return canonicalObject(package, guid, fileID);
            }
            return map;
        }
        QVariantMap out;
        for (auto it = map.begin(); it != map.end(); ++it) {
            out.insert(it.key(), canonicalFields(package, guid, it.value()));
        }
        return out;
    }
    if (value.type() == QVariant::List) {
        QVariantList out;
        for (const QVariant& item : value.toList()) {
            out.append(canonicalFields(package, guid, item));
        }
        return out;
    }
    return value;
}

// Applies one override (propertyPath "a.b.Array.data[2].c", "a.Array.size") to a field tree.
QVariant setPath(const QVariant& node, const QStringList& parts, int i, const QVariant& value) {
    if (i >= parts.size()) {
        return value;
    }
    const QString& part = parts[i];
    if (part == "Array" && i + 1 < parts.size()) {
        QVariantList list = node.toList();
        const QString& next = parts[i + 1];
        if (next == "size") {
            const int size = (int)unity::toNumber(value);
            while (list.size() < size) {
                list.append(QVariant());
            }
            while (list.size() > size && size >= 0) {
                list.removeLast();
            }
            return list;
        }
        if (next.startsWith("data[") && next.endsWith("]")) {
            const int index = next.mid(5, next.size() - 6).toInt();
            while (list.size() <= index) {
                list.append(QVariant());
            }
            list[index] = setPath(list[index], parts, i + 2, value);
            return list;
        }
        return list;
    }
    QVariantMap map = node.toMap();
    map[part] = setPath(map.value(part), parts, i + 1, value);
    return map;
}

QString objectKey(const QVariantMap& canonical) {
    return canonical.value("guid").toString() + ":" + canonical.value("fileID").toString();
}

QVector<PrefabComponent> gatherComponents(const unity::Package& package, const QString& guid, int depth) {
    QVector<PrefabComponent> components;
    if (depth > 8) {
        return components;
    }
    for (const auto& object : package.objects(guid)) {
        if (object.classID != 114 || object.stripped) {
            continue;
        }
        PrefabComponent component;
        component.className = unity::componentClassName(unity::fileIDOf(object.fields.value("m_Script")));
        component.fields = canonicalFields(package, guid, object.fields).toMap();
        component.guid = guid;
        component.fileID = object.fileID;
        component.gameObject = canonicalObject(package, guid, unity::fileIDOf(object.fields.value("m_GameObject")));
        components.append(component);
    }
    for (const auto& object : package.objects(guid)) {
        if (object.classID != 1001) {
            continue;
        }
        const QString source = unity::guidOf(object.fields.value("m_SourcePrefab"));
        const auto* sourceAsset = package.byGuid(source);
        if (!sourceAsset || !sourceAsset->path.endsWith(".prefab", Qt::CaseInsensitive)) {
            continue;  // a model, whose overrides prefabModelState reads
        }
        QVector<PrefabComponent> inner = gatherComponents(package, source, depth + 1);
        const QVariantMap modification = object.fields.value("m_Modification").toMap();
        QSet<QString> removed;
        for (const char* key : { "m_RemovedComponents", "m_RemovedGameObjects" }) {
            for (const QVariant& entry : modification.value(key).toList()) {
                removed.insert(objectKey(canonicalObject(package, unity::guidOf(entry), unity::fileIDOf(entry))));
            }
        }
        QHash<QString, QVector<QVariantMap>> byTarget;
        for (const QVariant& entry : modification.value("m_Modifications").toList()) {
            const QVariantMap m = entry.toMap();
            const QVariant target = m.value("target");
            byTarget[objectKey(canonicalObject(package, unity::guidOf(target), unity::fileIDOf(target)))].append(m);
        }
        for (PrefabComponent& component : inner) {
            const QString key = objectKey(QVariantMap { { "guid", component.guid }, { "fileID", QString::number(component.fileID) } });
            if (removed.contains(key) || removed.contains(objectKey(component.gameObject))) {
                continue;
            }
            QVariant fields = component.fields;
            for (const QVariantMap& m : byTarget.value(key)) {
                const QVariantMap reference = m.value("objectReference").toMap();
                QVariant value = m.value("value");
                if (unity::fileIDOf(reference) != 0) {
                    value = canonicalFields(package, guid, reference);
                }
                fields = setPath(fields, m.value("propertyPath").toString().split('.'), 0, value);
            }
            component.fields = fields.toMap();
            components.append(component);
        }
    }
    return components;
}

}  // namespace

QVector<PrefabComponent> prefabComponents(const unity::Package& package, const QString& prefabGuid) {
    return gatherComponents(package, prefabGuid, 0);
}

QHash<QString, QStringList> shapeKeysByMesh(const QByteArray& fbx) {
    QBuffer buffer(const_cast<QByteArray*>(&fbx));
    buffer.open(QIODevice::ReadOnly);
    FBXNode root;
    try {
        root = FBXSerializer::parseFBX(&buffer);
    } catch (const QString&) {
        return {};
    }
    QHash<QString, QString> kindOf, nameOf, typeOf;  // object ID -> subclass ("Mesh", "BlendShape"), name, node name
    QHash<QString, QStringList> children;           // parent ID -> child IDs, in the file's connection order
    for (const FBXNode& section : root.children) {
        if (section.name == "Objects") {
            for (const FBXNode& object : section.children) {
                if (object.properties.size() < 3) {
                    continue;
                }
                const QString id = object.properties.at(0).toString();
                const QByteArray name = object.properties.at(1).toByteArray();
                nameOf.insert(id, QString::fromUtf8(name.left(name.indexOf('\0'))));
                kindOf.insert(id, object.properties.at(2).toString());
                typeOf.insert(id, QString::fromUtf8(object.name));
            }
        } else if (section.name == "Connections") {
            for (const FBXNode& connection : section.children) {
                if ((connection.name == "C" || connection.name == "Connect") && connection.properties.size() >= 3 &&
                    connection.properties.at(0).toString() == "OO") {
                    children[connection.properties.at(2).toString()].append(connection.properties.at(1).toString());
                }
            }
        }
    }
    QHash<QString, QStringList> shapeKeys;
    for (auto it = typeOf.begin(); it != typeOf.end(); ++it) {
        if (it.value() != "Model" || kindOf.value(it.key()) != "Mesh") {
            continue;
        }
        QStringList names;
        for (const QString& geometry : children.value(it.key())) {
            if (typeOf.value(geometry) != "Geometry") {
                continue;
            }
            for (const QString& deformer : children.value(geometry)) {
                if (kindOf.value(deformer) != "BlendShape") {
                    continue;
                }
                for (const QString& channel : children.value(deformer)) {
                    if (kindOf.value(channel) == "BlendShapeChannel") {
                        names.append(nameOf.value(channel));
                    }
                }
            }
        }
        if (!names.isEmpty()) {
            shapeKeys.insert(nameOf.value(it.key()), names);
        }
    }
    return shapeKeys;
}

}  // namespace avatarimport
