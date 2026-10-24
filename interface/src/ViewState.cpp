//
//  ViewState.cpp
//  interface/src
//
//  The camera and what it sees (see ViewState.h). The code of Application's cycleCamera, cameraModeChanged,
//  changeViewAsNeeded, cameraMenuChanged, rotationModeChanged, computePickRay, copyViewFrustum and
//  copyDisplayViewFrustum, of the frame loop's updateCamera, its view-frustum step and
//  updateSecondaryCameraViewFrustum, and of the sign-in gate's save and restore of the mode, with Application reached
//  through qApp for the headset mode, the display plug-in, the compositor, the canvas, the entities and the graphics
//  engine.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "ViewState.h"

#include <glm/gtx/transform.hpp>   // the one-argument translate and scale

#include <controllers/UserInputMapper.h>
#include <display-plugins/CompositorHelper.h>
#include <EntityScriptingInterface.h>
#include <SecondaryCamera.h>

#include "Application.h"
#include "avatar/AvatarManager.h"
#include "avatar/MyHead.h"
#include "editor/Editor.h"
#include "LODManager.h"

static const float MIRROR_FULLSCREEN_DISTANCE = 0.789f;

// Application's getMyAvatar is private; this is the same call
static std::shared_ptr<MyAvatar> getMyAvatar() {
    return DependencyManager::get<AvatarManager>()->getMyAvatar();
}

void ViewState::copyViewFrustum(ViewFrustum& viewOut) const {
    QMutexLocker viewLocker(&_mutex);
    viewOut = _viewFrustum;
}

void ViewState::copyDisplayViewFrustum(ViewFrustum& viewOut) const {
    QMutexLocker viewLocker(&_mutex);
    viewOut = _displayViewFrustum;
}

PickRay ViewState::computePickRay(float x, float y) const {
    vec2 pickPoint { x, y };
    PickRay result;
    if (qApp->isHMDMode()) {
        qApp->getApplicationCompositor().computeHmdPickRay(pickPoint, result.origin, result.direction);
    } else {
        pickPoint /= qApp->getCanvasSize();
        if (_camera.getMode() == CameraMode::CAMERA_MODE_MIRROR) {
            pickPoint.x = 1.0f - pickPoint.x;
        }
        QMutexLocker viewLocker(&_mutex);
        _viewFrustum.computePickRay(pickPoint.x, pickPoint.y, result.origin, result.direction);
    }
    return result;
}

void ViewState::loadViewFrustum() {
    QMutexLocker viewLocker(&_mutex);
    _camera.loadViewFrustum(_viewFrustum);
}

void ViewState::setProjection(const glm::mat4& projection) {
    QMutexLocker viewLocker(&_mutex);
    _viewFrustum.setProjection(projection);
    _viewFrustum.calculate();
}

void ViewState::loadDisplayViewFrustum() {
    QMutexLocker viewLocker(&_mutex);
    _camera.loadViewFrustum(_displayViewFrustum);
}

void ViewState::update() {
    // Update _viewFrustum with latest camera and view frustum data...
    // NOTE: we get this from the view frustum, to make it simpler, since the
    // loadViewFrumstum() method will get the correct details from the camera
    // We could optimize this to not actually load the viewFrustum, since we don't
    // actually need to calculate the view frustum planes to send these details
    // to the server.
    QMutexLocker viewLocker(&_mutex);
    _camera.loadViewFrustum(_viewFrustum);
    // where a model shown as its impostor card fetches its mesh (ENTITY-STREAMING.md 3.1)
    qApp->getEntities()->setImpostorLoadView(_viewFrustum.getPosition(),
                                             DependencyManager::get<LODManager>()->getImpostorLoadHalfTanSq());

    _conicalViews.clear();
    _conicalViews.push_back(_viewFrustum);
    // TODO: Fix this by modeling the way the secondary camera works on how the main camera works
    // ie. Use a camera object stored in the game logic and informs the Engine on where the secondary
    // camera should be.
    addSecondaryCameraView();
}

