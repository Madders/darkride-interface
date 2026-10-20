//
//  GazeStyle.cpp
//  libraries/avatars/src
//
//  Created on 2026-10-15 for Darkride's avatar gaze styles.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "GazeStyle.h"

#include <vector>

namespace {

// The styles. "calm" is the stock behaviour, unchanged: an avatar whose owner never picks a style looks as it did.
// The others' numbers are a first pass by eye, not a measurement.
const std::vector<GazeStyle>& table() {
    static const std::vector<GazeStyle> styles = [] {
        std::vector<GazeStyle> list;

        GazeStyle calm;
        calm.name = GazeStyle::defaultName();
        list.push_back(calm);

        GazeStyle excited;  // quick, wide, scanning; the head goes with the eyes; hardly blinks
        excited.name = "excited";
        excited.saccadeInterval = 0.6f;
        excited.saccadeMagnitude = 0.18f;
        excited.saccadeSpeed = 0.7f;
        excited.lookSeconds = 1.5f;
        excited.awaySeconds = 0.6f;
        excited.restPitch = 5.0f;
        excited.restYaw = 25.0f;
        excited.headFollow = 0.6f;
        excited.headPitch = 3.0f;
        excited.blinkOnGlance = false;
        excited.blinkInterval = 9.0f;
        excited.blinkSpeed = 1.5f;
        excited.lidsClosed = -0.3f;
        list.push_back(excited);

        GazeStyle shy;  // rests low and aside, short glances up at the other person, blinks more, head a little down
        shy.name = "shy";
        shy.saccadeInterval = 1.5f;
        shy.saccadeMagnitude = 0.07f;
        shy.saccadeSpeed = 0.4f;
        shy.lookSeconds = 0.7f;
        shy.awaySeconds = 3.0f;
        shy.restPitch = -25.0f;
        shy.restYaw = 20.0f;
        shy.headPitch = -8.0f;
        shy.blinkInterval = 2.5f;
        list.push_back(shy);

        GazeStyle sleepy;  // slow drifting eyes that sink, long slow blinks, half-shut lids, the head dips and recovers
        sleepy.name = "sleepy";
        sleepy.saccadeInterval = 4.0f;
        sleepy.saccadeMagnitude = 0.06f;
        sleepy.saccadeSpeed = 0.08f;
        sleepy.lookSeconds = 3.0f;
        sleepy.awaySeconds = 4.0f;
        sleepy.restPitch = -15.0f;
        sleepy.restYaw = 6.0f;
        sleepy.glanceSpeed = 0.05f;
        sleepy.headPitch = -6.0f;
        sleepy.nodPeriod = 7.0f;
        sleepy.nodDegrees = 10.0f;
        sleepy.blinkInterval = 3.0f;
        sleepy.blinkSpeed = 0.3f;
        sleepy.blinkHold = 0.35f;
        sleepy.lidsClosed = 0.4f;
        list.push_back(sleepy);

        GazeStyle intent;  // the stare: locked on, hardly a dart, hardly a blink, the head turns with it
        intent.name = "intent";
        intent.saccadeInterval = 12.0f;
        intent.saccadeMagnitude = 0.02f;
        intent.lookSeconds = 30.0f;
        intent.headFollow = 0.3f;
        intent.blinkOnGlance = false;
        intent.blinkInterval = 12.0f;
        list.push_back(intent);

        return list;
    }();
    return styles;
}

}  // namespace

bool GazeStyle::isKnown(const QString& name) {
    for (const auto& style : table()) {
        if (style.name == name) {
            return true;
        }
    }
    return false;
}

const QStringList& GazeStyle::names() {
    static const QStringList list = [] {
        QStringList names;
        for (const auto& style : table()) {
            names << style.name;
        }
        return names;
    }();
    return list;
}

const GazeStyle& GazeStyle::named(const QString& name) {
    for (const auto& style : table()) {
        if (style.name == name) {
            return style;
        }
    }
    return table().front();
}
