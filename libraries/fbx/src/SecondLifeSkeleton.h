//
//  SecondLifeSkeleton.h
//  libraries/fbx/src
//
//  Created on 2026-10-19 for Darkride's avatar support.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SecondLifeSkeleton_h
#define hifi_SecondLifeSkeleton_h

#include <QHash>
#include <QString>
#include <QStringList>

struct aiScene;

// A Second Life or OpenSim mesh body's skeleton made whole again. Avastar and other exporters write only the joints a
// body is weighted to, and write them all as children of the armature: a "fitted mesh" body (Ruth2, Roth2) weighted to
// the collision volumes (PELVIS, BELLY, L_UPPER_ARM...) and a few bones has no mPelvis, mTorso or mShoulderLeft at all,
// and each volume's transform is still its offset from the bone it belongs under. The missing bones are put back where
// the skin's bind poses say they are (a volume's bind pose less its offset), and every joint goes under its parent in
// Second Life's skeleton, the nearest one there is, resting where it was bound less a volume's size (the skin's offsets
// follow, so it lies as bound). The bones have Second Life's names, which secondLifeJointNames turns into Darkride's.
// Returns the bones put back (they skin nothing themselves, but are the skeleton); nothing is changed, and none
// returned, for a skeleton that isn't one of these.
QStringList rebuildSecondLifeSkeleton(aiScene* scene);

// Darkride's names for Second Life's bones (mPelvis is Hips, mShoulderLeft LeftArm, mHandIndex1Left LeftHandIndex1...),
// given straight, as the bones' names are fixed and the collision volumes beside them (LEFT_HANDLE) confuse working
// them out from their shapes. Bento's extra spine bones keep theirs.
const QHash<QString, QString>& secondLifeJointNames();

#endif // hifi_SecondLifeSkeleton_h
