//
//  MacAutoFill.mm
//  interface/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include <QtGlobal>
#ifdef Q_OS_MACOS

#include "MacAutoFill.h"

#import <Foundation/Foundation.h>

void disableMacAutoFill() {
    @autoreleasepool {
        [[NSUserDefaults standardUserDefaults] registerDefaults:@{ @"NSAutoFillHeuristicsEnabled": @NO }];
    }
}

#endif // Q_OS_MACOS
