//
//  SettingHelpers.h
//  libraries/shared/src
//
//  Created by Clement on 9/13/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SettingHelpers_h
#define hifi_SettingHelpers_h

#include <QSettings>

extern const QSettings::Format JSON_FORMAT;

QString settingsFilename();

bool readJSONFile(QIODevice& device, QSettings::SettingsMap& map);
bool writeJSONFile(QIODevice& device, const QSettings::SettingsMap& map);

void loadOldINIFile(QSettings& settings);

// A walled-off client or a sandboxed server (TIVOLI_APP_DATA_DIR set) keeps its settings file in its own folders too:
// XDG_CONFIG_HOME when that is set as well, else TIVOLI_APP_DATA_DIR. Qt reads XDG_CONFIG_HOME itself on macOS and
// Linux, but on Windows it asks the system for the Roaming folder, which no environment variable moves. Returns the
// folder it set, or an empty string where Qt's own choice already holds. Call it before the first QSettings.
QString setSettingsPathFromEnvironment();

#endif // hifi_SettingHelpers_h
