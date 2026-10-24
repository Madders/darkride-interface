//
//  ApplicationMeshProvider.h
//  interface/src
//
//  The mesh provider the graphics scripting interface asks for a model by id: an entity's renderer or an avatar,
//  found through the spatial parent finder. Out of Application.cpp, where it was defined beside the registrations.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_ApplicationMeshProvider_h
#define hifi_ApplicationMeshProvider_h

#include <graphics-scripting/Forward.h>
#include <EntityTreeRenderer.h>
#include <SpatialParentFinder.h>

#include "Application.h"
#include "InterfaceLogging.h"
#include "avatar/AvatarManager.h"

class ApplicationMeshProvider : public scriptable::ModelProviderFactory  {
public:
    virtual scriptable::ModelProviderPointer lookupModelProvider(const QUuid& uuid) override {
        bool success;
        if (auto nestable = DependencyManager::get<SpatialParentFinder>()->find(uuid, success).lock()) {
            auto type = nestable->getNestableType();
#ifdef SCRIPTABLE_MESH_DEBUG
            qCDebug(interfaceapp) << "ApplicationMeshProvider::lookupModelProvider" << uuid << SpatiallyNestable::nestableTypeToString(type);
#endif
            switch (type) {
            case NestableType::Entity:
                return getEntityModelProvider(static_cast<EntityItemID>(uuid));
            case NestableType::Avatar:
                return getAvatarModelProvider(uuid);
            }
        }
        return nullptr;
    }

private:
    scriptable::ModelProviderPointer getEntityModelProvider(EntityItemID entityID) {
        scriptable::ModelProviderPointer provider;
        auto entityTreeRenderer = qApp->getEntities();
        auto entityTree = entityTreeRenderer->getTree();
        if (auto entity = entityTree->findEntityByID(entityID)) {
            if (auto renderer = entityTreeRenderer->renderableForEntityId(entityID)) {
                provider = std::dynamic_pointer_cast<scriptable::ModelProvider>(renderer);
                provider->modelProviderType = NestableType::Entity;
            } else {
                qCWarning(interfaceapp) << "no renderer for entity ID" << entityID.toString();
            }
        }
        return provider;
    }

    scriptable::ModelProviderPointer getAvatarModelProvider(QUuid sessionUUID) {
        scriptable::ModelProviderPointer provider;
        auto avatarManager = DependencyManager::get<AvatarManager>();
        if (auto avatar = avatarManager->getAvatarBySessionID(sessionUUID)) {
            provider = std::dynamic_pointer_cast<scriptable::ModelProvider>(avatar);
            provider->modelProviderType = NestableType::Avatar;
        }
        return provider;
    }
};

#endif // hifi_ApplicationMeshProvider_h
