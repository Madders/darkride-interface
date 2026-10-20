//
//  UnityHumanoid.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "UnityHumanoid.h"

#include <cmath>
#include <functional>

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Humanoid.h>

#include "UnityPackage.h"
#include "UnityYaml.h"

namespace avatarimport {

namespace {

using namespace humanoid;

glm::vec3 vec3Of(const QVariant& value, const glm::vec3& fallback = glm::vec3(0.0f)) {
    const QVariantMap v = value.toMap();
    if (v.isEmpty()) {
        return fallback;
    }
    return glm::vec3((float)unity::toNumber(v.value("x")), (float)unity::toNumber(v.value("y")),
                     (float)unity::toNumber(v.value("z")));
}

glm::quat quatOf(const QVariant& value) {
    const QVariantMap v = value.toMap();
    return glm::normalize(glm::quat((float)unity::toNumber(v.value("w"), 1.0), (float)unity::toNumber(v.value("x")),
                                    (float)unity::toNumber(v.value("y")), (float)unity::toNumber(v.value("z"))));
}

// Unity's coordinates mirror the engine's along X (as an FBX reads in each): a rotation (x, y, z, w) in one is
// (x, -y, -z, w) in the other.
glm::quat mirrored(const glm::quat& q) {
    return glm::quat(q.w, q.x, -q.y, -q.z);
}

QVariantList listOf(const glm::quat& q) {
    return { q.x, q.y, q.z, q.w };
}

QVariantList listOf(const glm::vec3& v) {
    return { v.x, v.y, v.z };
}

struct SkeletonBone {
    QString parent;
    glm::vec3 position { 0.0f };
    glm::quat rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale { 1.0f };
};

struct WorldPose {
    glm::quat rotation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 position { 0.0f };
    glm::vec3 scale { 1.0f };
};

bool isThumb(int bone) {
    return (bone >= LeftThumbProximal && bone <= LeftThumbDistal) || (bone >= RightThumbProximal && bone <= RightThumbDistal);
}

bool isFingerPhalanx(int bone) {
    return bone >= LeftThumbProximal && bone <= RightLittleDistal;
}

// The body direction a bone's muscle frame keeps its Z axis towards, in Unity's coordinates (+X right, +Y up, +Z
// forward): the avatar's left for the spine and head, its right for legs and feet, up for forearms and thumbs, right
// for the eyes, forward for shoulders, arms, hands and fingers.
glm::vec3 zTowards(int bone) {
    switch (bone) {
        case Hips: case Spine: case Chest: case UpperChest: case Neck: case Head: case Jaw:
            return glm::vec3(-1.0f, 0.0f, 0.0f);
        case LeftUpperLeg: case RightUpperLeg: case LeftLowerLeg: case RightLowerLeg: case LeftFoot: case RightFoot:
        case LeftToes: case RightToes: case LeftEye: case RightEye:
            return glm::vec3(1.0f, 0.0f, 0.0f);
        case LeftLowerArm: case RightLowerArm:
            return glm::vec3(0.0f, 1.0f, 0.0f);
        default:
            return isThumb(bone) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    }
}

// Unity's keyframe curve at a time: cubic Hermite between keys with their slopes; an infinite slope holds the key.
double evaluateCurve(const QVariantList& keys, double time) {
    if (keys.isEmpty()) {
        return 0.0;
    }
    auto key = [&](int i, const char* field, double fallback) {
        return unity::toNumber(keys[i].toMap().value(field), fallback);
    };
    if (time <= key(0, "time", 0.0) || keys.size() == 1) {
        return key(0, "value", 0.0);
    }
    const int last = keys.size() - 1;
    if (time >= key(last, "time", 0.0)) {
        return key(last, "value", 0.0);
    }
    int i = 0;
    while (i < last - 1 && key(i + 1, "time", 0.0) <= time) {
        i++;
    }
    const double t0 = key(i, "time", 0.0);
    const double t1 = key(i + 1, "time", 0.0);
    const double v0 = key(i, "value", 0.0);
    const double v1 = key(i + 1, "value", 0.0);
    const double m0 = key(i, "outSlope", NAN);
    const double m1 = key(i + 1, "inSlope", NAN);
    const double dt = t1 - t0;
    if (dt <= 0.0) {
        return v1;
    }
    if (!std::isfinite(m0) || !std::isfinite(m1)) {
        return v0;
    }
    const double s = (time - t0) / dt;
    const double s2 = s * s;
    const double s3 = s2 * s;
    return (2 * s3 - 3 * s2 + 1) * v0 + (s3 - 2 * s2 + s) * dt * m0 + (-2 * s3 + 3 * s2) * v1 + (s3 - s2) * dt * m1;
}

const unity::YamlObject* firstObject(const unity::Package& package, const QString& guid, int classID) {
    for (const auto& object : package.objects(guid)) {
        if (object.classID == classID && !object.stripped) {
            return &object;
        }
    }
    return nullptr;
}

}  // namespace

QVariantMap humanoidFrames(const QVariantMap& description, QStringList& notes) {
    // The T-pose: every transform of the skeleton the description was made from, in Unity's coordinates.
    QHash<QString, SkeletonBone> skeleton;
    for (const QVariant& entry : description.value("skeleton").toList()) {
        const QVariantMap bone = entry.toMap();
        SkeletonBone b;
        b.parent = bone.value("parentName").toString();
        b.position = vec3Of(bone.value("position"));
        b.rotation = quatOf(bone.value("rotation"));
        b.scale = vec3Of(bone.value("scale"), glm::vec3(1.0f));
        skeleton.insert(bone.value("name").toString(), b);
    }
    QHash<QString, WorldPose> worlds;
    std::function<WorldPose(const QString&, int)> world = [&](const QString& name, int depth) -> WorldPose {
        auto found = worlds.find(name);
        if (found != worlds.end()) {
            return *found;
        }
        WorldPose pose;
        auto bone = skeleton.find(name);
        if (bone != skeleton.end() && depth < 256) {
            WorldPose parent;
            if (!bone->parent.isEmpty() && bone->parent != name && skeleton.contains(bone->parent)) {
                parent = world(bone->parent, depth + 1);
            }
            pose.rotation = parent.rotation * bone->rotation;
            pose.position = parent.position + parent.rotation * (parent.scale * bone->position);
            pose.scale = parent.scale * bone->scale;
        }
        worlds.insert(name, pose);
        return pose;
    };

    QHash<int, QString> transformOf;
    QHash<int, QVariantMap> limitOf;
    for (const QVariant& entry : description.value("human").toList()) {
        const QVariantMap human = entry.toMap();
        const int bone = boneFromUnityName(human.value("humanName").toString());
        const QString transform = human.value("boneName").toString();
        if (bone >= 0 && skeleton.contains(transform)) {
            transformOf.insert(bone, transform);
            limitOf.insert(bone, human.value("limit").toMap());
        }
    }
    if (!transformOf.contains(Hips)) {
        notes << "the humanoid description has no hips: no muscle clips";
        return QVariantMap();
    }
    auto present = [&](int bone) { return transformOf.contains(bone); };
    auto position = [&](int bone) { return world(transformOf.value(bone), 0).position; };
    auto firstPresent = [&](std::initializer_list<int> bones) {
        for (int bone : bones) {
            if (present(bone)) {
                return bone;
            }
        }
        return -1;
    };

    // A bone's muscle frame runs its X axis to its child; the head points up, the feet down, the eyes forward, and a
    // bone with no child carries on in its parent's direction.
    std::function<glm::vec3(int)> along = [&](int bone) -> glm::vec3 {
        if (bone == LeftEye || bone == RightEye) {
            return glm::vec3(0.0f, 0.0f, 1.0f);
        }
        if (bone == Head) {
            return glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if (bone == LeftFoot || bone == RightFoot) {
            return glm::vec3(0.0f, -1.0f, 0.0f);
        }
        int child = -1;
        switch (bone) {
            case Hips: child = firstPresent({ Spine }); break;
            case Spine: child = firstPresent({ Chest, UpperChest, Neck, Head }); break;
            case Chest: child = firstPresent({ UpperChest, Neck, Head }); break;
            case UpperChest: child = firstPresent({ Neck, Head }); break;
            case Neck: child = firstPresent({ Head }); break;
            case LeftUpperLeg: child = firstPresent({ LeftLowerLeg }); break;
            case RightUpperLeg: child = firstPresent({ RightLowerLeg }); break;
            case LeftLowerLeg: child = firstPresent({ LeftFoot }); break;
            case RightLowerLeg: child = firstPresent({ RightFoot }); break;
            case LeftShoulder: child = firstPresent({ LeftUpperArm }); break;
            case RightShoulder: child = firstPresent({ RightUpperArm }); break;
            case LeftUpperArm: child = firstPresent({ LeftLowerArm }); break;
            case RightUpperArm: child = firstPresent({ RightLowerArm }); break;
            case LeftLowerArm: child = firstPresent({ LeftHand }); break;
            case RightLowerArm: child = firstPresent({ RightHand }); break;
            default:
                if (isFingerPhalanx(bone) && (bone - LeftThumbProximal) % 3 < 2) {
                    child = firstPresent({ bone + 1 });
                }
                break;
        }
        if (child >= 0) {
            const glm::vec3 line = position(child) - position(bone);
            if (glm::length(line) > 1.0e-6f) {
                return glm::normalize(line);
            }
        }
        const int parent = boneParent(bone);
        return parent >= 0 ? along(parent) : glm::vec3(0.0f, 1.0f, 0.0f);
    };

    QVariantMap bones;
    for (auto it = transformOf.begin(); it != transformOf.end(); ++it) {
        const int bone = it.key();
        const glm::vec3 x = along(bone);
        glm::vec3 z = zTowards(bone);
        z -= glm::dot(z, x) * x;
        if (glm::length(z) < 1.0e-6f) {
            notes << "no muscle frame for " + unityName(bone) + " (it runs along its body direction)";
            continue;
        }
        z = glm::normalize(z);
        const glm::vec3 y = glm::cross(z, x);
        const glm::quat post = glm::inverse(world(it.value(), 0).rotation) * glm::quat_cast(glm::mat3(x, y, z));

        // Limits: the avatar's own where its description changed them, Unity's defaults otherwise.
        const QVariantMap limit = limitOf.value(bone);
        const bool custom = unity::toNumber(limit.value("modified")) != 0.0;
        const glm::vec3 customMin = vec3Of(limit.value("min"));
        const glm::vec3 customMax = vec3Of(limit.value("max"));
        glm::vec3 minimum(0.0f), maximum(0.0f), tpose(0.0f);
        const glm::vec3 sign = boneLimitSign(bone);
        for (int axis = 0; axis < 3; axis++) {
            const int muscle = boneMuscle(bone, axis);
            if (muscle < 0) {
                continue;
            }
            minimum[axis] = custom ? customMin[axis] : muscleDefaultMin(muscle);
            maximum[axis] = custom ? customMax[axis] : muscleDefaultMax(muscle);
            const float value = tposeMuscle(muscle);
            tpose[axis] = value * (value >= 0.0f ? maximum[axis] : -minimum[axis]) * sign[axis];
        }
        const glm::quat pre = skeleton.value(it.value()).rotation * post * glm::inverse(muscleRotation(tpose));

        QVariantMap frame;
        frame["pre"] = listOf(mirrored(pre));
        frame["post"] = listOf(mirrored(post));
        frame["sign"] = listOf(glm::vec3(sign.x, -sign.y, -sign.z));
        frame["min"] = listOf(minimum);
        frame["max"] = listOf(maximum);
        bones.insert(standardName(bone), frame);
    }
    QVariantMap frames;
    frames["version"] = 1;
    frames["twist"] = QVariantList { unity::toNumber(description.value("armTwist"), 0.5),
                                     unity::toNumber(description.value("foreArmTwist"), 0.5),
                                     unity::toNumber(description.value("upperLegTwist"), 0.5),
                                     unity::toNumber(description.value("legTwist"), 0.5) };
    frames["bones"] = bones;
    return frames;
}

bool writeMuscleClip(const unity::Package& package, const QString& clipGuid, const QString& path, QString* error,
                     int* frameCountOut, double* frameRateOut) {
    const unity::YamlObject* clip = firstObject(package, clipGuid, 74);
    if (!clip) {
        return false;
    }
    struct Curve {
        int muscle;
        QVariantList keys;
    };
    std::vector<Curve> curves;
    for (const QVariant& entry : clip->fields.value("m_FloatCurves").toList()) {
        const QVariantMap curve = entry.toMap();
        if ((int)unity::toNumber(curve.value("classID")) != 95) {
            continue;
        }
        const int muscle = muscleFromClipAttribute(curve.value("attribute").toString());
        if (muscle >= 0) {
            curves.push_back({ muscle, curve.value("curve").toMap().value("m_Curve").toList() });
        }
    }
    if (curves.empty()) {
        return false;
    }
    const double rate = std::max(1.0, unity::toNumber(clip->fields.value("m_SampleRate"), 60.0));
    const QVariantMap settings = clip->fields.value("m_AnimationClipSettings").toMap();
    const double start = unity::toNumber(settings.value("m_StartTime"), 0.0);
    const double stop = unity::toNumber(settings.value("m_StopTime"), 0.0);
    const int frameCount = std::max(1, (int)std::lround((stop - start) * rate) + 1);
    QJsonArray muscles;
    for (const Curve& curve : curves) {
        muscles.append(curve.muscle);
    }
    QJsonArray frames;
    for (int f = 0; f < frameCount; f++) {
        const double time = start + f / rate;
        QJsonArray frame;
        for (const Curve& curve : curves) {
            frame.append(std::round(evaluateCurve(curve.keys, time) * 1.0e5) / 1.0e5);
        }
        frames.append(frame);
    }
    const QJsonObject json { { "version", 1 }, { "frameRate", rate },
                             { "loop", unity::toNumber(settings.value("m_LoopTime")) != 0.0 },
                             { "muscles", muscles }, { "frames", frames } };
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(json).toJson(QJsonDocument::Compact)) < 0) {
        if (error) {
            *error = "cannot write " + path;
        }
        return false;
    }
    if (frameCountOut) {
        *frameCountOut = frameCount;
    }
    if (frameRateOut) {
        *frameRateOut = rate;
    }
    return true;
}

