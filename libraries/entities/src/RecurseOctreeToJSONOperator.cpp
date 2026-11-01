//
//  RecurseOctreeToJSONOperator.cpp
//  libraries/entities/src
//
//  Created by Simon Walton on Oct 11, 2018.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RecurseOctreeToJSONOperator.h"

#include <PlainScriptValue.h>

#include "EntityItemProperties.h"

RecurseOctreeToJSONOperator::RecurseOctreeToJSONOperator(const OctreeElementPointer&,
    QString jsonPrefix, bool skipDefaults, bool skipThoseWithBadParents):
    _json(jsonPrefix),
    _skipDefaults(skipDefaults),
    _skipThoseWithBadParents(skipThoseWithBadParents)
{
}

bool RecurseOctreeToJSONOperator::postRecursion(const OctreeElementPointer& element) {
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);

    entityTreeElement->forEachEntity([&](const EntityItemPointer& entity) { processEntity(entity); } );
    return true;
}

void RecurseOctreeToJSONOperator::processEntity(const EntityItemPointer& entity) {
    if (_skipThoseWithBadParents && !entity->isParentIDValid()) {
        return;  // we weren't able to resolve a parent from _parentID, so don't save this entity.
    }

    PlainScriptEngine engine;
    const PlainScriptValue values = entity->getProperties().copyToScriptValue(&engine, _skipDefaults);

    if (_comma) {
        _json += ',';
    };
    _comma = true;
    _json += "\n    ";

    // the text JSON.stringify(values, null, '    ') gives, which QtScript's own JSON.stringify wrote before
    _json += values.toJson("    ");
}