void ViewState::addSecondaryCameraView() {
    // TODO: Fix this by modeling the way the secondary camera works on how the main camera works
    // ie. Use a camera object stored in the game logic and informs the Engine on where the secondary
    // camera should be.

    // Code based on SecondaryCameraJob
    auto renderConfig = qApp->getGraphicsEngine().getRenderEngine()->getConfiguration();
    assert(renderConfig);
    auto camera = dynamic_cast<SecondaryCameraJobConfig*>(renderConfig->getConfig("SecondaryCamera"));

    if (!camera || !camera->isEnabled()) {
        return;
    }

    ViewFrustum secondaryViewFrustum;
    if (camera->portalProjection && !camera->attachedEntityId.isNull() && !camera->portalEntranceEntityId.isNull()) {
        auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
        EntityItemPointer portalEntrance = qApp->getEntities()->getTree()->findEntityByID(camera->portalEntranceEntityId);
        EntityItemPointer portalExit = qApp->getEntities()->getTree()->findEntityByID(camera->attachedEntityId);

        glm::vec3 portalEntrancePropertiesPosition = portalEntrance->getWorldPosition();
        glm::quat portalEntrancePropertiesRotation = portalEntrance->getWorldOrientation();
        glm::mat4 worldFromPortalEntranceRotation = glm::mat4_cast(portalEntrancePropertiesRotation);
        glm::mat4 worldFromPortalEntranceTranslation = glm::translate(portalEntrancePropertiesPosition);
        glm::mat4 worldFromPortalEntrance = worldFromPortalEntranceTranslation * worldFromPortalEntranceRotation;
        glm::mat4 portalEntranceFromWorld = glm::inverse(worldFromPortalEntrance);

        glm::vec3 portalExitPropertiesPosition = portalExit->getWorldPosition();
        glm::quat portalExitPropertiesRotation = portalExit->getWorldOrientation();
        glm::vec3 portalExitPropertiesDimensions = portalExit->getScaledDimensions();
        glm::vec3 halfPortalExitPropertiesDimensions = 0.5f * portalExitPropertiesDimensions;

        glm::mat4 worldFromPortalExitRotation = glm::mat4_cast(portalExitPropertiesRotation);
        glm::mat4 worldFromPortalExitTranslation = glm::translate(portalExitPropertiesPosition);
        glm::mat4 worldFromPortalExit = worldFromPortalExitTranslation * worldFromPortalExitRotation;

        glm::vec3 mainCameraPositionWorld = _camera.getPosition();
        glm::vec3 mainCameraPositionPortalEntrance = vec3(portalEntranceFromWorld * vec4(mainCameraPositionWorld, 1.0f));
        mainCameraPositionPortalEntrance = vec3(-mainCameraPositionPortalEntrance.x, mainCameraPositionPortalEntrance.y,
            -mainCameraPositionPortalEntrance.z);
        glm::vec3 portalExitCameraPositionWorld = vec3(worldFromPortalExit * vec4(mainCameraPositionPortalEntrance, 1.0f));

        secondaryViewFrustum.setPosition(portalExitCameraPositionWorld);
        secondaryViewFrustum.setOrientation(portalExitPropertiesRotation);

        float nearClip = mainCameraPositionPortalEntrance.z + portalExitPropertiesDimensions.z * 2.0f;
        // `mainCameraPositionPortalEntrance` should technically be `mainCameraPositionPortalExit`,
        // but the values are the same.
        glm::vec3 upperRight = halfPortalExitPropertiesDimensions - mainCameraPositionPortalEntrance;
        glm::vec3 bottomLeft = -halfPortalExitPropertiesDimensions - mainCameraPositionPortalEntrance;
        glm::mat4 frustum = glm::frustum(bottomLeft.x, upperRight.x, bottomLeft.y, upperRight.y, nearClip, camera->farClipPlaneDistance);
        secondaryViewFrustum.setProjection(frustum);
    } else if (camera->mirrorProjection && !camera->attachedEntityId.isNull()) {
        auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
        auto entityProperties = entityScriptingInterface->getEntityProperties(camera->attachedEntityId);
        glm::vec3 mirrorPropertiesPosition = entityProperties.getPosition();
        glm::quat mirrorPropertiesRotation = entityProperties.getRotation();
        glm::vec3 mirrorPropertiesDimensions = entityProperties.getDimensions();
        glm::vec3 halfMirrorPropertiesDimensions = 0.5f * mirrorPropertiesDimensions;

        // setup mirror from world as inverse of world from mirror transformation using inverted x and z for mirrored image
        // TODO: we are assuming here that UP is world y-axis
        glm::mat4 worldFromMirrorRotation = glm::mat4_cast(mirrorPropertiesRotation) * glm::scale(vec3(-1.0f, 1.0f, -1.0f));
        glm::mat4 worldFromMirrorTranslation = glm::translate(mirrorPropertiesPosition);
        glm::mat4 worldFromMirror = worldFromMirrorTranslation * worldFromMirrorRotation;
        glm::mat4 mirrorFromWorld = glm::inverse(worldFromMirror);

        // get mirror camera position by reflecting main camera position's z coordinate in mirror space
        glm::vec3 mainCameraPositionWorld = _camera.getPosition();
        glm::vec3 mainCameraPositionMirror = vec3(mirrorFromWorld * vec4(mainCameraPositionWorld, 1.0f));
        glm::vec3 mirrorCameraPositionMirror = vec3(mainCameraPositionMirror.x, mainCameraPositionMirror.y,
                                                    -mainCameraPositionMirror.z);
        glm::vec3 mirrorCameraPositionWorld = vec3(worldFromMirror * vec4(mirrorCameraPositionMirror, 1.0f));

        // set frustum position to be mirrored camera and set orientation to mirror's adjusted rotation
        glm::quat mirrorCameraOrientation = glm::quat_cast(worldFromMirrorRotation);
        secondaryViewFrustum.setPosition(mirrorCameraPositionWorld);
        secondaryViewFrustum.setOrientation(mirrorCameraOrientation);

        // build frustum using mirror space translation of mirrored camera
        float nearClip = mirrorCameraPositionMirror.z + mirrorPropertiesDimensions.z * 2.0f;
        glm::vec3 upperRight = halfMirrorPropertiesDimensions - mirrorCameraPositionMirror;
        glm::vec3 bottomLeft = -halfMirrorPropertiesDimensions - mirrorCameraPositionMirror;
        glm::mat4 frustum = glm::frustum(bottomLeft.x, upperRight.x, bottomLeft.y, upperRight.y, nearClip, camera->farClipPlaneDistance);
        secondaryViewFrustum.setProjection(frustum);
    } else {
        if (!camera->attachedEntityId.isNull()) {
            auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
            auto entityProperties = entityScriptingInterface->getEntityProperties(camera->attachedEntityId);
            secondaryViewFrustum.setPosition(entityProperties.getPosition());
            secondaryViewFrustum.setOrientation(entityProperties.getRotation());
        } else {
            secondaryViewFrustum.setPosition(camera->position);
            secondaryViewFrustum.setOrientation(camera->orientation);
        }

        float aspectRatio = (float)camera->textureWidth / (float)camera->textureHeight;
        secondaryViewFrustum.setProjection(camera->vFoV,
                                            aspectRatio,
                                            camera->nearClipPlaneDistance,
                                            camera->farClipPlaneDistance);
    }
    // Without calculating the bound planes, the secondary camera will use the same culling frustum as the main camera,
    // which is not what we want here.
    secondaryViewFrustum.calculate();

    _conicalViews.push_back(secondaryViewFrustum);
}

