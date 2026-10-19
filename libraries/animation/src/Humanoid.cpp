//
//  Humanoid.cpp
//  libraries/animation/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "Humanoid.h"

#include <cmath>

#include <QHash>
#include <QRegularExpression>

#include <GLMHelpers.h>

#include "AnimSkeleton.h"

namespace humanoid {

namespace {

struct Names {
    const char* unity;
    const char* standard;
};

struct MuscleTrait {
    const char* name;
    int bone;
    int axis;
    float minimum;
    float maximum;
};

#include "HumanoidTables.inc"

const Names NAMES[BoneCount] = {
    { "Hips", "Hips" }, { "LeftUpperLeg", "LeftUpLeg" }, { "RightUpperLeg", "RightUpLeg" },
    { "LeftLowerLeg", "LeftLeg" }, { "RightLowerLeg", "RightLeg" }, { "LeftFoot", "LeftFoot" },
    { "RightFoot", "RightFoot" }, { "Spine", "Spine" }, { "Chest", "Spine1" }, { "Neck", "Neck" },
    { "Head", "Head" }, { "LeftShoulder", "LeftShoulder" }, { "RightShoulder", "RightShoulder" },
    { "LeftUpperArm", "LeftArm" }, { "RightUpperArm", "RightArm" }, { "LeftLowerArm", "LeftForeArm" },
    { "RightLowerArm", "RightForeArm" }, { "LeftHand", "LeftHand" }, { "RightHand", "RightHand" },
    { "LeftToes", "LeftToeBase" }, { "RightToes", "RightToeBase" }, { "LeftEye", "LeftEye" },
    { "RightEye", "RightEye" }, { "Jaw", "Jaw" },
    { "Left Thumb Proximal", "LeftHandThumb1" }, { "Left Thumb Intermediate", "LeftHandThumb2" },
    { "Left Thumb Distal", "LeftHandThumb3" }, { "Left Index Proximal", "LeftHandIndex1" },
    { "Left Index Intermediate", "LeftHandIndex2" }, { "Left Index Distal", "LeftHandIndex3" },
    { "Left Middle Proximal", "LeftHandMiddle1" }, { "Left Middle Intermediate", "LeftHandMiddle2" },
    { "Left Middle Distal", "LeftHandMiddle3" }, { "Left Ring Proximal", "LeftHandRing1" },
    { "Left Ring Intermediate", "LeftHandRing2" }, { "Left Ring Distal", "LeftHandRing3" },
    { "Left Little Proximal", "LeftHandPinky1" }, { "Left Little Intermediate", "LeftHandPinky2" },
    { "Left Little Distal", "LeftHandPinky3" },
    { "Right Thumb Proximal", "RightHandThumb1" }, { "Right Thumb Intermediate", "RightHandThumb2" },
    { "Right Thumb Distal", "RightHandThumb3" }, { "Right Index Proximal", "RightHandIndex1" },
    { "Right Index Intermediate", "RightHandIndex2" }, { "Right Index Distal", "RightHandIndex3" },
    { "Right Middle Proximal", "RightHandMiddle1" }, { "Right Middle Intermediate", "RightHandMiddle2" },
    { "Right Middle Distal", "RightHandMiddle3" }, { "Right Ring Proximal", "RightHandRing1" },
    { "Right Ring Intermediate", "RightHandRing2" }, { "Right Ring Distal", "RightHandRing3" },
    { "Right Little Proximal", "RightHandPinky1" }, { "Right Little Intermediate", "RightHandPinky2" },
    { "Right Little Distal", "RightHandPinky3" },
    { "UpperChest", "Spine2" },
};

}  // namespace

QString unityName(int bone) {
    return bone >= 0 && bone < BoneCount ? QString(NAMES[bone].unity) : QString();
}

QString standardName(int bone) {
    return bone >= 0 && bone < BoneCount ? QString(NAMES[bone].standard) : QString();
}

int boneFromUnityName(const QString& name) {
    static const QHash<QString, int> BONES = [] {
        QHash<QString, int> bones;
        for (int b = 0; b < BoneCount; b++) {
            bones.insert(NAMES[b].unity, b);
        }
        return bones;
    }();
    return BONES.value(name, -1);
}

std::vector<int> mapBones(const AnimSkeleton& skeleton) {
    std::vector<int> joints(BoneCount, -1);
    for (int b = 0; b < BoneCount; b++) {
        joints[b] = skeleton.nameToJointIndex(NAMES[b].standard);
    }
    return joints;
}

TPose computeTPose(const AnimSkeleton& skeleton) {
    TPose pose;
    const std::vector<int> bones = mapBones(skeleton);
    for (int required : { Hips, LeftUpperLeg, RightUpperLeg, LeftLowerLeg, RightLowerLeg, LeftFoot, RightFoot,
                          LeftUpperArm, RightUpperArm, LeftLowerArm, RightLowerArm, LeftHand, RightHand }) {
        if (bones[required] < 0) {
            return pose;
        }
    }

    const int count = skeleton.getNumJoints();
    pose.rotations.resize(count);
    pose.positions.resize(count);
    std::vector<std::vector<int>> children(count);
    for (int j = 0; j < count; j++) {
        const AnimPose& rest = skeleton.getAbsoluteDefaultPose(j);
        pose.rotations[j] = rest.rot();
        pose.positions[j] = rest.trans();
        const int parent = skeleton.getParentIndex(j);
        if (parent >= 0) {
            children[parent].push_back(j);
        }
    }

    // Turn a joint and everything under it about the joint.
    auto turnSubtree = [&](int joint, const glm::quat& turn) {
        const glm::vec3 pivot = pose.positions[joint];
        std::vector<int> stack { joint };
        while (!stack.empty()) {
            const int m = stack.back();
            stack.pop_back();
            pose.rotations[m] = glm::normalize(turn * pose.rotations[m]);
            pose.positions[m] = pivot + turn * (pose.positions[m] - pivot);
            stack.insert(stack.end(), children[m].begin(), children[m].end());
        }
    };
    // Swing a joint so the line to another joint points along a direction.
    auto aim = [&](int joint, int toward, const glm::vec3& direction) {
        if (joint < 0 || toward < 0) {
            return;
        }
        const glm::vec3 line = pose.positions[toward] - pose.positions[joint];
        const float length = glm::length(line);
        if (length > 1.0e-6f) {
            turnSubtree(joint, rotationBetween(line / length, direction));
        }
    };
    auto firstChild = [&](int joint) {
        return joint >= 0 && !children[joint].empty() ? children[joint].front() : -1;
    };

    // Face +Z: the line from the right hip to the left hip should point along +X.
    {
        glm::vec3 right = pose.positions[bones[LeftUpperLeg]] - pose.positions[bones[RightUpperLeg]];
        right.y = 0.0f;
        if (glm::length(right) > 1.0e-6f) {
            int root = bones[Hips];
            while (skeleton.getParentIndex(root) >= 0) {
                root = skeleton.getParentIndex(root);
            }
            // A turn about the vertical alone, so a skeleton made facing -Z is turned around rather than flipped.
            turnSubtree(root, glm::angleAxis(atan2f(right.z, right.x), glm::vec3(0.0f, 1.0f, 0.0f)));
        }
    }

    for (int side = 0; side < 2; side++) {
        const bool left = side == 0;
        const glm::vec3 along(left ? 1.0f : -1.0f, 0.0f, 0.0f);
        const int offset = left ? 0 : (RightThumbProximal - LeftThumbProximal);
        const int upperArm = bones[left ? LeftUpperArm : RightUpperArm];
        const int lowerArm = bones[left ? LeftLowerArm : RightLowerArm];
        const int hand = bones[left ? LeftHand : RightHand];

        aim(upperArm, lowerArm, along);
        aim(lowerArm, hand, along);

        const int index = bones[LeftIndexProximal + offset];
        const int middle = bones[LeftMiddleProximal + offset];
        const int little = bones[LeftLittleProximal + offset];
        aim(hand, middle >= 0 ? middle : (index >= 0 ? index : firstChild(hand)), along);

        // Palm down: with the arm out to the side and the palm down, the index finger is in front of the little
        // finger, so the line from the little finger's base to the index finger's base points along +Z.
        if (index >= 0 && little >= 0) {
            glm::vec3 across = pose.positions[index] - pose.positions[little];
            across -= glm::dot(across, along) * along;
            if (glm::length(across) > 1.0e-6f) {
                across = glm::normalize(across);
                const glm::vec3 forward(0.0f, 0.0f, 1.0f);
                const float angle = atan2f(glm::dot(glm::cross(across, forward), along), glm::dot(across, forward));
                turnSubtree(hand, glm::angleAxis(angle, along));
            }
        }

        for (int finger : { LeftIndexProximal, LeftMiddleProximal, LeftRingProximal, LeftLittleProximal }) {
            const int proximal = bones[finger + offset];
            const int intermediate = bones[finger + 1 + offset];
            const int distal = bones[finger + 2 + offset];
            aim(proximal, intermediate, along);
            aim(intermediate, distal, along);
            aim(distal, firstChild(distal), along);
        }
    }

    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    aim(bones[LeftUpperLeg], bones[LeftLowerLeg], down);
    aim(bones[LeftLowerLeg], bones[LeftFoot], down);
    aim(bones[RightUpperLeg], bones[RightLowerLeg], down);
    aim(bones[RightLowerLeg], bones[RightFoot], down);
    return pose;
}

QString muscleName(int muscle) {
    return muscle >= 0 && muscle < MUSCLE_COUNT ? QString(MUSCLES[muscle].name) : QString();
}

int muscleBone(int muscle) {
    return muscle >= 0 && muscle < MUSCLE_COUNT ? MUSCLES[muscle].bone : -1;
}

int muscleAxis(int muscle) {
    return muscle >= 0 && muscle < MUSCLE_COUNT ? MUSCLES[muscle].axis : -1;
}

float muscleDefaultMin(int muscle) {
    return muscle >= 0 && muscle < MUSCLE_COUNT ? MUSCLES[muscle].minimum : 0.0f;
}

float muscleDefaultMax(int muscle) {
    return muscle >= 0 && muscle < MUSCLE_COUNT ? MUSCLES[muscle].maximum : 0.0f;
}

int boneMuscle(int bone, int axis) {
    for (int m = 0; m < MUSCLE_COUNT; m++) {
        if (MUSCLES[m].bone == bone && MUSCLES[m].axis == axis) {
            return m;
        }
    }
    return -1;
}

int boneParent(int bone) {
    return bone >= 0 && bone < BoneCount ? BONE_PARENTS[bone] : -1;
}

glm::vec3 boneLimitSign(int bone) {
    return bone >= 0 && bone < BoneCount ? glm::vec3(LIMIT_SIGNS[bone][0], LIMIT_SIGNS[bone][1], LIMIT_SIGNS[bone][2])
                                         : glm::vec3(1.0f);
}

float tposeMuscle(int muscle) {
    return muscle >= 0 && muscle < MUSCLE_COUNT ? TPOSE_MUSCLES[muscle] : 0.0f;
}

int muscleFromClipAttribute(const QString& attribute) {
    static const QHash<QString, int> MUSCLES_BY_NAME = [] {
        QHash<QString, int> byName;
        for (int m = 0; m < MUSCLE_COUNT; m++) {
            const QString name = MUSCLES[m].name;
            byName.insert(name, m);
            // Clips spell a finger's muscles by hand: "Left Index 1 Stretched" is "LeftHand.Index.1 Stretched".
            static const QRegularExpression FINGER("^(Left|Right) (Thumb|Index|Middle|Ring|Little) (.*)$");
            const auto match = FINGER.match(name);
            if (match.hasMatch()) {
                byName.insert(match.captured(1) + "Hand." + match.captured(2) + "." + match.captured(3), m);
            }
        }
        return byName;
    }();
    return MUSCLES_BY_NAME.value(attribute, -1);
}

glm::quat muscleRotation(const glm::vec3& degrees) {
    const glm::vec3 t = glm::vec3(tanf(glm::radians(degrees.x) * 0.5f), tanf(glm::radians(degrees.y) * 0.5f),
                                  tanf(glm::radians(degrees.z) * 0.5f));
    return glm::normalize(glm::quat(1.0f, t.x, t.y + t.x * t.z, t.z - t.x * t.y));
}

namespace {

glm::quat toQuat(const QVariant& value) {
    const QVariantList list = value.toList();
    return list.size() == 4 ? glm::quat(list[3].toFloat(), list[0].toFloat(), list[1].toFloat(), list[2].toFloat()) : glm::quat();
}

glm::vec3 toVec3(const QVariant& value, const glm::vec3& fallback) {
    const QVariantList list = value.toList();
    return list.size() == 3 ? glm::vec3(list[0].toFloat(), list[1].toFloat(), list[2].toFloat()) : fallback;
}

// Each limb whose twist is shared with its child: the parent, the child, and the share's place in MuscleFrames::_twist.
const int TWIST_SHARES[8][3] = {
    { LeftUpperArm, LeftLowerArm, 0 }, { RightUpperArm, RightLowerArm, 0 }, { LeftLowerArm, LeftHand, 1 },
    { RightLowerArm, RightHand, 1 }, { LeftUpperLeg, LeftLowerLeg, 2 }, { RightUpperLeg, RightLowerLeg, 2 },
    { LeftLowerLeg, LeftFoot, 3 }, { RightLowerLeg, RightFoot, 3 },
};

}  // namespace

bool MuscleFrames::setup(const AnimSkeleton& skeleton, const QVariantMap& description) {
    _valid = false;
    const QVariantMap bones = description.value("bones").toMap();
    for (int b = 0; b < BoneCount; b++) {
        Frame frame;
        const QVariantMap entry = bones.value(standardName(b)).toMap();
        const int joint = entry.isEmpty() ? -1 : skeleton.nameToJointIndex(standardName(b));
        if (joint >= 0) {
            frame.joint = joint;
            frame.pre = toQuat(entry.value("pre"));
            frame.post = toQuat(entry.value("post"));
            frame.sign = toVec3(entry.value("sign"), glm::vec3(1.0f));
            glm::vec3 minimum, maximum;
            for (int axis = 0; axis < 3; axis++) {
                const int m = boneMuscle(b, axis);
                minimum[axis] = muscleDefaultMin(m);
                maximum[axis] = muscleDefaultMax(m);
            }
            frame.minimum = toVec3(entry.value("min"), minimum);
            frame.maximum = toVec3(entry.value("max"), maximum);
            _valid = true;
        }
        _frames[b] = frame;
    }
    const QVariantList twist = description.value("twist").toList();
    for (int i = 0; i < 4 && i < twist.size(); i++) {
        _twist[i] = twist[i].toFloat();
    }
    return _valid;
}

void MuscleFrames::pose(const std::vector<float>& muscles, const std::vector<bool>& animated,
                        std::vector<glm::quat>& relativeRotations) const {
    glm::vec3 angles[BoneCount];
    for (auto& angle : angles) {
        angle = glm::vec3(0.0f);
    }
    bool moved[BoneCount] = {};
    for (int m = 0; m < MUSCLE_COUNT && m < (int)muscles.size() && m < (int)animated.size(); m++) {
        if (!animated[m]) {
            continue;
        }
        // A spine bone the avatar lacks gives its muscles to its nearest present ancestor, as Unity gives a missing
        // UpperChest's to the Chest; a missing jaw's, toes' or finger's do nothing.
        int bone = MUSCLES[m].bone;
        if (_frames[bone].joint < 0 && bone != UpperChest && bone != Chest && bone != Neck) {
            continue;
        }
        while (bone >= 0 && _frames[bone].joint < 0) {
            bone = BONE_PARENTS[bone];
        }
        if (bone < 0) {
            continue;
        }
        const Frame& frame = _frames[bone];
        const int axis = MUSCLES[m].axis;
        const float value = muscles[m];
        const float limit = MUSCLES[m].bone == bone ? (value >= 0.0f ? frame.maximum[axis] : -frame.minimum[axis])
                                                    : (value >= 0.0f ? MUSCLES[m].maximum : -MUSCLES[m].minimum);
        angles[bone][axis] += value * limit * frame.sign[axis];
        moved[bone] = true;
    }

    // A twisted upper limb keeps part of its twist, and its child turns by the rest about the parent's twist axis.
    float childTwist[BoneCount] = {};
    int childTwistFrom[BoneCount];
    for (int& from : childTwistFrom) {
        from = -1;
    }
    for (const auto& share : TWIST_SHARES) {
        const int parent = share[0];
        const int child = share[1];
        if (moved[parent] && _frames[child].joint >= 0) {
            childTwist[child] = angles[parent].x * _twist[share[2]];
            childTwistFrom[child] = parent;
            angles[parent].x -= childTwist[child];
            moved[child] = true;
        }
    }

    for (int b = 0; b < BoneCount; b++) {
        const Frame& frame = _frames[b];
        if (!moved[b] || frame.joint < 0 || frame.joint >= (int)relativeRotations.size()) {
            continue;
        }
        glm::quat rotation = frame.pre * muscleRotation(angles[b]) * glm::inverse(frame.post);
        if (childTwist[b] != 0.0f && childTwistFrom[b] >= 0) {
            const glm::vec3 axis = _frames[childTwistFrom[b]].post * glm::vec3(1.0f, 0.0f, 0.0f);
            rotation = glm::angleAxis(glm::radians(childTwist[b]), axis) * rotation;
        }
        relativeRotations[frame.joint] = glm::normalize(rotation);
    }
}

}  // namespace humanoid

