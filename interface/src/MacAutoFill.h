//
//  MacAutoFill.h
//  interface/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_MacAutoFill_h
#define hifi_MacAutoFill_h

// Turns macOS's AutoFill off for this process, before the application starts (bug #29). Each time the app became
// active, AutoFill started a com.apple.SafariPlatformSupport.Helper that outlived the client; 422 of them used up
// launchservicesd's threads and brought a Mac down (2026-10-25). Registered as a default, so a user's own
// `defaults write` or a -NSAutoFillHeuristicsEnabled YES argument still wins.
void disableMacAutoFill();

#endif // hifi_MacAutoFill_h
