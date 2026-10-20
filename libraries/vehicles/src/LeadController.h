//
//  LeadController.h
//  libraries/vehicles/src
//
//  The driver's side of the input buffer (PHYSICS.md round 10): keeps the client's inputs a few substeps ahead of the
//  physics server's need for them. Every VehicleState brings the server's margin (how many ticks of input past the
//  substep it applied had arrived); the controller watches the least margin over a window and, when it is short of
//  the target, asks for the client to get further ahead, and when it stays well over, to fall back. A small shift is
//  made by time dilation (the client's simulation runs a few percent fast or slow until the shift is in, so the
//  prediction stays one substep per tick and nothing is corrected); a large one forward, after a stall, is a jump of
//  the tick counter. The least over a window adapts the lead to the jitter: a jittery link gives a low least for its
//  mean, so the mean is pushed up by as much.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_LeadController_h
#define hifi_LeadController_h

#include <deque>
#include <utility>

class LeadController {
public:
    static constexpr int TARGET = 2;            // ticks: the least margin wanted over a window
    static constexpr int SLACK = 3;             // ticks over the target the least may sit before the lead is cut
    static constexpr float WINDOW = 1.0f;       // s: the margins judged together
    static constexpr int MIN_SAMPLES = 5;
    static constexpr float HOLD_OFF = 1.5f;     // s after a shift before the next is judged (the shift takes a while)
    static constexpr int JUMP = 9;              // ticks: a shift forward this large is a jump of the counter, not a dilation
    static constexpr int MAX_SHIFT = 90;        // ticks: no shift larger than a second

    // a state's margin, at local time t (s; a double, since a float of the wall clock in seconds cannot tell two
    // moments a second apart)
    void note(int margin, double t);
    // the shift the client should make now, in ticks (positive: get further ahead), 0 for none; at most one per hold-off
    int shift(double t);
    // whether a shift of this size should be a jump of the tick counter (true) or a dilation (false)
    static bool isJump(int shift) { return shift >= JUMP; }

    int lastMinimum() const { return _lastMinimum; }   // the least margin of the last window judged
    int shifts() const { return _shifts; }             // shifts asked for so far
    void reset();

private:
    std::deque<std::pair<int, double>> _samples;       // margin, time
    double _lastShiftTime { -1.0e12 };
    int _lastMinimum { 0 };
    int _shifts { 0 };
};

#endif // hifi_LeadController_h
