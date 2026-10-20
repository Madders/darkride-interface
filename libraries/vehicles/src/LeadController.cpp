//
//  LeadController.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "LeadController.h"

#include <algorithm>

void LeadController::note(int margin, double t) {
    _samples.push_back({ margin, t });
    while (!_samples.empty() && t - _samples.front().second > WINDOW) {
        _samples.pop_front();
    }
}

int LeadController::shift(double t) {
    if (t - _lastShiftTime < HOLD_OFF || (int)_samples.size() < MIN_SAMPLES || t - _samples.front().second < WINDOW * 0.9f) {
        return 0;
    }
    int least = _samples.front().first;
    for (const auto& sample : _samples) {
        least = std::min(least, sample.first);
    }
    _lastMinimum = least;
    int shift = 0;
    if (least < TARGET) {
        shift = TARGET - least;
    } else if (least > TARGET + SLACK) {
        shift = TARGET - least;   // negative: fall back to the target
    }
    shift = std::max(-MAX_SHIFT, std::min(MAX_SHIFT, shift));
    if (shift != 0) {
        _lastShiftTime = t;
        _samples.clear();
        ++_shifts;
    }
    return shift;
}

void LeadController::reset() {
    _samples.clear();
    _lastShiftTime = -1.0e12;
    _lastMinimum = 0;
    _shifts = 0;
}
