//
//  Humanoid.h
//  libraries/animation/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_Humanoid_h
#define hifi_Humanoid_h

#include <vector>

#include <QString>
#include <QVariantMap>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class AnimSkeleton;

// Humanoid retargeting, the idea behind Unity's humanoid "muscles": every skeleton is first stood in one
// canonical T-pose, and a pose moves from one skeleton to another as each bone's turn away from that T-pose.
// It works whatever way an artist pointed the bones and whatever pose the model was made in.
namespace humanoid {

// Unity's humanoid bones, in Unity's HumanBodyBones order (an avatar's humanoid map in a Unity package names these).
enum Bone : int {
    Hips = 0, LeftUpperLeg, RightUpperLeg, LeftLowerLeg, RightLowerLeg, LeftFoot, RightFoot, Spine, Chest, Neck,
    Head, LeftShoulder, RightShoulder, LeftUpperArm, RightUpperArm, LeftLowerArm, RightLowerArm, LeftHand, RightHand,
    LeftToes, RightToes, LeftEye, RightEye, Jaw,
    LeftThumbProximal, LeftThumbIntermediate, LeftThumbDistal, LeftIndexProximal, LeftIndexIntermediate,
    LeftIndexDistal, LeftMiddleProximal, LeftMiddleIntermediate, LeftMiddleDistal, LeftRingProximal,
    LeftRingIntermediate, LeftRingDistal, LeftLittleProximal, LeftLittleIntermediate, LeftLittleDistal,
    RightThumbProximal, RightThumbIntermediate, RightThumbDistal, RightIndexProximal, RightIndexIntermediate,
    RightIndexDistal, RightMiddleProximal, RightMiddleIntermediate, RightMiddleDistal, RightRingProximal,
    RightRingIntermediate, RightRingDistal, RightLittleProximal, RightLittleIntermediate, RightLittleDistal,
    UpperChest,
    BoneCount
};

// Unity's name for a bone, as a humanoid map writes it: "LeftUpperArm", "Left Index Proximal".
QString unityName(int bone);
// Darkride's standard joint name for a bone, Mixamo's naming: "LeftArm", "LeftHandIndex1".
QString standardName(int bone);
// The bone a Unity name stands for, or -1.
int boneFromUnityName(const QString& name);

// The skeleton's joint for each bone, found by Darkride's standard names; -1 where the skeleton has none.
std::vector<int> mapBones(const AnimSkeleton& skeleton);

// Every joint's absolute rotation and position with the body in the canonical T-pose: facing +Z, the left arm
// along +X and the right along -X, palms down, fingers straight along the arms, legs straight down. Spine, neck,
// head, shoulders, thumbs and feet keep the skeleton's own default pose; joints that are not humanoid bones
// move with their parents. Invalid when the skeleton lacks the hips, arms or legs.
struct TPose {
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> positions;
    bool isValid() const { return !rotations.empty(); }
};
TPose computeTPose(const AnimSkeleton& skeleton);

// Unity's humanoid muscles (HumanTrait), 95 of them: each turns one bone about one of its axes (0 the twist about X,
// 1 Y, 2 Z) between two limits, -1 at the lower and 1 at the upper.
const int MUSCLE_COUNT = 95;
QString muscleName(int muscle);  // "Left Arm Down-Up"
int muscleBone(int muscle);
int muscleAxis(int muscle);
float muscleDefaultMin(int muscle);  // degrees
float muscleDefaultMax(int muscle);
int boneMuscle(int bone, int axis);  // -1 where the bone has no muscle about that axis
int boneParent(int bone);            // the humanoid parent, -1 for the hips
glm::vec3 boneLimitSign(int bone);   // Unity's sign per axis, in Unity's coordinates
float tposeMuscle(int muscle);       // the muscle's value in a T-pose
// The muscle a clip's curve animates: "Left Arm Down-Up", or a clip's spelling of a finger's, "LeftHand.Index.1 Stretched".
int muscleFromClipAttribute(const QString& attribute);
// Unity's muscle rotation in a bone's muscle frame for angles in degrees about X (twist), Y and Z.
glm::quat muscleRotation(const glm::vec3& degrees);

// Unity's humanoid frames for one avatar, as the avatar importer writes them in the .fst ("humanoid = {...}") in the
// engine's coordinates: per bone, Unity's pre and post rotations, limit signs and limits, and how much of each limb's
// twist its child carries. With them a bone's rotation is pre * muscleRotation(angles) * inverse(post), as Unity makes it.
class MuscleFrames {
public:
    bool setup(const AnimSkeleton& skeleton, const QVariantMap& description);
    bool isValid() const { return _valid; }
    // Sets the relative rotation of every humanoid joint whose muscles are animated (Unity's 95 values); the other
    // joints keep their poses.
    void pose(const std::vector<float>& muscles, const std::vector<bool>& animated, std::vector<glm::quat>& relativeRotations) const;

private:
    struct Frame {
        int joint { -1 };
        glm::quat pre;
        glm::quat post;
        glm::vec3 sign { 1.0f };
        glm::vec3 minimum;  // degrees, per axis
        glm::vec3 maximum;
    };
    Frame _frames[BoneCount];
    float _twist[4] { 0.5f, 0.5f, 0.5f, 0.5f };  // the share a child carries: upper arm, lower arm, upper leg, lower leg
    bool _valid { false };
};

}

#endif // hifi_Humanoid_h
