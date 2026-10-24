//
//  SignInGate.cpp
//  interface/src
//
//  The sign-in gate (see SignInGate.h). The code of Application's showLoginScreen, pauseUntilLoginDetermined,
//  resumeAfterLoginDialogActionTaken, createLoginDialog, updateLoginDialogPosition and onDismissedLoginDialog, of the
//  frame loop's step for the headset's dialog and of the start-up's handler for a display change, with Application
//  reached through qApp for the window, the view, the keyboard focus, the compositor, the controller interface, the
//  tablet mode, the refresh-rate manager, the scripts location and the test property.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "SignInGate.h"

#include <QtCore/QThread>

#include <AccountManager.h>
#include <AddressManager.h>
#include <controllers/UserInputMapper.h>
#include <display-plugins/CompositorHelper.h>
#include <EntityScriptingInterface.h>
#include <MainWindow.h>
#include <PathUtils.h>
#include <ScriptEngines.h>
#include <shared/GlobalAppProperties.h>
#include <StartupProfiler.h>

#include "Application.h"
#include "avatar/AvatarManager.h"
#include "ClientOptions.h"
#include "ui/Skins.h"
#include "ui/SystemToolbar.h"
#include "ui/DialogsManager.h"
#include "ui/Keyboard.h"
#include "ui/LoginDialog.h"

QString labelledTitle(const QString& title);   // Display.cpp's: a window title with the client label

// the mappings the pause swaps: the standard one off, one without movement on
static const QString STANDARD_TO_ACTION_MAPPING_NAME = "Standard to Action";
static const QString NO_MOVEMENT_MAPPING_NAME = "Standard to Action (No Movement)";
static const QString NO_MOVEMENT_MAPPING_JSON = PathUtils::resourcesPath() + "/controllers/standard_nomovement.json";

// Application's getMyAvatar is private; this is the same call
static std::shared_ptr<MyAvatar> getMyAvatar() {
    return DependencyManager::get<AvatarManager>()->getMyAvatar();
}

void SignInGate::showLoginScreen() {
    // Signed in or not, the sign-in screen comes first: a remembered account ("Remember me") gets "Continue as <name>"
    // there (LoginDialog/ContinueBody.qml), and goes in from it.
    auto dialogsManager = DependencyManager::get<DialogsManager>();
    if (!qApp->isHMDMode()) {
        SystemToolbar::instance()->setAllowed(false);   // either bar, floating or on a side
    }
    DependencyManager::get<Skins>()->setHeld(true);     // no skin on the sign-in screen

    _poppedUp = true;
    _poppedUpSetting.set(_poppedUp);
    dialogsManager->showLoginDialog();
    StartupProfiler::instant("sign-in dialog opened");
    emit loginDialogFocusEnabled();
    qApp->getWindow()->setWindowTitle(labelledTitle("Darkride"));
}

void SignInGate::pauseUntilLoginDetermined() {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, &SignInGate::pauseUntilLoginDetermined);
        return;
    }

    _signInPending = true;

    auto myAvatar = getMyAvatar();
    _previousAvatarTargetScale = myAvatar->getTargetScale();
    _previousAvatarSkeletonModel = myAvatar->getSkeletonModelURL().toString();
    myAvatar->setTargetScale(1.0f);
    myAvatar->setSkeletonModelURLFromScript(myAvatar->defaultFullAvatarModelUrl().toString());
    myAvatar->setEnableMeshVisible(false);

    qApp->getControllerScriptingInterface()->disableMapping(STANDARD_TO_ACTION_MAPPING_NAME);

    {
        auto userInputMapper = DependencyManager::get<UserInputMapper>();
        if (userInputMapper->loadMapping(NO_MOVEMENT_MAPPING_JSON)) {
            qApp->getControllerScriptingInterface()->enableMapping(NO_MOVEMENT_MAPPING_NAME);
        }
    }

    const auto& nodeList = DependencyManager::get<NodeList>();
    // save interstitial mode setting until resuming.
    _interstitialModeEnabled = nodeList->getDomainHandler().getInterstitialModeEnabled();
    nodeList->getDomainHandler().setInterstitialModeEnabled(false);

    auto menu = Menu::getInstance();
    menu->getMenu("Edit")->setVisible(false);
    menu->getMenu("View")->setVisible(false);
    menu->getMenu("Navigate")->setVisible(false);
    menu->getMenu("Settings")->setVisible(false);
    _developerMenuVisible = menu->getMenu("Developer")->isVisible();
    menu->setIsOptionChecked(MenuOption::Stats, false);
    if (_developerMenuVisible) {
        menu->getMenu("Developer")->setVisible(false);
    }
    qApp->getViewState().setModeKeepingPrevious(CAMERA_MODE_FIRST_PERSON_LOOK_AT);

    // disconnect domain handler.
    nodeList->getDomainHandler().disconnect("Pause until login determined");

    // From now on, it's permissible to call resumeAfterLoginDialogActionTaken()
    _resumeSafeToRun = true;

    if (_resumeWasPostponed) {
        // resumeAfterLoginDialogActionTaken() was already called, but it aborted. Now it's safe to call it again.
        resumeAfterLoginDialogActionTaken();
    }
}

