//
//  CallbackEndpoint.h
//  libraries/controllers/src/controllers/impl/endpoints
//
//  A route endpoint that is a function of a script on the WebAssembly engine: what ScriptEndpoint is for a
//  QtScript function. As a source it is asked for a number or a pose; as a destination it is handed the
//  value and the source's input id. Every call runs on the engine's thread (the endpoint is made there, and
//  the mapper's thread queues to it), and never blocks the mapper.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Controllers_CallbackEndpoint_h
#define hifi_Controllers_CallbackEndpoint_h

#include <ScriptCallback.h>

#include "../Endpoint.h"

namespace controller {

class CallbackEndpoint : public Endpoint {
    Q_OBJECT;
public:
    using Endpoint::apply;
    CallbackEndpoint(const ScriptCallback& callback) : Endpoint(Input::INVALID_INPUT), _callback(callback) {}

    virtual AxisValue peek() const override;
    virtual void apply(AxisValue newValue, const Pointer& source) override;
    virtual Pose peekPose() const override;
    virtual void apply(const Pose& newValue, const Pointer& source) override;
    virtual bool isPose() const override { return _returnPose; }

    /// A pose as a script sees it: {translation, rotation, velocity, angularVelocity, valid}, and back.
    static QVariant poseToVariant(const Pose& pose);
    static bool poseFromVariant(const QVariant& value, Pose& pose);

protected:
    Q_INVOKABLE void updateValue();
    Q_INVOKABLE void internalApply(float newValue, int sourceID);
    Q_INVOKABLE void updatePose();
    Q_INVOKABLE void internalApplyPose(const QVariant& pose, int sourceID);

private:
    ScriptCallback _callback;
    float _lastValueRead { 0.0f };
    AxisValue _lastValueWritten { 0.0f, 0, false };
    bool _returnPose { false };
    Pose _lastPoseRead;
    Pose _lastPoseWritten;
};

} // namespace controller

#endif // hifi_Controllers_CallbackEndpoint_h
