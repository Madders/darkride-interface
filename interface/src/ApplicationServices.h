//
//  ApplicationServices.h
//  interface/src
//
//  See ApplicationServices.cpp: the client's services, registered in order.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_ApplicationServices_h
#define hifi_ApplicationServices_h

#include <QtCore/QString>

// Registers every service the client runs on, in the order the list gives. listenPort is the node list's port
// (INVALID_PORT lets it pick); defaultScriptsOverride is another defaultScripts.js, or empty.
void registerApplicationServices(int listenPort, const QString& defaultScriptsOverride);

extern const QString STATE_IN_HMD;
extern const QString STATE_CAMERA_FULL_SCREEN_MIRROR;
extern const QString STATE_CAMERA_FIRST_PERSON;
extern const QString STATE_CAMERA_FIRST_PERSON_LOOK_AT;
extern const QString STATE_CAMERA_THIRD_PERSON;
extern const QString STATE_CAMERA_ENTITY;
extern const QString STATE_CAMERA_INDEPENDENT;
extern const QString STATE_CAMERA_LOOK_AT;
extern const QString STATE_CAMERA_SELFIE;
extern const QString STATE_SNAP_TURN;
extern const QString STATE_ADVANCED_MOVEMENT_CONTROLS;
extern const QString STATE_GROUNDED;
extern const QString STATE_NAV_FOCUSED;
extern const QString STATE_PLATFORM_WINDOWS;
extern const QString STATE_PLATFORM_MAC;
extern const QString STATE_PLATFORM_ANDROID;
extern const QString STATE_LEFT_HAND_DOMINANT;
extern const QString STATE_RIGHT_HAND_DOMINANT;
extern const QString STATE_STRAFE_ENABLED;

#endif // hifi_ApplicationServices_h
