//
//  GazeStyle.h
//  libraries/avatars/src
//
//  Created on 2026-10-15 for Darkride's avatar gaze styles.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_GazeStyle_h
#define hifi_GazeStyle_h

#include <QtCore/QString>
#include <QtCore/QStringList>

// How an avatar's eyes behave: how the gaze darts, where it rests, how much it meets other people's eyes, what the
// head does with it, and how the lids blink. An avatar wears one named style at a time. Its owner picks it (the
// gazeStyle property), it travels with the avatar's menu parameters, and every client plays it on the avatar it sees:
// the darts and blinks are made by each viewer for itself, the look-at point by the owner.
struct GazeStyle {
    QString name;

    // the darts (saccades), made by every viewer for itself
    float saccadeInterval { 2.5f };   // seconds between darts, on average
    float saccadeMagnitude { 0.1f };  // a dart's size, sideways, as a fraction of the look direction: 0.1 is about 6 degrees
    float saccadeSpeed { 0.5f };      // the share of the way to a dart's target covered per frame at 60 Hz: 0.5 snaps, 0.05 drifts

    // where the gaze rests, chosen by the owner's client as the look-at point everyone sees
    float lookSeconds { 6.0f };       // how long a look at someone's eyes (or straight ahead) lasts, on average
    float awaySeconds { 0.0f };       // how long the eyes rest elsewhere between looks (0: never look away)
    float restPitch { 0.0f };         // degrees the resting gaze sits below (negative) or above level
    float restYaw { 0.0f };           // degrees it sits to one side or the other
    float glanceSpeed { 1.0f };       // the share of the way to a new look-at point covered per frame at 60 Hz: 1 jumps, 0.05 drifts

    // the head, on the owner's client, where the camera mode lets the head follow the look-at
    float headFollow { 0.0f };        // 0..1: how far the head turns toward where the eyes look
    float headPitch { 0.0f };         // degrees the head is held down (negative) or up
    float nodPeriod { 0.0f };         // seconds for one slow dip and recovery of the head (0: none)
    float nodDegrees { 0.0f };        // how deep the dip goes

    // the lids, on avatars with eyelid blend shapes
    bool blinkOnGlance { true };      // a jump of the gaze over 20 degrees waits for a blink, as people's often do
    float blinkInterval { 4.0f };     // seconds between blinks, on average (0: never)
    float blinkSpeed { 1.0f };        // 1 is a normal blink, 0.3 a slow one
    float blinkHold { 0.0f };         // seconds the lids stay shut in a blink
    float lidsClosed { 0.0f };        // 0..1: how far the lids sit shut at rest; negative holds them wide open

    static QString defaultName() { return QStringLiteral("calm"); }
    static QString traitKey() { return QStringLiteral("@gaze"); }  // its key beside the menu parameters on the wire
    static bool isKnown(const QString& name);
    static const QStringList& names();
    static const GazeStyle& named(const QString& name);  // the default style for an empty or unknown name
};

#endif // hifi_GazeStyle_h
