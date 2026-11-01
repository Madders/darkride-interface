//
//  QtScriptEditFilterEngine.h
//  libraries/entities/src
//
//  An edit filter's script in a QScriptEngine of its own, as the filters have run since 2017 (David Kelly's
//  EntityEditFilters), behind the EntityEditFilterEngine interface.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_QtScriptEditFilterEngine_h
#define hifi_QtScriptEditFilterEngine_h

#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>

#include "EntityEditFilterEngine.h"

class QtScriptEditFilterEngine : public EntityEditFilterEngine {
public:
    explicit QtScriptEditFilterEngine(const EntityItemID& entityID);
    ~QtScriptEditFilterEngine() override;

    bool load(const QString& source, const QString& url, QString& error) override;
    QVariantMap flags() override;
    Result call(EntityItemProperties& in, EntityItemProperties& out, int filterType,
                const EntityItemProperties* original, const EntityItemProperties* zone, const AABox* zoneBox) override;

private:
    /// Logs and clears an uncaught exception; true when there was one.
    bool hadUncaughtExceptions(QString& message);

    EntityItemID _entityID;
    QString _url;
    QScriptEngine* _engine { nullptr };
    QScriptValue _filterFn;
};

#endif // hifi_QtScriptEditFilterEngine_h
