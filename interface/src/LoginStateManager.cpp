//
//  LoginStateManager.cpp
//  interface/src
//
//  Created by Wayne Chen on 11/5/18.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LoginStateManager.h"

#include <QtCore/QString>
#include <QtCore/QVariantMap>

#include <PathUtils.h>
#include <plugins/PluginUtils.h>
#include <RegisteredMetaTypes.h>

#include "controllers/StateController.h"
#include "controllers/UserInputMapper.h"
#include "raypick/PointerScriptingInterface.h"
#include "raypick/RayPickScriptingInterface.h"
#include "raypick/PickScriptingInterface.h"
#include "scripting/ControllerScriptingInterface.h"

// the in-world lasers' look (Caity 10-28, look A, scripts/system/libraries/pointersUtils.js): a fine cyan tube that fades
// out and a soft dot where it hits. The sign-in screen's two pointers come before those scripts run, so they're made here;
// they were still High Fidelity's red, blue and pink (Caity's bug batch, 10-29)
static const QVariantMap LASER_COLOR = {{"red", 0}, {"green", 175}, {"blue", 255}};
static const QVariantMap DOT_COLOR = {{"red", 150}, {"green", 220}, {"blue", 255}};   // lighter, so its centre reads white-hot
static const float DOT_SIZE = 0.015f;   // a metre away (the end scales with distance)

static const int DEFAULT_SEARCH_SPHERE_DISTANCE = 1000; // how far from camera to search intersection?

// the ray steadied as the in-world lasers' is (controllerDispatcher.js): a One Euro filter on its direction
static const QVariantMap LASER_SMOOTHING = {{"minCutoff", 1.0f}, {"beta", 1.5f}};

static QVariantMap laserPath() {
    return {
        {"type", "line3d"},
        {"color", LASER_COLOR},
        {"visible", true},
        {"alpha", 1.0f},
        {"solid", true},
        {"glow", 1.0f},
        {"faceCamera", true},
        {"isUVModeStretch", true},
        {"textures", PathUtils::resourcesUrl("images/lasers/fade.png").toString()},
        {"strokeWidths", QVariantList({0.006f, 0.0045f, 0.003f})},
        {"ignoreRayIntersection", true}, // always ignore this
        {"drawInFront", true}, // Even when burried inside of something, show it.
        {"drawHUDLayer", false}
    };
}

static QVariantMap laserDot() {
    return {
        {"type", "image3d"},
        {"url", PathUtils::resourcesUrl("images/lasers/dot.png").toString()},
        {"dimensions", QVariantMap({{"x", DOT_SIZE}, {"y", DOT_SIZE}, {"z", 0.001f}})},
        {"color", DOT_COLOR},
        {"alpha", 1.0f},
        {"emissive", true},
        {"billboardMode", "full"},
        {"ignoreRayIntersection", true},
        {"drawInFront", true}, // Even when burried inside of something, show it.
        {"drawHUDLayer", false},
        {"visible", true}
    };
}



void LoginStateManager::tearDown() {
    auto pointers = DependencyManager::get<PointerManager>().data();
    if (pointers) {
        if (_leftLoginPointerID > PointerEvent::INVALID_POINTER_ID) {
            pointers->removePointer(_leftLoginPointerID);
            _leftLoginPointerID = PointerEvent::INVALID_POINTER_ID;
        }
        if (_rightLoginPointerID > PointerEvent::INVALID_POINTER_ID) {
            pointers->removePointer(_rightLoginPointerID);
            _rightLoginPointerID = PointerEvent::INVALID_POINTER_ID;
        }
    }
}

