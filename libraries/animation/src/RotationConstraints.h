//
//  RotationConstraints.h
//  libraries/animation/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_RotationConstraints_h
#define hifi_RotationConstraints_h

#include <vector>

#include <QVariantList>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AnimPose.h"

class AnimSkeleton;

// Rotation constraints, as avatars from Unity packages turn their twist bones (constraint components that behave as
// Unity's RotationConstraint). Each joint turns with the weighted rotation of its source joints, plus an offset; the result is
// blended from the rest rotation by the constraint's weight (a normalized lerp); and on the axes the constraint does not
// drive (Euler angles, Z then X then Y) it keeps the rest rotation's angles. From the avatar's .fst, one line per
// constrained joint:
//
//   rotationConstraint = <joint> = {"sources": [["<joint>", weight], ...], "weight": w, "axes": [x, y, z],
//                                   "atRest": [x, y, z, w], "offset": [x, y, z, w], "local": false}
//
// joints named as the avatar names them (after jointMap), rotations in the model's frame; "local" solves from the
// sources' local rotations instead of their rotations in the model.
class RotationConstraints {
public:
    void setup(const AnimSkeleton& skeleton, const QVariantList& settings);
    bool isEmpty() const { return _constraints.empty(); }

    // Turns each constrained joint, in order, from the pose's other joints.
    void apply(const AnimSkeleton& skeleton, AnimPoseVec& relativePoses) const;

    // Euler angles in radians, Z then X then Y (q = qY * qX * qZ), as Unity reads and writes them.
    static glm::vec3 eulerZXY(const glm::quat& rotation);
    static glm::quat fromEulerZXY(const glm::vec3& angles);

private:
    struct Constraint {
        int joint { -1 };
        std::vector<std::pair<int, float>> sources;
        float weight { 1.0f };
        bool axes[3] { true, true, true };
        glm::quat atRest;
        glm::quat offset;
        bool local { false };
    };
    std::vector<Constraint> _constraints;
};

#endif // hifi_RotationConstraints_h