void ViewState::updateCamera(RenderArgs& renderArgs, float deltaTime) {
    PROFILE_RANGE(render, __FUNCTION__);
    PerformanceTimer perfTimer("updateCamera");

    glm::vec3 boomOffset;
    auto myAvatar = getMyAvatar();
    boomOffset = myAvatar->getModelScale() * myAvatar->getBoomLength() * -IDENTITY_FORWARD;

    // The render mode is default or mirror if the camera is in mirror mode, assigned further below
    renderArgs._renderMode = RenderArgs::DEFAULT_RENDER_MODE;

    // build mode drives the camera itself, on CAMERA_MODE_INDEPENDENT
    {
        auto editor = DependencyManager::get<Editor>();
        if (editor && editor->isActive()) {
            editor->update(deltaTime);
        }
    }

    // Always use the default eye position, not the actual head eye position.
    // Using the latter will cause the camera to wobble with idle animations,
    // or with changes from the face tracker
    CameraMode mode = _camera.getMode();
    if (mode == CAMERA_MODE_FIRST_PERSON || mode == CAMERA_MODE_FIRST_PERSON_LOOK_AT) {
        _thirdPersonHMDCameraBoomValid= false;
        if (qApp->isHMDMode()) {
            mat4 camMat = myAvatar->getSensorToWorldMatrix() * myAvatar->getHMDSensorMatrix();
            _camera.setPosition(extractTranslation(camMat));
            _camera.setOrientation(glmExtractRotation(camMat));
        } else if (mode == CAMERA_MODE_FIRST_PERSON) {
            _camera.setPosition(myAvatar->getDefaultEyePosition());
            _camera.setOrientation(myAvatar->getMyHead()->getHeadOrientation());
        } else {
            _camera.setPosition(myAvatar->getCameraEyesPosition(deltaTime));
            _camera.setOrientation(myAvatar->getLookAtRotation());
        }
    } else if (mode == CAMERA_MODE_THIRD_PERSON || mode == CAMERA_MODE_LOOK_AT || mode == CAMERA_MODE_SELFIE) {
        if (qApp->isHMDMode()) {
            if (!_thirdPersonHMDCameraBoomValid) {
                const glm::vec3 CAMERA_OFFSET = glm::vec3(0.0f, 0.0f, 0.7f);
                _thirdPersonHMDCameraBoom = cancelOutRollAndPitch(myAvatar->getHMDSensorOrientation()) * CAMERA_OFFSET;
                _thirdPersonHMDCameraBoomValid = true;
            }

            glm::mat4 thirdPersonCameraSensorToWorldMatrix = myAvatar->getSensorToWorldMatrix();

            const glm::vec3 cameraPos = myAvatar->getHMDSensorPosition() + _thirdPersonHMDCameraBoom * myAvatar->getBoomLength();
            glm::mat4 sensorCameraMat = createMatFromQuatAndPos(myAvatar->getHMDSensorOrientation(), cameraPos);
            glm::mat4 worldCameraMat = thirdPersonCameraSensorToWorldMatrix * sensorCameraMat;

            _camera.setOrientation(glm::normalize(glmExtractRotation(worldCameraMat)));
            _camera.setPosition(extractTranslation(worldCameraMat));
        } else {
            _thirdPersonHMDCameraBoomValid = false;
            if (mode == CAMERA_MODE_THIRD_PERSON) {
                _camera.setOrientation(myAvatar->getHead()->getOrientation());
                if (qApp->isOptionChecked(MenuOption::CenterPlayerInView)) {
                    _camera.setPosition(myAvatar->getDefaultEyePosition()
                        + _camera.getOrientation() * boomOffset);
                } else {
                    _camera.setPosition(myAvatar->getDefaultEyePosition()
                        + myAvatar->getWorldOrientation() * boomOffset);
                }
            } else {
                glm::quat lookAtRotation = myAvatar->getLookAtRotation();
                if (mode == CAMERA_MODE_SELFIE) {
                    lookAtRotation = lookAtRotation * glm::angleAxis(PI, myAvatar->getWorldOrientation() * Vectors::UP);
                }
                _camera.setPosition(myAvatar->getLookAtPivotPoint()
                    + lookAtRotation * boomOffset);
                _camera.lookAt(myAvatar->getLookAtPivotPoint());
            }
        }
    } else if (mode == CAMERA_MODE_MIRROR) {
        _thirdPersonHMDCameraBoomValid= false;

        if (qApp->isHMDMode()) {
            auto mirrorBodyOrientation = myAvatar->getWorldOrientation() * glm::quat(glm::vec3(0.0f, PI + _mirrorYawOffset, 0.0f));

            glm::quat hmdRotation = extractRotation(myAvatar->getHMDSensorMatrix());
            // Mirror HMD yaw and roll
            glm::vec3 mirrorHmdEulers = glm::eulerAngles(hmdRotation);
            mirrorHmdEulers.y = -mirrorHmdEulers.y;
            mirrorHmdEulers.z = -mirrorHmdEulers.z;
            glm::quat mirrorHmdRotation = glm::quat(mirrorHmdEulers);

            glm::quat worldMirrorRotation = mirrorBodyOrientation * mirrorHmdRotation;

            _camera.setOrientation(worldMirrorRotation);

            glm::vec3 hmdOffset = extractTranslation(myAvatar->getHMDSensorMatrix());
            // Mirror HMD lateral offsets
            hmdOffset.x = -hmdOffset.x;

            _camera.setPosition(myAvatar->getDefaultEyePosition()
                + glm::vec3(0, _raiseMirror * myAvatar->getModelScale(), 0)
                + mirrorBodyOrientation * glm::vec3(0.0f, 0.0f, 1.0f) * MIRROR_FULLSCREEN_DISTANCE * _scaleMirror
                + mirrorBodyOrientation * hmdOffset);
        } else {
            auto userInputMapper = DependencyManager::get<UserInputMapper>();
            const float YAW_SPEED = TWO_PI / 5.0f;
            float deltaYaw = userInputMapper->getActionState(controller::Action::YAW) * YAW_SPEED * deltaTime;
            _mirrorYawOffset += deltaYaw;
            _camera.setOrientation(myAvatar->getWorldOrientation() * glm::quat(glm::vec3(0.0f, PI + _mirrorYawOffset, 0.0f)));
            _camera.setPosition(myAvatar->getDefaultEyePosition()
                + glm::vec3(0, _raiseMirror * myAvatar->getModelScale(), 0)
                + (myAvatar->getWorldOrientation() * glm::quat(glm::vec3(0.0f, _mirrorYawOffset, 0.0f))) *
                glm::vec3(0.0f, 0.0f, -1.0f) * myAvatar->getBoomLength() * _scaleMirror);
        }
        renderArgs._renderMode = RenderArgs::MIRROR_RENDER_MODE;
    } else if (mode == CAMERA_MODE_ENTITY) {
        _thirdPersonHMDCameraBoomValid= false;
        if (auto cameraEntity = _camera.getParentPointer()) {
            bool success;
            Transform entityTransform = _camera.parentJointIndex != -1 ?
              cameraEntity->getJointTransform(_camera.parentJointIndex, success) :
              cameraEntity->getTransform();
            Transform localTransform = { _camera.localRotation, glm::vec3(), _camera.localPosition };
            Transform worldTransform = entityTransform.worldTransform(localTransform);
            if (qApp->isHMDMode()) {
                glm::quat hmdRotation = extractRotation(myAvatar->getHMDSensorMatrix());
                _camera.setOrientation(worldTransform.getRotation() * hmdRotation);
                glm::vec3 hmdOffset = extractTranslation(myAvatar->getHMDSensorMatrix());
                _camera.setPosition(worldTransform.getTranslation() + (hmdRotation * hmdOffset));
            } else {
                _camera.setOrientation(worldTransform.getRotation());
                _camera.setPosition(worldTransform.getTranslation());
            }
        }
    }
    // Update camera position
    if (!qApp->isHMDMode()) {
        _camera.update();
    }

    renderArgs._cameraMode = (int8_t)_camera.getMode();
}

