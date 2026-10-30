//
//  ScriptSettingsInterface.h
//
//  Created by scripting on 2026-10-30.
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//
//  The Settings object a script engine gets: the client's settings through one object per engine, which knows where
//  the engine's scripts came from. A script from the network (a world's script, by the network rule for a script's
//  own requests, ScriptNetworkPolicy) may not write the settings that decide what the client runs at start, where it
//  goes, whose avatar it wears, what it is allowed to do, or where its files are; a local script, or any script of a
//  local world, writes as before. Reads are unchanged here (a settings space per world is the next step).
//

#ifndef hifi_ScriptSettingsInterface_h
#define hifi_ScriptSettingsInterface_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

class ScriptEngine;

class ScriptSettingsInterface : public QObject {
    Q_OBJECT
public:
    explicit ScriptSettingsInterface(ScriptEngine* engine);

    /// Whether a script from the network may not write this setting.
    static bool isProtected(const QString& setting);

public slots:
    QVariant getValue(const QString& setting);
    QVariant getValue(const QString& setting, const QVariant& defaultValue);
    void setValue(const QString& setting, const QVariant& value);

signals:
    void valueChanged(const QString& setting, const QVariant& value);

private:
    bool refuses(const QString& setting) const;

    ScriptEngine* _engine { nullptr };
};

#endif // hifi_ScriptSettingsInterface_h
