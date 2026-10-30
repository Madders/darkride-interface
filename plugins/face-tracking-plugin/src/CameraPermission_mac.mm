//
//  CameraPermission_mac.mm
//  plugins/face-tracking-plugin/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "CameraPermission.h"

#import <AVFoundation/AVFoundation.h>

CameraPermission cameraPermission() {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
        case AVAuthorizationStatusAuthorized:
            return CameraPermission::Granted;
        case AVAuthorizationStatusNotDetermined:
            return CameraPermission::NotAsked;
        default:
            return CameraPermission::Denied;
    }
}

void askCameraPermission() {
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL) {}];
}
