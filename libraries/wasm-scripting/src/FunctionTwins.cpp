//
//  FunctionTwins.cpp
//  libraries/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "FunctionTwins.h"

#include <map>
#include <mutex>

namespace {

std::mutex& registryMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<const void*, FunctionTwins::Factory>& registry() {
    static std::map<const void*, FunctionTwins::Factory> twins;
    return twins;
}

} // namespace

void FunctionTwins::add(const void* function, Factory factory) {
    std::lock_guard<std::mutex> lock(registryMutex());
    registry()[function] = std::move(factory);
}

FunctionTwins::Factory FunctionTwins::find(const void* function) {
    std::lock_guard<std::mutex> lock(registryMutex());
    auto found = registry().find(function);
    return found == registry().end() ? Factory() : found->second;
}
