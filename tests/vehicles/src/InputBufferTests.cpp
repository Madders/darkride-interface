//
//  InputBufferTests.cpp
//  tests/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "InputBufferTests.h"

#include <InputBuffer.h>
#include <LeadController.h>

// Add additional qtest functionality (the include order is important!)
#include <test-utils/QTestExtensions.h>

QTEST_MAIN(InputBufferTests)

namespace {
    VehicleInput inputAt(uint32_t tick) {
        VehicleInput input;
        input.sequence = tick;
        input.throttle = 0.01f * (float)tick;
        input.steering = -0.5f;
        return input;
    }
}

void InputBufferTests::alignmentAndOrder() {
    InputBuffer buffer;
    QVERIFY(!buffer.aligned());
    QCOMPARE(buffer.margin(1000), 0);
    // the first input, tick 7, arriving at substep 1000, is applied INITIAL_LEAD substeps later
    QCOMPARE(buffer.push(inputAt(7), 1000), InputBuffer::Push::Stored);
    QVERIFY(buffer.aligned());
    QCOMPARE(buffer.tickForStep(1000 + InputBuffer::INITIAL_LEAD), 7u);
    QCOMPARE(buffer.tickForStep(1000 + InputBuffer::INITIAL_LEAD + 5), 12u);
    // the next ticks, one per substep as they arrive
    for (uint32_t tick = 8; tick < 20; ++tick) {
        QCOMPARE(buffer.push(inputAt(tick), 1000 + (tick - 7)), InputBuffer::Push::Stored);
    }
    QCOMPARE(buffer.size(), (size_t)13);
    VehicleInput out;
    // the substeps before tick 7's hold (nothing to take)
    QVERIFY(!buffer.take(1000, out));
    QVERIFY(!buffer.take(1001, out));
    QCOMPARE(buffer.counts().holds, 2);
    // then every substep takes its tick, in order
    for (uint32_t step = 1002; step < 1015; ++step) {
        QVERIFY(buffer.take(step, out));
        QCOMPARE(out.sequence, 7u + (step - 1002));
        QCOMPARE(out.throttle, 0.01f * (float)out.sequence);
    }
    QCOMPARE(buffer.size(), (size_t)0);
    QCOMPARE(buffer.counts().stored, 13);
    QCOMPARE(buffer.counts().holds, 2);
    QCOMPARE(buffer.counts().late, 0);
    QCOMPARE(buffer.counts().realigned, 0);
}

void InputBufferTests::outOfOrderAndBurst() {
    InputBuffer buffer;
    QCOMPARE(buffer.push(inputAt(1), 100), InputBuffer::Push::Stored);   // tick 1 -> substep 102
    // a burst, out of order, with the redundant copies a packet carries
    QCOMPARE(buffer.push(inputAt(4), 101), InputBuffer::Push::Stored);
    QCOMPARE(buffer.push(inputAt(2), 101), InputBuffer::Push::Stored);
    QCOMPARE(buffer.push(inputAt(3), 101), InputBuffer::Push::Stored);
    QCOMPARE(buffer.push(inputAt(2), 101), InputBuffer::Push::Duplicate);
    QCOMPARE(buffer.push(inputAt(5), 101), InputBuffer::Push::Stored);
    QCOMPARE(buffer.counts().duplicates, 1);
    VehicleInput out;
    QVERIFY(!buffer.take(100, out));
    QVERIFY(!buffer.take(101, out));
    for (uint32_t step = 102; step <= 106; ++step) {
        QVERIFY(buffer.take(step, out));
        QCOMPARE(out.sequence, step - 101);
    }
    QCOMPARE(buffer.counts().holds, 2);
}

