//
//  SnapshotBuffer.h
//  libraries/vehicles/src
//
//  A jitter buffer of vehicle states for snapshot interpolation (PHYSICS.md 4.6, built in round 9). A follower shows a
//  vehicle a little behind the newest state it has, so that two states bracket the time it shows, and interpolates
//  between them; when none does (a late or lost packet) the caller extrapolates with the model instead. The delay
//  behind the newest state adapts to the jitter of the arrivals: a steady stream is shown about 100 ms behind, a
//  jittery one further. Plain numbers on glm, no Qt beyond the state's id; unit-tested in tests/vehicles.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SnapshotBuffer_h
#define hifi_SnapshotBuffer_h

#include <cstdint>
#include <deque>

#include "VehicleState.h"

class SnapshotBuffer {
public:
    static constexpr float BASE_DELAY = 0.1f;       // s behind the newest state: PHYSICS.md 4.6's "about 100 ms"
    static constexpr float MAX_DELAY = 0.4f;
    static constexpr float INTERVAL_MARGIN = 1.5f;  // at least this many arrival intervals behind, so two states bracket
    static constexpr float JITTER_MARGIN = 3.0f;    // plus this many times the measured jitter
    static constexpr float SMOOTHING = 0.1f;        // the arrivals' interval and jitter are smoothed with this weight
    static constexpr float KEEP = 1.0f;             // s of states kept behind the newest
    static constexpr int MAX_STATES = 64;

    // a state as received, its time already on this clock, and when it arrived (this clock); false for one whose time
    // is already held (a duplicate) or older than what is kept
    bool push(const VehicleState& state, uint64_t arrivalTime);
    // the state at time t, interpolated between the two that bracket it; false when t is at or past the newest (the
    // caller extrapolates from where it is) or before the oldest, or there are not two to bracket with
    bool sample(uint64_t t, VehicleState& out) const;

    bool empty() const { return _states.empty(); }
    size_t size() const { return _states.size(); }
    const VehicleState& newest() const { return _states.back(); }
    const VehicleState& oldest() const { return _states.front(); }
    uint64_t newestArrival() const { return _newestArrival; }

    // seconds behind the newest state's time to show: the base delay, or more when the arrivals are far apart or jittery
    float delay() const;
    float interval() const { return _interval; }   // s, the smoothed time between arrivals
    float jitter() const { return _jitter; }       // s, the smoothed deviation of that interval

    void clear();

private:
    std::deque<VehicleState> _states;   // by time, oldest first
    uint64_t _newestArrival { 0 };
    float _interval { 0.0f };
    float _jitter { 0.0f };
};

#endif // hifi_SnapshotBuffer_h
