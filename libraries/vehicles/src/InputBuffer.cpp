//
//  InputBuffer.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "InputBuffer.h"

void InputBuffer::align(uint32_t tick, uint32_t step) {
    if (_aligned) {
        ++_counts.realigned;
    }
    _aligned = true;
    _offset = step + (uint32_t)INITIAL_LEAD - tick;
    _inputs.clear();
    _newestTick = tick;
    _overfullSteps = 0;
    _lateRun = 0;
    for (int i = 0; i < SEEN_RING; ++i) {
        _seenValid[i] = false;
    }
}

InputBuffer::Push InputBuffer::push(const VehicleInput& input, uint32_t step) {
    uint32_t tick = input.sequence;
    if (!_aligned) {
        align(tick, step);
    } else {
        // the driver's ticks started over (its counter went back by more than a lead's worth), or they come so late
        // that no lead would do (a client that paused and resumed), or after a gap of more than a lead's worth with
        // nothing (the driver stood up and sat again, round 12): the alignment is set again from this input
        int32_t behindNewest = (int32_t)(_newestTick - tick);
        int32_t stepOffset = (int32_t)((tick + _offset) - (_taken ? _lastTakenStep : step));
        bool gap = _pushed && (int32_t)(step - _lastPushStep) > MAX_LEAD;
        if (behindNewest > MAX_LEAD || stepOffset < -MAX_LEAD || gap) {
            align(tick, step);
        }
    }
    _lastPushStep = step;
    _pushed = true;
    uint32_t targetStep = tick + _offset;
    if ((int32_t)(tick - _newestTick) > 0) {
        _newestTick = tick;
    }
    if (seen(tick)) {
        ++_counts.duplicates;   // one of a packet's redundant copies, or a resend
        return Push::Duplicate;
    }
    markSeen(tick);
    if (_taken && (int32_t)(targetStep - _lastTakenStep) <= 0) {
        // late for long: the driver lost a few ticks (a hitch on its side) and every input now comes after its substep
        // by less than would align it again above, so each would be dropped for good while the last input is held
        // (10-24: a flight on the assistant, mid-course, after the client stalled loading models). Set the alignment
        // again from this input, as take() does for a driver far ahead for long.
        if (++_lateRun < LATE_REALIGN_INPUTS) {
            ++_counts.late;
            return Push::Late;
        }
        align(tick, step);
        markSeen(tick);
        targetStep = tick + _offset;
    }
    _lateRun = 0;
    if ((int32_t)(targetStep - step) > MAX_LEAD) {
        ++_counts.early;
        return Push::Early;
    }
    _inputs[tick] = input;
    ++_counts.stored;
    return Push::Stored;
}

bool InputBuffer::take(uint32_t step, VehicleInput& out) {
    _lastTakenStep = step;
    _taken = true;
    if (!_aligned) {
        return false;
    }
    // the driver far ahead for long (this side dropped time, or its clock runs fast): set the alignment again from
    // the newest tick, which is applied a lead from now; the inputs between are stale and go
    if (margin(step) > REALIGN_MARGIN) {
        if (++_overfullSteps >= REALIGN_STEPS) {
            VehicleInput newest;
            bool haveNewest = false;
            auto it = _inputs.find(_newestTick);
            if (it != _inputs.end()) {
                newest = it->second;
                haveNewest = true;
            }
            align(_newestTick, step);
            if (haveNewest) {
                _inputs[_newestTick] = newest;
            }
        }
    } else {
        _overfullSteps = 0;
    }
    uint32_t tick = step - _offset;
    // everything older than this substep's tick was for substeps that have passed
    auto it = _inputs.begin();
    while (it != _inputs.end() && (int32_t)(it->first - tick) < 0) {
        it = _inputs.erase(it);
    }
    if (it != _inputs.end() && it->first == tick) {
        out = it->second;
        _inputs.erase(it);
        return true;
    }
    ++_counts.holds;
    return false;
}

int InputBuffer::margin(uint32_t step) const {
    if (!_aligned) {
        return 0;
    }
    return (int32_t)(_newestTick - (step - _offset));
}

void InputBuffer::reset() {
    _inputs.clear();
    _aligned = false;
    _pushed = false;
    _offset = 0;
    _newestTick = 0;
    _lastTakenStep = 0;
    _taken = false;
    _overfullSteps = 0;
    _lateRun = 0;
    _counts = Counts();
    for (int i = 0; i < SEEN_RING; ++i) {
        _seenValid[i] = false;
    }
}