void InputBufferTests::gapsHoldTheLast() {
    InputBuffer buffer;
    buffer.push(inputAt(10), 500);   // tick 10 -> substep 502
    buffer.push(inputAt(11), 501);
    // 12 and 13 never arrive (lost on the way, and no later packet carried them); 14 does
    buffer.push(inputAt(14), 504);
    VehicleInput out;
    buffer.take(500, out);
    buffer.take(501, out);
    QVERIFY(buffer.take(502, out));
    QCOMPARE(out.sequence, 10u);
    QVERIFY(buffer.take(503, out));
    QCOMPARE(out.sequence, 11u);
    QVERIFY(!buffer.take(504, out));   // the caller holds 11
    QVERIFY(!buffer.take(505, out));
    QVERIFY(buffer.take(506, out));
    QCOMPARE(out.sequence, 14u);
    QCOMPARE(buffer.counts().holds, 4);
    // tick 12 arriving now is late: its substep has passed
    QCOMPARE(buffer.push(inputAt(12), 507), InputBuffer::Push::Late);
    QCOMPARE(buffer.counts().late, 1);
}

void InputBufferTests::lateAndEarly() {
    InputBuffer buffer;
    buffer.push(inputAt(1), 10);   // tick 1 -> substep 12
    VehicleInput out;
    for (uint32_t step = 10; step <= 20; ++step) {
        buffer.take(step, out);
    }
    // ticks up to 9 are for substeps already taken
    QCOMPARE(buffer.push(inputAt(5), 21), InputBuffer::Push::Late);
    QCOMPARE(buffer.push(inputAt(9), 21), InputBuffer::Push::Late);
    QCOMPARE(buffer.push(inputAt(10), 21), InputBuffer::Push::Stored);   // substep 21: this one
    // more than a second ahead is refused, but counts as heard of for the margin
    QCOMPARE(buffer.push(inputAt(10 + InputBuffer::MAX_LEAD), 21), InputBuffer::Push::Stored);
    QCOMPARE(buffer.push(inputAt(11 + InputBuffer::MAX_LEAD), 21), InputBuffer::Push::Early);
    QCOMPARE(buffer.counts().late, 2);
    QCOMPARE(buffer.counts().early, 1);
    QCOMPARE(buffer.margin(21), 1 + InputBuffer::MAX_LEAD);
    QVERIFY(buffer.take(21, out));
    QCOMPARE(out.sequence, 10u);
}

void InputBufferTests::margin() {
    InputBuffer buffer;
    buffer.push(inputAt(1), 0);   // tick 1 -> substep 2
    // the margin is in ticks of input past the substep's own tick: substep 0 is tick -1, so tick 1 is two ahead
    QCOMPARE(buffer.margin(0), 2);
    QCOMPARE(buffer.margin(2), 0);
    QCOMPARE(buffer.margin(5), -3);
    for (uint32_t tick = 2; tick <= 6; ++tick) {
        buffer.push(inputAt(tick), 1);
    }
    QCOMPARE(buffer.margin(2), 5);   // ticks 2..6 past substep 2's tick, 1
    QCOMPARE(buffer.margin(6), 1);
    QCOMPARE(buffer.margin(8), -1);
}

void InputBufferTests::realignment() {
    InputBuffer buffer;
    buffer.push(inputAt(1000), 50);   // tick 1000 -> substep 52
    VehicleInput out;
    for (uint32_t step = 50; step < 60; ++step) {
        buffer.take(step, out);
    }
    // the driver's counter started over (a new session of driving): aligned again from this input
    QCOMPARE(buffer.push(inputAt(1), 60), InputBuffer::Push::Stored);
    QCOMPARE(buffer.counts().realigned, 1);
    QCOMPARE(buffer.tickForStep(60 + InputBuffer::INITIAL_LEAD), 1u);
    QVERIFY(!buffer.take(60, out));
    QVERIFY(!buffer.take(61, out));
    QVERIFY(buffer.take(62, out));
    QCOMPARE(out.sequence, 1u);
    // the driver resumes after a long pause: its next tick is far behind this side's substep, so aligned again
    for (uint32_t step = 63; step < 400; ++step) {
        buffer.take(step, out);
    }
    QCOMPARE(buffer.push(inputAt(2), 400), InputBuffer::Push::Stored);
    QCOMPARE(buffer.counts().realigned, 2);
    QVERIFY(buffer.take(400 + InputBuffer::INITIAL_LEAD, out));
    QCOMPARE(out.sequence, 2u);
    // the driver far ahead for long (this side dropped time): after REALIGN_STEPS substeps over the margin, the newest
    // tick is applied a lead from now and the stale ones between go
    uint32_t step = 400 + InputBuffer::INITIAL_LEAD + 1;
    for (uint32_t tick = 3; tick <= 63; ++tick) {
        buffer.push(inputAt(tick), step);   // sixty ticks ahead
    }
    int taken = 0;
    for (uint32_t i = 0; i < 100; ++i) {
        // the driver keeps sending, one per substep, so the margin stays at sixty
        buffer.push(inputAt(64 + i), step + i);
        if (buffer.take(step + i, out)) {
            ++taken;
        }
    }
    QCOMPARE(buffer.counts().realigned, 3);
    // eighty-nine takes at a margin of sixty, the alignment set again from the newest tick (153) at the ninetieth
    // (which holds, the stale ticks gone), one more hold, and the ticks from 153 on taken at their new substeps
    QCOMPARE(taken, 98);
    QCOMPARE(out.sequence, 161u);
    QCOMPARE(buffer.size(), (size_t)2);
    QCOMPARE(buffer.margin(step + 100), 1);
}

