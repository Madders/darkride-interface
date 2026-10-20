//
//  ImpostorSwitch.cpp
//  libraries/render-utils/src
//
//  Created by the editor session on 2026-10-18. See ImpostorSwitch.h and IMPOSTORS.md 2.4.
//

#include "ImpostorSwitch.h"

#include <NumericalConstants.h>
#include <SharedUtil.h>
#include <ViewFrustum.h>

#include "FadeEffectJobs.h"

const float ImpostorSwitch::CROSSFADE_SECONDS = 0.35f;
const float ImpostorSwitch::STAND_IN_GRACE_SECONDS = 0.25f;

namespace {
    // the hysteresis band of the main view, on the squared half-tangent: the card comes at a fifth under the angle's
    // measure and goes at a fifth over it, about a tenth of the distance either way
    const float BAND = 0.2f;

    void setParams(gpu::StructBuffer<FadeObjectParams>& buffer, float threshold) {
        auto& params = buffer.edit();
        params.noiseOffset = glm::vec4(0.0f);
        params.baseOffset = glm::vec4(0.0f);
        params.baseInvSize = glm::vec4(1.0f);
        params.category = FADE_LOD_SWITCH;
        params.threshold = threshold;
    }
}

// LODManager::shouldRender's test: the bound's half size squared against the angle's half tangent squared times
// the squared distance; the card shows where the model would be too small for the impostor angle, and never with the
// camera inside the sphere (the card is a section through the sphere's centre, nothing from in there)
bool ImpostorSwitch::wantsCardAt(float distanceSq, float thresholdSq) const {
    float radiusSq = _radius * _radius;
    return distanceSq > radiusSq && radiusSq < thresholdSq * distanceSq;
}

bool ImpostorSwitch::standsIn(quint64 now) {
    if (!_standIn.load(std::memory_order_relaxed)) {
        _standInSince = 0;
        return false;
    }
    if (_standInSince == 0) {
        _standInSince = now;
    }
    return (float)(now - _standInSince) >= STAND_IN_GRACE_SECONDS * (float)USECS_PER_SECOND;
}

ImpostorSwitch::View ImpostorSwitch::evaluate(const RenderArgs* args) {
    View view;
    if (!args) {
        return view;
    }
    const bool mainView = args->_renderMode == RenderArgs::RenderMode::DEFAULT_RENDER_MODE;
    const bool shadow = args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE;
    if (mainView || shadow) {
        if (mainView) {
            // the state the view goes to: every item of a frame reads the same one, and the band's two tests agree
            // with each other for either state, so the model's parts and the card never disagree about it
            bool target = _mainViewShowsCard;
            const float thresholdSq = args->_impostorAngleHalfTanSq;
            quint64 now = usecTimestampNow();
            const bool standIn = standsIn(now);
            if (!_cardReady || _radius <= 0.0f || thresholdSq <= 0.0f || !args->hasViewFrustum()) {
                target = false;
            } else {
                glm::vec3 toCamera = args->getViewFrustum().getPosition() - _center;
                float distanceSq = glm::dot(toCamera, toCamera);
                float radiusSq = _radius * _radius;
                _shownAngleDeg.store(distanceSq > radiusSq ? glm::degrees(2.0f * atanf(_radius / sqrtf(distanceSq))) : 180.0f,
                                     std::memory_order_relaxed);
                if (distanceSq <= radiusSq) {
                    target = false;
                } else if (standIn) {
                    target = true;   // the models are not here yet: the card at any angle
                } else if (_mainViewShowsCard) {
                    if (radiusSq > thresholdSq * (1.0f + BAND) * distanceSq) {
                        target = false;
                    }
                } else if (radiusSq < thresholdSq * (1.0f - BAND) * distanceSq) {
                    target = true;
                }
            }
            if (target != _mainViewShowsCard) {
                _mainViewShowsCard = target;
                _fadeFrom = _share;   // a reversal in mid-dissolve goes on from where it is
                _fadeStart = now;
            }
            float goal = _mainViewShowsCard ? 1.0f : 0.0f;
            if (args->_impostorCrossfade >= 0.0f) {
                _share = glm::clamp(args->_impostorCrossfade, 0.0f, 1.0f);
            } else if (_share != goal) {
                float t = (float)(now - _fadeStart) / (CROSSFADE_SECONDS * (float)USECS_PER_SECOND);
                _share = (t >= 1.0f) ? goal : glm::mix(_fadeFrom, goal, glm::max(t, 0.0f));
            }
            _shownShare.store(_share, std::memory_order_relaxed);
        }
        // the shadow pass takes the main view's state as it stands (its own camera is the light's)
        view.share = _share;
        view.card = _share > 0.0f;
        view.model = _share < 1.0f;
        return view;
    }
    // mirrors and the secondary camera: the plain threshold on their own camera, no band, no dissolve
    bool card = false;
    if (_cardReady && _radius > 0.0f && args->_impostorAngleHalfTanSq > 0.0f && args->hasViewFrustum()) {
        glm::vec3 toCamera = args->getViewFrustum().getPosition() - _center;
        float distanceSq = glm::dot(toCamera, toCamera);
        card = wantsCardAt(distanceSq, args->_impostorAngleHalfTanSq) ||
            (standsIn(usecTimestampNow()) && distanceSq > _radius * _radius);
    }
    view.card = card;
    view.model = !card;
    view.share = card ? 1.0f : 0.0f;
    return view;
}

const gpu::BufferView& ImpostorSwitch::fadeParams(const RenderArgs* args) {
    const bool mainOrShadow = args && (args->_renderMode == RenderArgs::RenderMode::DEFAULT_RENDER_MODE ||
                                       args->_renderMode == RenderArgs::RenderMode::SHADOW_RENDER_MODE);
    if (mainOrShadow) {
        if (_mainParamsShare != _share) {
            setParams(_mainParams, _share);
            _mainParamsShare = _share;
        }
        return _mainParams;
    }
    if (!_plainParamsSet) {
        setParams(_plainParams, 0.0f);
        _plainParamsSet = true;
    }
    return _plainParams;
}
