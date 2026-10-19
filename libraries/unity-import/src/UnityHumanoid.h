//
//  UnityHumanoid.h
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_UnityHumanoid_h
#define hifi_UnityHumanoid_h

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace unity {
class Package;
}

namespace avatarimport {

// Unity's humanoid frames for an avatar ("humanoid = {...}" in its .fst, animation/src/Humanoid.h), worked out from
// the T-pose in its humanoid description (a model's .meta) as Unity works them out, in the engine's coordinates: per
// bone, post turns the bone's frame to one whose X axis runs along the bone and whose Z axis follows a body direction;
// pre holds the bone's T-pose with Unity's T-pose muscle values taken off. Against Unity on a test avatar: post exact,
// pre exact for the body and within about a degree for fingers, ten for thumbs.
QVariantMap humanoidFrames(const QVariantMap& humanDescription, QStringList& notes);

// Samples a Unity clip's humanoid muscle curves into a muscle clip (".muscles.json", hfm::MuscleClip) at the clip's own
// rate. False when the clip moves no muscle.
bool writeMuscleClip(const unity::Package& package, const QString& clipGuid, const QString& path, QString* error = nullptr,
                     int* frameCount = nullptr, double* frameRate = nullptr);

// The standard hand gestures 0 to 7 (neutral, fist, open hand, point, victory, rock'n'roll, hand gun, thumbs up): the clip
// the avatar's hand-gesture layer plays for each, per hand, found from its GestureLeft/GestureRight conditions.
struct GestureClips {
    QString left[8];   // clip GUIDs
    QString right[8];
};
GestureClips gestureClips(const unity::Package& package, const QVariantMap& descriptor);

}

#endif // hifi_UnityHumanoid_h