void LoginStateManager::setUp() {
    // "full" is the one the sign-in screen shows, while a hand points at it: the line and the dot
    const QVariantMap halfPathRenderState = laserPath();
    const QVariantMap halfEndRenderState = laserDot();
    const QVariantMap fullPathRenderState = laserPath();
    const QVariantMap fullEndRenderState = laserDot();
    const QVariantMap holdPathRenderState = laserPath();
    QVariantMap halfRenderStateIdentifier {
        {"name", "half"},
        {"path", halfPathRenderState},
        {"end", halfEndRenderState}
    };
    QVariantMap fullRenderStateIdentifier {
        {"name", "full"},
        {"path", fullPathRenderState},
        {"end", fullEndRenderState}
    };
    QVariantMap holdRenderStateIdentifier {
        {"name", "hold"},
        {"path", holdPathRenderState},
    };

    QVariantMap halfDefaultRenderStateIdentifier {
        {"name", "half"},
        {"distance", DEFAULT_SEARCH_SPHERE_DISTANCE},
        {"path", halfPathRenderState}
    };
    QVariantMap fullDefaultRenderStateIdentifier {
        {"name", "full"},
        {"distance", DEFAULT_SEARCH_SPHERE_DISTANCE},
        {"path", fullPathRenderState}
    };
    QVariantMap holdDefaultRenderStateIdentifier {
        {"name", "hold"},
        {"distance", DEFAULT_SEARCH_SPHERE_DISTANCE},
        {"path", holdPathRenderState}
    };

    _renderStates = QList<QVariant>({halfRenderStateIdentifier, fullRenderStateIdentifier, holdRenderStateIdentifier});
    _defaultRenderStates = QList<QVariant>({halfDefaultRenderStateIdentifier, fullDefaultRenderStateIdentifier, holdDefaultRenderStateIdentifier});

    auto pointers = DependencyManager::get<PointerScriptingInterface>();
    auto controller = DependencyManager::get<controller::ScriptingInterface>();

    const glm::vec3 grabPointSphereOffsetLeft { -0.04f, 0.13f, 0.039f };  // x = upward, y = forward, z = lateral
    const glm::vec3 grabPointSphereOffsetRight { 0.04f, 0.13f, 0.039f };  // x = upward, y = forward, z = lateral
    const glm::vec3 malletOffset {glm::vec3(0.0f, 0.18f - 0.050f, 0.0f)};

    QList<QVariant> leftPointerTriggerProperties;
    QVariantMap ltClick1 {
        { "action", controller->getStandard()["LTClick"] },
        { "button", "Focus" }
    };
    QVariantMap ltClick2 {
        { "action", controller->getStandard()["LTClick"] },
        { "button", "Primary" }
    };

    leftPointerTriggerProperties = QList<QVariant>({ltClick1, ltClick2});
    const unsigned int leftHand = 0;
    QVariantMap leftPointerProperties {
        { "joint", "_CAMERA_RELATIVE_CONTROLLER_LEFTHAND" },
        { "filter", PickScriptingInterface::PICK_LOCAL_ENTITIES() },
        { "triggers", leftPointerTriggerProperties },
        { "posOffset", vec3toVariant(grabPointSphereOffsetLeft + malletOffset) },
        { "hover", true },
        { "scaleWithParent", true },
        { "distanceScaleEnd", true },
        { "hand", leftHand },
        { "smoothing", LASER_SMOOTHING }
    };
    leftPointerProperties["renderStates"] = _renderStates;
    leftPointerProperties["defaultRenderStates"] = _defaultRenderStates;
    _leftLoginPointerID = pointers->createPointer(PickQuery::PickType::Ray, leftPointerProperties);
    pointers->setRenderState(_leftLoginPointerID, "");
    pointers->enablePointer(_leftLoginPointerID);
    const unsigned int rightHand = 1;
    QList<QVariant> rightPointerTriggerProperties;

    QVariantMap rtClick1 {
        { "action", controller->getStandard()["RTClick"] },
        { "button", "Focus" }
    };
    QVariantMap rtClick2 {
        { "action", controller->getStandard()["RTClick"] },
        { "button", "Primary" }
    };
    rightPointerTriggerProperties = QList<QVariant>({rtClick1, rtClick2});
    QVariantMap rightPointerProperties{
        { "joint", "_CAMERA_RELATIVE_CONTROLLER_RIGHTHAND" },
        { "filter", PickScriptingInterface::PICK_LOCAL_ENTITIES() },
        { "triggers", rightPointerTriggerProperties },
        { "posOffset", vec3toVariant(grabPointSphereOffsetRight + malletOffset) },
        { "hover", true },
        { "scaleWithParent", true },
        { "distanceScaleEnd", true },
        { "hand", rightHand },
        { "smoothing", LASER_SMOOTHING }
    };
    rightPointerProperties["renderStates"] = _renderStates;
    rightPointerProperties["defaultRenderStates"] = _defaultRenderStates;
    _rightLoginPointerID = pointers->createPointer(PickQuery::PickType::Ray, rightPointerProperties);
    pointers->setRenderState(_rightLoginPointerID, "");
    pointers->enablePointer(_rightLoginPointerID);
}

void LoginStateManager::update(const QString& dominantHand, const QUuid& loginEntityID) {
    if (!isSetUp()) {
        return;
    }
    if (_dominantHand != dominantHand) {
        _dominantHand = dominantHand;
    }
    auto pointers = DependencyManager::get<PointerScriptingInterface>();
    auto raypicks = DependencyManager::get<RayPickScriptingInterface>();
    if (pointers && raypicks) {
        const auto rightObjectID = raypicks->getPrevRayPickResult(_rightLoginPointerID)["objectID"].toUuid();
        const auto leftObjectID = raypicks->getPrevRayPickResult(_leftLoginPointerID)["objectID"].toUuid();
        const QString leftMode = (leftObjectID.isNull() || leftObjectID != loginEntityID) ? "" : "full";
        const QString rightMode = (rightObjectID.isNull() || rightObjectID != loginEntityID) ? "" : "full";
        pointers->setRenderState(_leftLoginPointerID, leftMode);
        pointers->setRenderState(_rightLoginPointerID, rightMode);
        if (_dominantHand == "left" && !leftObjectID.isNull()) {
            // dominant is left.
            pointers->setRenderState(_rightLoginPointerID, "");
            pointers->setRenderState(_leftLoginPointerID, leftMode);
        } else if (_dominantHand == "right" && !rightObjectID.isNull()) {
            // dominant is right.
            pointers->setRenderState(_leftLoginPointerID, "");
            pointers->setRenderState(_rightLoginPointerID, rightMode);
        }
    }
}
