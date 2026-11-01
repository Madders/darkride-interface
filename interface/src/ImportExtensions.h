//
//  ImportExtensions.h
//  interface/src
//
//  The file extensions the client accepts for import, by drop, URL or dialog (Application.cpp defines them and keys
//  the table of importers by them; AssetImport.cpp reads them).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_ImportExtensions_h
#define hifi_ImportExtensions_h

#include <QtCore/QString>

extern const QString SNAPSHOT_EXTENSION;
extern const QString JPG_EXTENSION;
extern const QString PNG_EXTENSION;
extern const QString SVO_EXTENSION;
extern const QString SVO_JSON_EXTENSION;
extern const QString JSON_EXTENSION;
extern const QString JS_EXTENSION;
extern const QString FST_EXTENSION;
extern const QString FBX_EXTENSION;
extern const QString OBJ_EXTENSION;
extern const QString AVA_JSON_EXTENSION;
extern const QString ZIP_EXTENSION;

#endif // hifi_ImportExtensions_h
