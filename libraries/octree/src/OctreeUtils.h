//
//  OctreeUtils.h
//  libraries/octree/src
//
//  Created by Andrew Meadows 2016.03.04
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_OctreeUtils_h
#define hifi_OctreeUtils_h

#include <NumericalConstants.h>

#include "OctreeConstants.h"

class AABox;
class AACube;
class QJsonDocument;

float boundaryDistanceForRenderLevel(unsigned int renderLevel, float visibilityDistance);

float getPerspectiveAccuracyHalfAngleTan(float visibilityDistance, int boundaryLevelAdjust);
float getPerspectiveAccuracyHalfAngle(float visibilityDistance, int boundaryLevelAdjust);
float getVisibilityDistanceFromHalfAngle(float halfAngle);
float getHalfAngleFromVisibilityDistance(float visibilityDistance);
float getOrthographicAccuracySize(float visibilityDistance, int boundaryLevelAdjust);

// MIN_ELEMENT_ANGULAR_DIAMETER = angular diameter of 1x1x1m cube at 400m = sqrt(3) / 400 = 0.0043301 radians ~= 0.25 degrees
const float MIN_ELEMENT_ANGULAR_DIAMETER = 0.0043301f; // radians
// NOTE: the entity bounding cube is larger than the smallest possible containing octree element by sqrt(3)
const float MIN_ENTITY_ANGULAR_DIAMETER = MIN_ELEMENT_ANGULAR_DIAMETER * SQRT_THREE;
const float MIN_VISIBLE_DISTANCE = 0.0001f; // helps avoid divide-by-zero check

// An impostor card (IMPOSTORS.md) is drawn as a square of side two radii around its sphere. The client's draw cull
// (LODManager::shouldRender at the default LOD angle) keeps a bound whose half-diagonal looks larger than
// UNIT_ELEMENT_MAX_EXTENT / DEFAULT_VISIBILITY_DISTANCE_FOR_UNIT_ELEMENT; a card's half-diagonal is sqrt(3) radii, so
// the card draws while radius / distance is above this: 1 / 800, 0.0716 degrees, 800 radii. The entity server sends
// a carded entity out to there, to a client that draws cards (ENTITY-STREAMING.md 3.3), as radius / distance like the
// two tests above.
const float MIN_IMPOSTOR_CARD_ANGULAR_SIZE = 1.0f / (2.0f * DEFAULT_VISIBILITY_DISTANCE_FOR_UNIT_ELEMENT); // radians

#endif // hifi_OctreeUtils_h
