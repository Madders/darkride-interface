//
//  InputBuffer.h
//  libraries/vehicles/src
//
//  The physics server's input buffer for a vehicle's driver (PHYSICS.md round 10): a time-synchronised input buffer, the
//  standard answer to inputs arriving early, late or in bursts. The driver's client numbers its inputs by its own
//  substeps (the "tick": one per substep, VehicleInput::sequence), and the server applies each at a substep of its
//  own fixed by the tick: the driver's first input is scheduled INITIAL_LEAD substeps after it arrives, and every later
//  tick is that many substeps after the first, so a burst or a stall on the way changes nothing about which substep an
//  input is applied at (round 9's queue applied inputs as they came and cut the queue when it grew, which put inputs
//  at the wrong substep and showed up as corrections at speed). An input whose substep has passed is late and
//  dropped; a substep with no input holds the last one. The margin, how many ticks of input past the current substep
//  have arrived, goes back to the driver in every VehicleState so its client can keep a small lead (LeadController).
//  The alignment is set again when the driver's ticks start over, or come far too late or early for long.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_InputBuffer_h
#define hifi_InputBuffer_h

#include <cstdint>
#include <map>

#include "VehicleInput.h"

class InputBuffer {
public:
    static constexpr int INITIAL_LEAD = 2;     // substeps: a driver's first input is applied this long after it arrives
    static constexpr int MAX_LEAD = 90;        // substeps: an input further ahead than this is refused (a second)
    static constexpr int REALIGN_MARGIN = 45;  // a margin past this for REALIGN_STEPS substeps sets the alignment again
    static constexpr int REALIGN_STEPS = 90;

    enum class Push { Stored, Duplicate, Late, Early };

    // the driver's input (its tick in `sequence`) arriving while this side's substep is `step`
    Push push(const VehicleInput& input, uint32_t step);
    // the input for substep `step`: true and the input when one arrived for it, false when none did (the caller
    // holds its last; counted as a hold). Takes are expected in order, one per substep.
    bool take(uint32_t step, VehicleInput& out);
    // how many ticks of input past substep `step` have arrived: 0 when the newest is for this substep, negative
    // when nothing has arrived for it or beyond
    int margin(uint32_t step) const;
    // the driver's tick that substep `step` applies (0 before the first input)
    uint32_t tickForStep(uint32_t step) const { return _aligned ? step - _offset : 0; }
    bool aligned() const { return _aligned; }
    size_t size() const { return _inputs.size(); }
    void reset();

    struct Counts {
        int stored { 0 };
        int duplicates { 0 };  // a tick received before (every packet carries the last few, so most of each packet)
        int late { 0 };        // a tick not received before whose substep had passed
        int early { 0 };
        int holds { 0 };      // substeps that had no input and held the last
        int realigned { 0 };  // times the alignment was set again after the first
    };
    const Counts& counts() const { return _counts; }

private:
    void align(uint32_t tick, uint32_t step);

    static const int SEEN_RING = 64;            // ticks remembered as received, to tell a packet's redundant copies apart
    bool seen(uint32_t tick) const { return _seenValid[tick % SEEN_RING] && _seen[tick % SEEN_RING] == tick; }
    void markSeen(uint32_t tick) { _seen[tick % SEEN_RING] = tick; _seenValid[tick % SEEN_RING] = true; }

    std::map<uint32_t, VehicleInput> _inputs;   // by tick, those not yet taken
    uint32_t _seen[SEEN_RING];
    bool _seenValid[SEEN_RING] { false };
    bool _aligned { false };
    uint32_t _offset { 0 };          // substep = tick + offset (wraps as the unsigned does)
    uint32_t _newestTick { 0 };      // the highest tick heard of, stored or not
    uint32_t _lastTakenStep { 0 };
    bool _taken { false };
    int _overfullSteps { 0 };
    Counts _counts;
};

#endif // hifi_InputBuffer_h
