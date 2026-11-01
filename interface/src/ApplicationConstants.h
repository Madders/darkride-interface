//
//  ApplicationConstants.h
//  interface/src
//
//  The constants the Application*.cpp files define that the others read too: two setting keys (the active display
//  plug-in's name, kept at shutdown and read at the next start; "keep me logged in"), the default cursor's name, the
//  thread pool's floor, the avatar-inputs bar's QML and the system tablet's name.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_ApplicationConstants_h
#define hifi_ApplicationConstants_h

#include <QtCore/QString>
#include <QtCore/QUrl>

extern const QString ACTIVE_DISPLAY_PLUGIN_SETTING_NAME;   // Display.cpp; the shutdown writes it
extern const QString KEEP_ME_LOGGED_IN_SETTING_NAME;       // Application.cpp
extern const int MIN_PROCESSING_THREAD_POOL_SIZE;          // Application.cpp
extern const QUrl AVATAR_INPUTS_BAR_QML;                   // Application.cpp; the start-up's QML-surface operator reads it
extern const QString SYSTEM_TABLET;                        // ApplicationDialogs.cpp; ApplicationUi.cpp reads it too
extern const QString DEFAULT_CURSOR_NAME;                  // ApplicationStartup.cpp, with the other settings' defaults

#endif // hifi_ApplicationConstants_h