void InputBufferTests::lateForLong() {
    InputBuffer buffer;
    VehicleInput out;
    // aligned: tick t is applied two substeps after it arrives, one tick a substep
    uint32_t step = 50;
    uint32_t tick = 1000;
    for (int i = 0; i < 20; ++i, ++step, ++tick) {
        buffer.push(inputAt(tick), step);
        buffer.take(step, out);
    }
    QCOMPARE(buffer.counts().realigned, 0);
    // the driver stalls for five substeps (a hitch on its side: this side runs on, holding the last input), then its
    // ticks go on from where they were: every input from now on comes three substeps after its own, by less than a
    // lead's worth, so each is late
    for (int i = 0; i < 5; ++i, ++step) {
        buffer.take(step, out);
    }
    int late = 0;
    for (int i = 0; i < InputBuffer::LATE_REALIGN_INPUTS - 1; ++i, ++step, ++tick) {
        if (buffer.push(inputAt(tick), step) == InputBuffer::Push::Late) {
            ++late;
        }
        buffer.take(step, out);
    }
    QCOMPARE(late, InputBuffer::LATE_REALIGN_INPUTS - 1);
    QCOMPARE(buffer.counts().realigned, 0);
    // the next late one sets the alignment again from itself: stored, and applied a lead from now
    QCOMPARE(buffer.push(inputAt(tick), step), InputBuffer::Push::Stored);
    QCOMPARE(buffer.counts().realigned, 1);
    QCOMPARE(buffer.tickForStep(step + InputBuffer::INITIAL_LEAD), tick);
    uint32_t realignedTick = tick;
    buffer.take(step, out);
    ++step;
    ++tick;
    // and the inputs after it are on time again
    int taken = 0;
    for (int i = 0; i < 20; ++i, ++step, ++tick) {
        QCOMPARE(buffer.push(inputAt(tick), step), InputBuffer::Push::Stored);
        if (buffer.take(step, out)) {
            ++taken;
        }
    }
    QVERIFY(taken >= 18);
    QVERIFY(out.sequence > realignedTick);
    // jitter: four inputs held up on the way and then arriving together, the ones whose substep has passed late, the
    // rest on time: a short run of late ones does not set the alignment again
    for (int i = 0; i < 4; ++i, ++step) {
        buffer.take(step, out);
    }
    for (int i = 0; i < 4; ++i, ++tick) {
        buffer.push(inputAt(tick), step);
    }
    for (int i = 0; i < 10; ++i, ++step, ++tick) {
        QCOMPARE(buffer.push(inputAt(tick), step), InputBuffer::Push::Stored);
        buffer.take(step, out);
    }
    QCOMPARE(buffer.counts().realigned, 1);
}