GestureClips gestureClips(const unity::Package& package, const QVariantMap& descriptor) {
    GestureClips clips;
    QString controller;
    for (const QVariant& entry : descriptor.value("baseAnimationLayers").toList()) {
        const QVariantMap layer = entry.toMap();
        if ((int)unity::toNumber(layer.value("type")) == 3 && unity::toNumber(layer.value("isDefault")) == 0.0) {
            controller = unity::guidOf(layer.value("animatorController"));
        }
    }
    if (controller.isEmpty() || !package.byGuid(controller)) {
        return clips;
    }
    QHash<qint64, const unity::YamlObject*> byID;
    for (const auto& object : package.objects(controller)) {
        byID.insert(object.fileID, &object);
    }
    auto clipOf = [&](qint64 stateID) -> QString {
        const unity::YamlObject* state = byID.value(stateID);
        QVariant motion = state ? state->fields.value("m_Motion") : QVariant();
        for (int depth = 0; depth < 4; depth++) {
            const QString guid = unity::guidOf(motion);
            if (!guid.isEmpty()) {
                return guid;
            }
            const unity::YamlObject* tree = byID.value(unity::fileIDOf(motion));  // a blend tree: its first motion
            if (!tree || tree->classID != 206) {
                break;
            }
            const QVariantList children = tree->fields.value("m_Childs").toList();
            motion = children.isEmpty() ? QVariant() : children.first().toMap().value("m_Motion");
        }
        return QString();
    };
    for (const auto& object : package.objects(controller)) {
        if (object.classID != 1101 && object.classID != 1109) {
            continue;
        }
        for (const QVariant& entry : object.fields.value("m_Conditions").toList()) {
            const QVariantMap condition = entry.toMap();
            if ((int)unity::toNumber(condition.value("m_ConditionMode")) != 6) {  // Equals
                continue;
            }
            const int gesture = (int)std::lround(unity::toNumber(condition.value("m_EventTreshold"), -1.0));
            const QString parameter = condition.value("m_ConditionEvent").toString();
            if (gesture < 0 || gesture > 7 || (parameter != "GestureLeft" && parameter != "GestureRight")) {
                continue;
            }
            QString& slot = parameter == "GestureLeft" ? clips.left[gesture] : clips.right[gesture];
            if (slot.isEmpty()) {
                slot = clipOf(unity::fileIDOf(object.fields.value("m_DstState")));
            }
        }
    }
    return clips;
}

}  // namespace avatarimport
