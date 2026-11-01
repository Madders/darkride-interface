//
//  FaceShapes.h
//  libraries/unity-import/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once

#include <QtCore/QSet>
#include <QtCore/QStringList>

namespace unity {

// The .fst `bs` lines that put the engine's face shapes (ARKit's 52 under their Faceshift names, BlendshapeConstants.h)
// on an avatar's own shape keys, for face tracking: the avatar's ARKit-named shapes ("perfect sync") first, else the
// names other avatars use for the same faces (VRoid's Fcl_ names, the usual anime-avatar names, MMD's Japanese morphs,
// the lip-sync visemes), half weight per side where an avatar has one shape for both sides.
//
// Only the shapes the engine's own procedural face never drives: with face tracking off they stay at 0 and change
// nothing, and the avatar's own animations of the same shape keys add to them rather than being replaced. The blink,
// the wide eyes, the brows' lift, the smile, the jaw, the funnel and the upper lip (Head::simulate drives those from
// the voice and the blink) are left to the importer's existing rules and to faceTrackingOnlyShapeLines.
QStringList faceTrackingShapeLines(const QSet<QString>& shapes, const QStringList& alreadyMapped);

// The `faceTrackingBs` lines for those twelve procedurally driven shapes, where the existing rules (alreadyMapped) left
// them out: the same matching, but the engine moves them only with face tracking or a script, never from the voice
// or the automatic blink (SkeletonModel), so an anime face doesn't grin and lift its brows while its owner talks.
QStringList faceTrackingOnlyShapeLines(const QSet<QString>& shapes, const QStringList& alreadyMapped);

// how many of the engine's 52 face shapes the lines name (each engine name once, bs and faceTrackingBs lines), for the
// import's notes
int countFaceShapes(const QStringList& lines);

}  // namespace unity
