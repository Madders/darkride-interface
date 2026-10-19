//
//  UnityTransforms.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_UnityTransforms_h
#define hifi_UnityTransforms_h

#include <QString>
#include <QStringList>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <hfm/HFM.h>

namespace unity {

// Each of a model's joints by its Unity hierarchy path below the model's root ("armature/Hips"): the FBX's node names
// from the top down, which is also how a clip's curves and a model-object ID (UnityIds.h) name it.
QStringList jointPaths(const hfm::Model& model);

// A joint's transform in its parent's frame, as the engine builds its default pose from the FBX (AnimSkeleton): in the
// model file's right-handed frame and units.
void jointLocalPose(const hfm::Joint& joint, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

// Unity reads an FBX into its left-handed frame by negating X, and (with the importer's "Use File Scale" and the
// global scale) in meters. Its local transforms come back to the engine's frame this way; metersPerUnit is the
// length of one file unit in Unity's meters.
glm::vec3 positionFromUnity(const glm::vec3& position, float metersPerUnit);
glm::quat rotationFromUnity(const glm::quat& rotation);  // Unity's (x, y, z, w) as a glm quaternion
// Unity's Euler angles in degrees, turning about Z, then X, then Y (Quaternion.Euler, and the default rotation order
// of clips' curves): the rotation in Unity's own frame, and in the engine's.
glm::quat unityEulerRotation(const glm::vec3& degrees);
glm::quat rotationFromUnityEuler(const glm::vec3& degrees);

// Meters per file unit for a model, from its importer settings (.meta ModelImporter) and the FBX's UnitScaleFactor
// (centimeters per unit) as the engine read it into hfm::Model::offset.
float metersPerUnit(const QVariantMap& modelImporter, const hfm::Model& model);

}

#endif // hifi_UnityTransforms_h