// the camera's mode and the View menu, kept in step

void ViewState::cycleCamera() {
    auto menu = Menu::getInstance();
    if (menu->isOptionChecked(MenuOption::FirstPersonLookAt)) {

        menu->setIsOptionChecked(MenuOption::FirstPersonLookAt, false);
        menu->setIsOptionChecked(MenuOption::LookAtCamera, true);

    } else if (menu->isOptionChecked(MenuOption::LookAtCamera)) {

        menu->setIsOptionChecked(MenuOption::LookAtCamera, false);
        if (menu->getActionForOption(MenuOption::SelfieCamera)->isVisible()) {
            menu->setIsOptionChecked(MenuOption::SelfieCamera, true);
        } else {
            menu->setIsOptionChecked(MenuOption::FirstPersonLookAt, true);
        }

    } else if (menu->isOptionChecked(MenuOption::SelfieCamera)) {

        menu->setIsOptionChecked(MenuOption::SelfieCamera, false);
        menu->setIsOptionChecked(MenuOption::FirstPersonLookAt, true);

    }
    cameraMenuChanged(); // handle the menu change
}

void ViewState::cameraModeChanged() {
    switch (_camera.getMode()) {
        case CAMERA_MODE_FIRST_PERSON_LOOK_AT:
            Menu::getInstance()->setIsOptionChecked(MenuOption::FirstPersonLookAt, true);
            break;
        case CAMERA_MODE_LOOK_AT:
            Menu::getInstance()->setIsOptionChecked(MenuOption::LookAtCamera, true);
            break;
        case CAMERA_MODE_SELFIE:
            Menu::getInstance()->setIsOptionChecked(MenuOption::SelfieCamera, true);
            break;
        default:
            // we don't have menu items for the others, so just leave it alone.
            return;
    }
    cameraMenuChanged();
}

