//
//  UnityIds.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "UnityIds.h"

#include <QCryptographicHash>
#include <QHash>
#include <QStringList>
#include <QVector>
#include <QtEndian>

namespace unity {

qint32 dllScriptFileID(const QString& nameSpace, const QString& className) {
    QByteArray input("s\0\0\0", 4);
    input += (nameSpace + className).toUtf8();
    const QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Md4);
    return qFromLittleEndian<qint32>(reinterpret_cast<const uchar*>(digest.constData()));
}

QString componentClassName(qint64 scriptFileID) {
    static const QHash<qint64, QString> TABLE = [] {
        const QVector<QPair<QString, QStringList>> classes {
            { "VRC.SDK3.Avatars.Components",
              { "VRCAvatarDescriptor", "VRCAnimatorLayerControl", "VRCAnimatorLocomotionControl",
                "VRCAnimatorTemporaryPoseSpace", "VRCAnimatorTrackingControl", "VRCAvatarParameterDriver",
                "VRCPlayableLayerControl", "VRCHeadChop", "VRCImpostorSettings", "VRCImpostorEnvironment",
                "VRCSpatialAudioSource", "VRCStation", "VRCPerPlatformOverrides" } },
            { "VRC.SDK3.Avatars.ScriptableObjects", { "VRCExpressionParameters", "VRCExpressionsMenu" } },
            { "VRC.SDK3.Dynamics.PhysBone.Components", { "VRCPhysBone", "VRCPhysBoneCollider", "VRCPhysBoneRoot" } },
            { "VRC.SDK3.Dynamics.Contact.Components", { "VRCContactSender", "VRCContactReceiver" } },
            { "VRC.SDK3.Dynamics.Constraint.Components",
              { "VRCParentConstraint", "VRCPositionConstraint", "VRCRotationConstraint", "VRCScaleConstraint",
                "VRCAimConstraint", "VRCLookAtConstraint" } },
            { "VRC.Core", { "PipelineManager" } },
        };
        QHash<qint64, QString> table;
        for (const auto& group : classes) {
            for (const QString& name : group.second) {
                table.insert(dllScriptFileID(group.first, name), name);
            }
        }
        return table;
    }();
    return TABLE.value(scriptFileID);
}

QString classIDName(int classID) {
    static const QHash<int, QString> NAMES {
        { 1, "GameObject" }, { 4, "Transform" }, { 20, "Camera" }, { 21, "Material" }, { 23, "MeshRenderer" },
        { 28, "Texture2D" }, { 33, "MeshFilter" }, { 43, "Mesh" }, { 48, "Shader" }, { 54, "Rigidbody" },
        { 65, "BoxCollider" }, { 74, "AnimationClip" }, { 82, "AudioSource" }, { 90, "Avatar" },
        { 91, "AnimatorController" }, { 95, "Animator" }, { 96, "TrailRenderer" }, { 108, "Light" },
        { 114, "MonoBehaviour" }, { 115, "MonoScript" }, { 120, "LineRenderer" }, { 135, "SphereCollider" },
        { 136, "CapsuleCollider" }, { 137, "SkinnedMeshRenderer" }, { 183, "Cloth" }, { 198, "ParticleSystem" },
        { 199, "ParticleSystemRenderer" }, { 206, "BlendTree" }, { 1001, "PrefabInstance" },
        { 1101, "AnimatorStateTransition" }, { 1102, "AnimatorState" }, { 1107, "AnimatorStateMachine" },
        { 1109, "AnimatorTransition" }, { 895512359, "AimConstraint" }, { 1183024399, "LookAtConstraint" },
        { 1773428102, "ParentConstraint" }, { 1818360608, "PositionConstraint" },
        { 1818360609, "RotationConstraint" }, { 1818360610, "ScaleConstraint" },
    };
    return NAMES.value(classID, QString::number(classID));
}

quint64 xxHash64(const QByteArray& data, quint64 seed) {
    const quint64 P1 = 11400714785074694791ULL, P2 = 14029467366897019727ULL, P3 = 1609587929392839161ULL,
                  P4 = 9650029242287828579ULL, P5 = 2870177450012600261ULL;
    auto rotl = [](quint64 x, int r) { return (x << r) | (x >> (64 - r)); };
    auto round = [&](quint64 acc, quint64 input) { return rotl(acc + input * P2, 31) * P1; };
    auto merge = [&](quint64 acc, quint64 value) { return (acc ^ round(0, value)) * P1 + P4; };
    const uchar* p = reinterpret_cast<const uchar*>(data.constData());
    const uchar* const end = p + data.size();
    quint64 h;
    if (data.size() >= 32) {
        quint64 v1 = seed + P1 + P2, v2 = seed + P2, v3 = seed, v4 = seed - P1;
        for (; p + 32 <= end; p += 32) {
            v1 = round(v1, qFromLittleEndian<quint64>(p));
            v2 = round(v2, qFromLittleEndian<quint64>(p + 8));
            v3 = round(v3, qFromLittleEndian<quint64>(p + 16));
            v4 = round(v4, qFromLittleEndian<quint64>(p + 24));
        }
        h = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
        h = merge(merge(merge(merge(h, v1), v2), v3), v4);
    } else {
        h = seed + P5;
    }
    h += (quint64)data.size();
    for (; p + 8 <= end; p += 8) {
        h = rotl(h ^ round(0, qFromLittleEndian<quint64>(p)), 27) * P1 + P4;
    }
    if (p + 4 <= end) {
        h = rotl(h ^ (quint64)qFromLittleEndian<quint32>(p) * P1, 23) * P2 + P3;
        p += 4;
    }
    for (; p < end; p++) {
        h = rotl(h ^ (quint64)*p * P5, 11) * P1;
    }
    h ^= h >> 33;
    h *= P2;
    h ^= h >> 29;
    h *= P3;
    h ^= h >> 32;
    return h;
}

qint64 modelObjectID(const QString& className, const QString& path, int n) {
    return (qint64)xxHash64(("Type:" + className + "->" + path + QString::number(n)).toUtf8());
}

QString modelNodePath(const QStringList& namesBelowRoot) {
    QString path = "//RootNode/root";
    for (const QString& name : namesBelowRoot) {
        path += "/" + name;
    }
    return path;
}

qint64 modelGameObjectID(const QString& nodePath, int n) {
    return modelObjectID("GameObject", nodePath, n);
}

qint64 modelComponentID(const QString& className, const QString& nodePath, int n) {
    return modelObjectID(className, nodePath + "/" + className, n);
}

qint64 modelMeshID(const QString& meshName, int n) {
    return modelObjectID("Mesh", meshName, n);
}

}  // namespace unity
