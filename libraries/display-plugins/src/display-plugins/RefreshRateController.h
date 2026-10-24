//
//  RefreshRateController.h
//  libraries/display-pluging/src/display-plugin
//
//  Created by Dante Ruiz on 2019-04-15.
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RefreshRateController_h
#define hifi_RefreshRateController_h

#include <DependencyManager.h>

#include <chrono>
#include <atomic>

class QThread;

class RefreshRateController {
public:
    RefreshRateController() = default;
    ~RefreshRateController() = default;

    void setRefreshRateLimitPeriod(int refreshRateLimit);
    int getRefreshRateLimitPeriod() const;

    // the display's own rate (Hz), what the display-based profile targets; 0 when unknown
    void setDisplayRefreshRate(int refreshRate) { _displayRefreshRate = refreshRate; }
    // the period the presents are paced at, in seconds (sleepThreadIfNeeded); 0 when they are not paced to a
    // sensible one (a minimized window's, or none set yet)
    float getPacedPeriodSeconds() const;
    // the slot of the next present: counts the paced intervals, one per present as a rule and more when a present
    // lost one (DisplayPlugin::presented carries it to the game loop)
    uint64_t getSlotIndex() const { return _slotIndex; }

    void clockStartTime() { _startTime = std::chrono::high_resolution_clock::now(); }
    void clockEndTime() { _endTime = std::chrono::high_resolution_clock::now(); }
    // Paces the presents, waiting precisely for the next slot (a coarse sleep, then a spin for the last fraction of
    // a millisecond). With the display's refresh signal (DisplayLink) and a target at the display's rate, each slot
    // is just after a refresh; otherwise the slots are an absolute schedule, one every period on the steady clock.
    // The frames then land on the display's refreshes evenly (PHYSICS.md "The driver's stutter": the old sleep, whole
    // milliseconds measured from the present's own duration, wandered by a millisecond either way and the display
    // repeated or skipped a frame wherever that drifted across a refresh). On the free schedule a present that ran
    // late is followed by a shorter wait, back onto it; more than a period late, the schedule restarts from now.
    // With vsync the swap itself waits for the display's refresh, so a target at the display's rate sleeps not at
    // all; an HMD never sleeps.
    std::chrono::nanoseconds sleepThreadIfNeeded(QThread* thread, bool isHmd, bool vsync = false);
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> _startTime { std::chrono::high_resolution_clock::now() };
    std::chrono::time_point<std::chrono::high_resolution_clock> _endTime { std::chrono::high_resolution_clock::now() };
    std::atomic<int64_t> _refreshRateLimitPeriod { 50 };
    std::atomic<int> _displayRefreshRate { 0 };
    std::atomic<int64_t> _pacedPeriodNs { 0 };   // the period of the slots as last paced (the display's when locked)
    std::atomic<uint64_t> _slotIndex { 0 };
    // after the free schedule restarts (a present that overran its period, a change of rate) the slots are not whole
    // periods after the last ones: the clock is reported unpaced (getPacedPeriodSeconds 0) for this many presents more
    std::atomic<int> _unpacedPresents { 0 };
    // how late the kernel's sleep wakes the present thread, a decaying maximum: the spin before a slot covers it
    int64_t _sleepLateNs { 600000 };
    // the schedule (present thread only)
    uint64_t _presentCount { 0 };
    int64_t _lastSlotNs { 0 };   // the last locked slot: the next is in a later interval
    bool _scheduled { false };
    std::chrono::nanoseconds _scheduledPeriod { 0 };
    std::chrono::steady_clock::time_point _nextPresent;

};

#endif