void InputBufferTests::leadAdaptation() {
    LeadController lead;
    // a steady margin at the target: nothing to do (the clock is the wall clock in seconds, as the client passes it)
    double t = 1.79e9;
    for (int i = 0; i < 40; ++i, t += 0.033f) {
        lead.note(2, t);
        QCOMPARE(lead.shift(t), 0);
    }
    // the margin falls short (the server held twice in the window): get ahead by the shortfall
    for (int i = 0; i < 30; ++i, t += 0.033f) {
        lead.note(i == 5 || i == 20 ? -1 : 2, t);
    }
    QCOMPARE(lead.shift(t), 3);   // the least was -1, the target 2
    QCOMPARE(lead.shifts(), 1);
    QVERIFY(!LeadController::isJump(3));
    // nothing more is asked during the hold-off, whatever comes in
    for (int i = 0; i < 30; ++i, t += 0.033f) {
        lead.note(-2, t);
        QCOMPARE(lead.shift(t), 0);
    }
    t += LeadController::HOLD_OFF;
    // then a window well over the target asks for a fall-back to it
    for (int i = 0; i < 35; ++i, t += 0.033f) {
        lead.note(9 + (i % 2), t);
    }
    QCOMPARE(lead.shift(t), -7);
    QVERIFY(!LeadController::isJump(-7));
    // a stall: the server far ahead for a window: a jump
    t += LeadController::HOLD_OFF;
    for (int i = 0; i < 35; ++i, t += 0.033f) {
        lead.note(-20, t);
    }
    int shift = lead.shift(t);
    QCOMPARE(shift, 22);
    QVERIFY(LeadController::isJump(shift));
    // a margin within the slack over the target leaves the lead alone
    t += LeadController::HOLD_OFF;
    for (int i = 0; i < 35; ++i, t += 0.033f) {
        lead.note(2 + (i % 4), t);
    }
    QCOMPARE(lead.shift(t), 0);
}

// Round 12: the driver drives, stands up for ten seconds (no inputs), sits again and drives on with the same counter. The
// buffer is quiet while nobody drives (no tick or margin to report: the margin of the standing time is what jumped the
// driver's clock a second ahead on a second sit), and the first input after the gap aligns it afresh at the initial lead.
void InputBufferTests::standingAndSittingAgain() {
    InputBuffer buffer;
    VehicleInput out;
    QVERIFY(buffer.quiet(1000));   // nothing yet
    uint32_t step = 1000;
    uint32_t tick = 1;
    for (int i = 0; i < 300; ++i, ++step, ++tick) {
        buffer.push(inputAt(tick), step);
        buffer.take(step, out);
    }
    QVERIFY(!buffer.quiet(step));
    QVERIFY(std::abs(buffer.margin(step)) <= InputBuffer::INITIAL_LEAD + 1);
    // standing: ten seconds of substeps with nothing; quiet once more than a lead's worth has passed
    uint32_t stoodAt = step;
    for (int i = 0; i < 900; ++i, ++step) {
        buffer.take(step, out);
        if ((int)(step - stoodAt) <= InputBuffer::MAX_LEAD - 1) {
            QVERIFY(!buffer.quiet(step));
        }
    }
    QVERIFY(buffer.quiet(step));
    QVERIFY(buffer.margin(step) < -800);   // what the stream used to report: hundreds of ticks behind
    // sitting again: the counter goes on where it was; the first input aligns afresh, the margin is the initial lead's
    int realigned = buffer.counts().realigned;
    QCOMPARE(buffer.push(inputAt(tick), step), InputBuffer::Push::Stored);
    QCOMPARE(buffer.counts().realigned, realigned + 1);
    QVERIFY(!buffer.quiet(step));
    QCOMPARE(buffer.tickForStep(step + InputBuffer::INITIAL_LEAD), tick);
    // and drives on: every substep takes its tick from there
    for (int i = 0; i < 30; ++i) {
        buffer.push(inputAt(tick + 1 + i), step + 1 + i);
    }
    QVERIFY(!buffer.take(step, out) || out.sequence < tick);   // the hold before the first is applied
    QVERIFY(!buffer.take(step + 1, out) || out.sequence < tick);
    QVERIFY(buffer.take(step + InputBuffer::INITIAL_LEAD, out));
    QCOMPARE(out.sequence, tick);
    QCOMPARE(buffer.counts().realigned, realigned + 1);   // once, not on every input after
}