void SignInGate::resumeAfterLoginDialogActionTaken() {
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, &SignInGate::resumeAfterLoginDialogActionTaken);
        return;
    }

    if (!_resumeSafeToRun) {
        _resumeWasPostponed = true;
        return;
    }

    // either bar, floating or on a side, may show now (each still waits for the desktop: none in VR), and the skin
    SystemToolbar::instance()->setAllowed(true);
    DependencyManager::get<Skins>()->setHeld(false);
    if (qApp->isHMDMode() || !qApp->getDesktopTabletBecomesToolbarSetting()) {
        qApp->getApplicationCompositor().getReticleInterface()->setAllowMouseCapture(true);
        qApp->getApplicationCompositor().getReticleInterface()->setVisible(true);
    }

    qApp->updateSystemTabletMode();

    {
        auto userInputMapper = DependencyManager::get<UserInputMapper>();
        userInputMapper->unloadMapping(NO_MOVEMENT_MAPPING_JSON);
        qApp->getControllerScriptingInterface()->disableMapping(NO_MOVEMENT_MAPPING_NAME);
    }

    auto myAvatar = getMyAvatar();
    myAvatar->setTargetScale(_previousAvatarTargetScale);
    myAvatar->setSkeletonModelURLFromScript(_previousAvatarSkeletonModel);
    myAvatar->setEnableMeshVisible(true);

    qApp->getControllerScriptingInterface()->enableMapping(STANDARD_TO_ACTION_MAPPING_NAME);

    const auto& nodeList = DependencyManager::get<NodeList>();
    nodeList->getDomainHandler().setInterstitialModeEnabled(_interstitialModeEnabled);
    {
        auto scriptEngines = DependencyManager::get<ScriptEngines>().data();
        // this will force the model the look at the correct directory (weird order of operations issue)
        scriptEngines->reloadLocalFiles();

        // if the --scripts command-line argument was used.
        if (qApp->getDefaultScriptsLocation().exists() && !ClientOptions::get().scriptsPath.isEmpty()) {
            scriptEngines->loadDefaultScripts();
            scriptEngines->defaultScriptsLocationOverridden(true);
        } else {
            scriptEngines->loadScripts();
        }
    }

    auto accountManager = DependencyManager::get<AccountManager>();
    auto addressManager = DependencyManager::get<AddressManager>();

    // restart domain handler.
    nodeList->getDomainHandler().resetting();

    QVariant testProperty = qApp->property(hifi::properties::TEST);
    if (testProperty.isValid()) {
        const auto testScript = qApp->property(hifi::properties::TEST).toUrl();
        // Set last parameter to exit interface when the test script finishes, if so requested
        DependencyManager::get<ScriptEngines>()->loadScript(testScript, false, false, false, false, ClientOptions::get().testQuitWhenFinished);
        // This is done so we don't get a "connection time-out" message when we haven't passed in a URL.
        if (ClientOptions::get().hasUrl) {
            QMetaObject::invokeMethod(&qApp->getDomainSession(), &DomainSession::goToStartingLocation, Qt::QueuedConnection);
        }
    } else {
        QMetaObject::invokeMethod(&qApp->getDomainSession(), &DomainSession::goToStartingLocation, Qt::QueuedConnection);
    }

    auto menu = Menu::getInstance();
    menu->getMenu("Edit")->setVisible(true);
    menu->getMenu("View")->setVisible(true);
    menu->getMenu("Navigate")->setVisible(true);
    menu->getMenu("Settings")->setVisible(true);
    menu->getMenu("Developer")->setVisible(_developerMenuVisible);
    qApp->getViewState().restorePreviousMode();
    _startUpFinished = true;
    StartupProfiler::instant("signed in: scripts loading, going to the starting location");
    qApp->getRefreshRateManager().setRefreshRateRegime(RefreshRateManager::RefreshRateRegime::FOCUS_ACTIVE);
    _signInPending = false;
}