void ViewState::changeViewAsNeeded(float boomLength) {
    // Switch between first and third person views as needed
    // This is called when the boom length has changed
    bool boomLengthGreaterThanMinimum = (boomLength > MyAvatar::ZOOM_MIN);

    if (
        (
            _camera.getMode() == CAMERA_MODE_FIRST_PERSON_LOOK_AT ||
            _camera.getMode() == CAMERA_MODE_FIRST_PERSON
        ) && boomLengthGreaterThanMinimum
    ) {
        Menu::getInstance()->setIsOptionChecked(MenuOption::FirstPersonLookAt, false);
        Menu::getInstance()->setIsOptionChecked(MenuOption::LookAtCamera, true);
        cameraMenuChanged();
    } else if (
        (
            _camera.getMode() == CAMERA_MODE_LOOK_AT ||
            _camera.getMode() == CAMERA_MODE_THIRD_PERSON
        ) && !boomLengthGreaterThanMinimum
    ) {
        Menu::getInstance()->setIsOptionChecked(MenuOption::FirstPersonLookAt, true);
        Menu::getInstance()->setIsOptionChecked(MenuOption::LookAtCamera, false);
        cameraMenuChanged();
    }
}

void ViewState::cameraMenuChanged() {
    auto menu = Menu::getInstance();
    if (menu->isOptionChecked(MenuOption::FirstPersonLookAt)) {
        if (_camera.getMode() != CAMERA_MODE_FIRST_PERSON_LOOK_AT) {
            _camera.setMode(CAMERA_MODE_FIRST_PERSON_LOOK_AT);
            getMyAvatar()->setBoomLength(MyAvatar::ZOOM_MIN);
        }
    } else if (menu->isOptionChecked(MenuOption::LookAtCamera)) {
        if (_camera.getMode() != CAMERA_MODE_LOOK_AT) {
            _camera.setMode(CAMERA_MODE_LOOK_AT);
            if (getMyAvatar()->getBoomLength() == MyAvatar::ZOOM_MIN) {
                getMyAvatar()->setBoomLength(MyAvatar::ZOOM_DEFAULT);
            }
        }
    } else if (menu->isOptionChecked(MenuOption::SelfieCamera)) {
        if (_camera.getMode() != CAMERA_MODE_SELFIE) {
            _camera.setMode(CAMERA_MODE_SELFIE);
            if (getMyAvatar()->getBoomLength() == MyAvatar::ZOOM_MIN) {
                getMyAvatar()->setBoomLength(MyAvatar::ZOOM_DEFAULT);
            }
        }
    }
}

void ViewState::rotationModeChanged() const {
    if (!Menu::getInstance()->isOptionChecked(MenuOption::CenterPlayerInView)) {
        getMyAvatar()->setHeadPitch(0);
    }
}

void ViewState::setModeKeepingPrevious(CameraMode mode) {
    _previousMode = _camera.getMode();
    _camera.setMode(mode);
    cameraModeChanged();
}

void ViewState::restorePreviousMode() {
    _camera.setMode(_previousMode);
    cameraModeChanged();
}
