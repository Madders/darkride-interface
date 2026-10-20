//
//  Created by Bradley Austin Davis on 2019/09/05
//  Copyright 2013-2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_LocalFileAccessGate_h
#define hifi_LocalFileAccessGate_h

namespace hifi { namespace scripting {

void setLocalAccessSafeThread(bool safe = true);

bool isLocalAccessSafeThread();

// A thread running a world's scripts on the visitor's computer (entity scripts): APIs that act on the user's own
// installation (their scripts, settings, menus, files, account) check this and refuse, or keep to the world's own space
void setRestrictedScriptThread(bool restricted = true);

bool isRestrictedScriptThread();

}}  // namespace hifi::scripting

#endif