void SignInGate::createLoginDialog() {
    const glm::vec3 LOGIN_DIMENSIONS { 0.89f, 0.5f, 0.01f };
    const auto OFFSET = glm::vec2(0.7f, -0.1f);
    auto cameraPosition = qApp->getViewState().camera().getPosition();
    auto cameraOrientation = qApp->getViewState().camera().getOrientation();
    auto upVec = getMyAvatar()->getWorldOrientation() * Vectors::UNIT_Y;
    auto headLookVec = (cameraOrientation * Vectors::FRONT);
    // DEFAULT_DPI / tablet scale percentage
    const float DPI = 31.0f / (75.0f / 100.0f);
    auto offset = headLookVec * OFFSET.x;
    auto position = (cameraPosition + offset) + (upVec * OFFSET.y);

    EntityItemProperties properties;
    properties.setType(EntityTypes::Web);
    properties.setName("LoginDialogEntity");
    properties.setSourceUrl(LOGIN_DIALOG.toString());
    properties.setPosition(position);
    properties.setRotation(cameraOrientation);
    properties.setDimensions(LOGIN_DIMENSIONS);
    properties.setPrimitiveMode(PrimitiveMode::SOLID);
    properties.getGrab().setGrabbable(false);
    properties.setIgnorePickIntersection(false);
    properties.setAlpha(1.0f);
    properties.setDPI(DPI);
    properties.setVisible(true);

    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    _loginDialogID = entityScriptingInterface->addEntityInternal(properties, entity::HostType::LOCAL);

    auto keyboard = DependencyManager::get<Keyboard>().data();
    if (!keyboard->getAnchorID().isNull() && !_loginDialogID.isNull()) {
        auto keyboardLocalOffset = cameraOrientation * glm::vec3(-0.4f * getMyAvatar()->getSensorToWorldScale(), -0.3f, 0.2f);

        EntityItemProperties properties;
        properties.setPosition(position + keyboardLocalOffset);
        properties.setRotation(cameraOrientation * Quaternions::Y_180);

        entityScriptingInterface->editEntity(keyboard->getAnchorID(), properties);
        keyboard->setResetKeyboardPositionOnRaise(false);
    }
    qApp->getKeyboardFocus().setFocusedEntity(_loginDialogID);
    emit loginDialogFocusEnabled();
    qApp->getApplicationCompositor().getReticleInterface()->setAllowMouseCapture(false);
    qApp->getApplicationCompositor().getReticleInterface()->setVisible(false);
    if (!_loginStateManager.isSetUp()) {
        _loginStateManager.setUp();
    }
}

void SignInGate::updateLoginDialogPosition() {
    const float LOOK_AWAY_THRESHOLD_ANGLE = 70.0f;
    const auto OFFSET = glm::vec2(0.7f, -0.1f);

    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    EntityPropertyFlags desiredProperties;
    desiredProperties += PROP_POSITION;
    auto properties = entityScriptingInterface->getEntityProperties(_loginDialogID, desiredProperties);
    auto positionVec = properties.getPosition();
    auto cameraPositionVec = qApp->getViewState().camera().getPosition();
    auto cameraOrientation = cancelOutRollAndPitch(qApp->getViewState().camera().getOrientation());
    auto headLookVec = (cameraOrientation * Vectors::FRONT);
    auto entityToHeadVec = positionVec - cameraPositionVec;
    auto pointAngle = (glm::acos(glm::dot(glm::normalize(entityToHeadVec), glm::normalize(headLookVec))) * 180.0f / PI);
    auto upVec = getMyAvatar()->getWorldOrientation() * Vectors::UNIT_Y;
    auto offset = headLookVec * OFFSET.x;
    auto newPositionVec = (cameraPositionVec + offset) + (upVec * OFFSET.y);

    bool outOfBounds = glm::distance(positionVec, cameraPositionVec) > 1.0f;

    if (pointAngle > LOOK_AWAY_THRESHOLD_ANGLE || outOfBounds) {
        {
            EntityItemProperties properties;
            properties.setPosition(newPositionVec);
            properties.setRotation(cameraOrientation);
            entityScriptingInterface->editEntity(_loginDialogID, properties);
        }

        {
            glm::vec3 keyboardLocalOffset = cameraOrientation * glm::vec3(-0.4f * getMyAvatar()->getSensorToWorldScale(), -0.3f, 0.2f);
            glm::quat keyboardOrientation = cameraOrientation * glm::quat(glm::radians(glm::vec3(-30.0f, 180.0f, 0.0f)));

            EntityItemProperties properties;
            properties.setPosition(newPositionVec + keyboardLocalOffset);
            properties.setRotation(keyboardOrientation);
            entityScriptingInterface->editEntity(DependencyManager::get<Keyboard>()->getAnchorID(), properties);
        }
    }
}

void SignInGate::onDismissedLoginDialog() {
    _poppedUp = false;
    _poppedUpSetting.set(false);
    auto keyboard = DependencyManager::get<Keyboard>().data();
    keyboard->setResetKeyboardPositionOnRaise(true);
    if (!_loginDialogID.isNull()) {
        DependencyManager::get<EntityScriptingInterface>()->deleteEntity(_loginDialogID);
        _loginDialogID = QUuid();
        _loginStateManager.tearDown();
    }
    resumeAfterLoginDialogActionTaken();
}

void SignInGate::update() {
    if (!_loginDialogID.isNull()) {
        _loginStateManager.update(getMyAvatar()->getDominantHand(), _loginDialogID);
        updateLoginDialogPosition();
    }
}

void SignInGate::displayPluginChanged() {
    if (!_poppedUp) {
        return;
    }
    auto dialogsManager = DependencyManager::get<DialogsManager>();
    if (qApp->isHMDMode()) {
        emit loginDialogFocusDisabled();
        dialogsManager->hideLoginDialog();
        createLoginDialog();
    } else {
        DependencyManager::get<EntityScriptingInterface>()->deleteEntity(_loginDialogID);
        _loginDialogID = QUuid();
        _loginStateManager.tearDown();
        dialogsManager->showLoginDialog();
        emit loginDialogFocusEnabled();
    }
}

void SignInGate::shutdown() {
    _poppedUpSetting.set(false);
}
