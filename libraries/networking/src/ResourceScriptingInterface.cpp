//
//  Created by Bradley Austin Davis on 2015/12/29
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ResourceScriptingInterface.h"

#include "ResourceCache.h"
#include "ResourceManager.h"

void ResourceScriptingInterface::overrideUrlPrefix(const QString& prefix, const QString& replacement) {
    DependencyManager::get<ResourceManager>()->setUrlPrefixOverride(prefix, replacement);
}

QVariantList ResourceScriptingInterface::getFailures() const {
    QVariantList list;
    for (const auto& failure : ResourceCache::failures()) {
        QVariantMap entry;
        entry["url"] = failure.url.toString();
        entry["type"] = failure.type;
        entry["reason"] = failure.reason;
        entry["time"] = (double)failure.time;
        list << entry;
    }
    return list;
}

void ResourceScriptingInterface::clearFailures() {
    ResourceCache::clearFailures();
}
