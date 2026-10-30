//
//  CameraPermission.cpp
//  plugins/face-tracking-plugin/src
//
//  Where the system doesn't ask (Windows has no prompt for desktop apps): the camera is the app's to try; if the
//  system's privacy settings block it, opening it fails and the tracker says so.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "CameraPermission.h"

#ifndef __APPLE__  // CameraPermission_mac.mm

CameraPermission cameraPermission() {
    return CameraPermission::Granted;
}

void askCameraPermission() {
}

#endif
