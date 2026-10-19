//
//  UnityTransforms.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "UnityTransforms.h"

#include <glm/gtx/transform.hpp>

#include "UnityYaml.h"

namespace unity {

QStringList jointPaths(const hfm::Model& model) {
    QStringList paths;
    paths.reserve((int)model.joints.size());
    for (int i = 0; i < (int)model.joints.size(); i++) {
        QStringList names;
        for (int j = i; j >= 0 && j < (int)model.joints.size() && names.size() <= (int)model.joints.size();
             j = model.joints[j].parentIndex) {
            names.prepend(model.joints[j].name);
        }
        paths.append(names.join('/'));
    }
    return paths;
}

void jointLocalPose(const hfm::Joint& joint, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale) {
    const glm::mat4 local = glm::translate(joint.translation) * joint.preTransform * glm::mat4_cast(joint.preRotation) *
        glm::mat4_cast(joint.rotation) * glm::mat4_cast(joint.postRotation) * joint.postTransform;
    translation = glm::vec3(local[3]);
    scale = glm::vec3(glm::length(glm::vec3(local[0])), glm::length(glm::vec3(local[1])), glm::length(glm::vec3(local[2])));
    const glm::mat3 axes(glm::vec3(local[0]) / scale.x, glm::vec3(local[1]) / scale.y, glm::vec3(local[2]) / scale.z);
    rotation = glm::normalize(glm::quat_cast(axes));
}

glm::vec3 positionFromUnity(const glm::vec3& position, float metersPerUnit) {
    return glm::vec3(-position.x, position.y, position.z) / metersPerUnit;
}

glm::quat rotationFromUnity(const glm::quat& rotation) {
    return glm::quat(rotation.w, rotation.x, -rotation.y, -rotation.z);
}

glm::quat unityEulerRotation(const glm::vec3& degrees) {
    const glm::vec3 r = glm::radians(degrees);
    // about Z first, then X, then Y: q = qY * qX * qZ
    const glm::quat qx = glm::angleAxis(r.x, glm::vec3(1, 0, 0));
    const glm::quat qy = glm::angleAxis(r.y, glm::vec3(0, 1, 0));
    const glm::quat qz = glm::angleAxis(r.z, glm::vec3(0, 0, 1));
    return qy * qx * qz;
}

glm::quat rotationFromUnityEuler(const glm::vec3& degrees) {
    return rotationFromUnity(unityEulerRotation(degrees));
}

float metersPerUnit(const QVariantMap& modelImporter, const hfm::Model& model) {
    const QVariantMap meshes = modelImporter.value("meshes").toMap();
    const float globalScale = (float)toNumber(meshes.value("globalScale"), 1.0);
    const bool useFileScale = toNumber(meshes.value("useFileScale"), 1.0) != 0.0;
    const float fileScale = glm::length(glm::vec3(model.offset[0]));  // the engine's meters per unit, from UnitScaleFactor
    return (useFileScale ? fileScale : 1.0f) * globalScale;
}

}  // namespace unity
