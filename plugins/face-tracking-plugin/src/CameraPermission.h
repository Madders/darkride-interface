//
//  CameraPermission.h
//  plugins/face-tracking-plugin/src
//
//  Whether this app may use the camera: macOS asks the user once (the app's NSCameraUsageDescription says why);
//  elsewhere it always may.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#pragma once

enum class CameraPermission {
    Granted,
    NotAsked,
    Denied,  // turned off in System Settings, or not allowed on this Mac
};

CameraPermission cameraPermission();

// shows the system's question, if the user hasn't been asked; cameraPermission() has the answer once they reply
void askCameraPermission();
