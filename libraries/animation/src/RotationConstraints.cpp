//
//  RotationConstraints.cpp
//  libraries/animation/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "RotationConstraints.h"

#include <QVariantMap>

#include "AnimSkeleton.h"
#include "AnimationLogging.h"

namespace {

glm::quat quatOf(const QVariant& value, const glm::quat& fallback = glm::quat()) {
    const QVariantList list = value.toList();
    if (list.size() != 4) {
        return fallback;
    }
    return glm::normalize(glm::quat(list[3].toFloat(), list[0].toFloat(), list[1].toFloat(), list[2].toFloat()));
}

}  // namespace

glm::vec3 RotationConstraints::eulerZXY(const glm::quat& rotation) {
    // R = Ry(y) * Rx(x) * Rz(z): R[1][2] = -sin x, R[0][2] / R[2][2] = tan y, R[1][0] / R[1][1] = tan z (row, column)
    const glm::mat3 m = glm::mat3_cast(glm::normalize(rotation));  // m[column][row]
    const float sinX = glm::clamp(-m[2][1], -1.0f, 1.0f);
    const float x = asinf(sinX);
    float y, z;
    if (fabsf(sinX) < 0.99999f) {
        y = atan2f(m[2][0], m[2][2]);
        z = atan2f(m[0][1], m[1][1]);
    } else {  // looking straight up or down: Y and Z turn about the same axis, so Z takes none of it
        y = atan2f(-m[0][2], m[0][0]);
        z = 0.0f;
    }
    return glm::vec3(x, y, z);
}

glm::quat RotationConstraints::fromEulerZXY(const glm::vec3& angles) {
    return glm::angleAxis(angles.y, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(angles.x, glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::angleAxis(angles.z, glm::vec3(0.0f, 0.0f, 1.0f));
}

void RotationConstraints::setup(const AnimSkeleton& skeleton, const QVariantList& settings) {
    _constraints.clear();
    for (const QVariant& entry : settings) {
        const QVariantMap map = entry.toMap();
        Constraint constraint;
        constraint.joint = skeleton.nameToJointIndex(map.value("joint").toString());
        if (constraint.joint < 0) {
            qCWarning(animation) << "rotationConstraint: no joint" << map.value("joint").toString();
            continue;
        }
        for (const QVariant& sourceEntry : map.value("sources").toList()) {
            const QVariantList source = sourceEntry.toList();
            const int joint = source.isEmpty() ? -1 : skeleton.nameToJointIndex(source[0].toString());
            const float weight = source.size() > 1 ? source[1].toFloat() : 1.0f;
            if (joint >= 0 && weight > 0.0f) {
                constraint.sources.emplace_back(joint, weight);
            } else if (joint < 0) {
                qCWarning(animation) << "rotationConstraint: no source joint" << source.value(0).toString();
            }
        }
        if (constraint.sources.empty()) {
            continue;
        }
        constraint.weight = map.value("weight", 1.0f).toFloat();
        const QVariantList axes = map.value("axes").toList();
        for (int i = 0; i < 3 && i < axes.size(); i++) {
            constraint.axes[i] = axes[i].toInt() != 0;
        }
        const glm::quat restDefault = skeleton.getRelativeDefaultPose(constraint.joint).rot();
        constraint.atRest = quatOf(map.value("atRest"), restDefault);
        constraint.offset = quatOf(map.value("offset"));
        constraint.local = map.value("local").toBool();
        _constraints.push_back(constraint);
    }
}

void RotationConstraints::apply(const AnimSkeleton& skeleton, AnimPoseVec& relativePoses) const {
    const int count = (int)relativePoses.size();
    if (count != skeleton.getNumJoints() || _constraints.empty()) {
        return;
    }
    std::vector<glm::quat> absolute(count);
    auto rebuild = [&](int from) {
        for (int i = from; i < count; i++) {  // parents come before their children
            const int parent = skeleton.getParentIndex(i);
            absolute[i] = parent >= 0 ? absolute[parent] * relativePoses[i].rot() : relativePoses[i].rot();
        }
    };
    rebuild(0);
    for (const Constraint& constraint : _constraints) {
        // the sources' weighted rotation, in the model or in their parents
        glm::quat sum(0.0f, 0.0f, 0.0f, 0.0f);
        float total = 0.0f;
        glm::quat first;
        bool haveFirst = false;
        for (const auto& source : constraint.sources) {
            glm::quat q = constraint.local ? relativePoses[source.first].rot() : absolute[source.first];
            if (!haveFirst) {
                first = q;
                haveFirst = true;
            } else if (glm::dot(first, q) < 0.0f) {
                q = -q;
            }
            sum += source.second * q;
            total += source.second;
        }
        if (total <= 0.0f || glm::length(sum) < 1.0e-6f) {
            continue;
        }
        const glm::quat sources = glm::normalize(sum);
        glm::quat local;
        if (constraint.local) {
            local = sources * constraint.offset;
        } else {
            const int parent = skeleton.getParentIndex(constraint.joint);
            const glm::quat parentRotation = parent >= 0 ? absolute[parent] : glm::quat();
            local = glm::inverse(parentRotation) * sources * constraint.offset;
        }
        // blended from rest by the weight (a normalized lerp, as Unity's Quaternion.Lerp), then the axes the constraint
        // does not drive take the rest rotation's angles; with a bent and twisted elbow the order matters
        const float weight = glm::clamp(constraint.weight, 0.0f, 1.0f);
        if (glm::dot(constraint.atRest, local) < 0.0f) {
            local = -local;
        }
        glm::quat result = glm::normalize((1.0f - weight) * constraint.atRest + weight * local);
        if (!(constraint.axes[0] && constraint.axes[1] && constraint.axes[2])) {
            glm::vec3 angles = eulerZXY(result);
            const glm::vec3 rest = eulerZXY(constraint.atRest);
            for (int i = 0; i < 3; i++) {
                if (!constraint.axes[i]) {
                    angles[i] = rest[i];
                }
            }
            result = fromEulerZXY(angles);
        }
        relativePoses[constraint.joint].rot() = result;
        rebuild(constraint.joint);
    }
}
