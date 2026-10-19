//
//  SnapshotBuffer.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SnapshotBuffer.h"

#include <algorithm>
#include <cmath>

bool SnapshotBuffer::push(const VehicleState& state, uint64_t arrivalTime) {
    const uint64_t keep = (uint64_t)(KEEP * 1.0e6f);
    if (!_states.empty()) {
        const uint64_t newestTime = _states.back().time;
        if (state.time + keep < newestTime) {
            return false;   // older than what is kept
        }
        if (state.time > newestTime) {
            // the arrivals' rhythm, from the states that advance the buffer
            if (_newestArrival > 0 && arrivalTime > _newestArrival) {
                float sample = (float)(arrivalTime - _newestArrival) * 1.0e-6f;
                if (_interval <= 0.0f) {
                    _interval = sample;
                } else {
                    _jitter += SMOOTHING * (fabsf(sample - _interval) - _jitter);
                    _interval += SMOOTHING * (sample - _interval);
                }
            }
            _newestArrival = arrivalTime;
            _states.push_back(state);
        } else {
            // out of order: in its place, unless its time is already held
            auto it = std::lower_bound(_states.begin(), _states.end(), state.time,
                                       [](const VehicleState& s, uint64_t t) { return s.time < t; });
            if (it != _states.end() && it->time == state.time) {
                return false;
            }
            _states.insert(it, state);
        }
    } else {
        _newestArrival = arrivalTime;
        _states.push_back(state);
    }
    const uint64_t newestTime = _states.back().time;
    while (!_states.empty() && _states.front().time + keep < newestTime) {
        _states.pop_front();
    }
    while ((int)_states.size() > MAX_STATES) {
        _states.pop_front();
    }
    return true;
}

bool SnapshotBuffer::sample(uint64_t t, VehicleState& out) const {
    if (_states.size() < 2 || t >= _states.back().time || t < _states.front().time) {
        return false;
    }
    // the first state after t, and the one before it
    auto after = std::upper_bound(_states.begin(), _states.end(), t,
                                  [](uint64_t time, const VehicleState& s) { return time < s.time; });
    auto before = after - 1;
    const uint64_t span = after->time - before->time;
    float alpha = span > 0 ? (float)((double)(t - before->time) / (double)span) : 0.0f;
    out = VehicleState::interpolate(*before, *after, alpha);
    return true;
}

float SnapshotBuffer::delay() const {
    float delay = std::max(BASE_DELAY, INTERVAL_MARGIN * _interval) + JITTER_MARGIN * _jitter;
    return std::min(delay, MAX_DELAY);
}

void SnapshotBuffer::clear() {
    _states.clear();
    _newestArrival = 0;
    _interval = 0.0f;
    _jitter = 0.0f;
}
