#!/usr/bin/env python3
#
#  hover-replica.py
#  tests-manual/physics-baseline
#
#  A replica of the tilt-thrust aircraft's hover in the horizontal plane (PHYSICS.md round 8), on which the autopilot's
#  homing law in airship.js was designed after five flights circled the pad. The craft: a pitch attitude that follows the
#  pitch input through the hover's torque balance (sin of the attitude = 0.41 per unit of input, the thrust point 0.5 m
#  aft of the centre of mass biasing it 0.222 nose-down), a yaw rate of 0.82 rad/s at full steering, and the bank that a
#  yaw while pitched gives (a rotation about a tilted axis), levelled with a 0.6 s time constant; the thrust carries the
#  weight, so the horizontal acceleration is g tan(attitude) along the nose and g sin(bank) to the right. Conventions as
#  in airship.js: heading = atan2(front.x, -front.z) (0 facing -z, 90 facing +x), positive steering turns right, positive
#  pitch input is nose-up (backward thrust), the pad's bearing = atan2(-dx, dz).
#
#  Run it to see the law through five starts (the states flights ended their brake in, an overshoot, a drift, a straight
#  run in): each line is the time to come within 2 m, the distance and speed at the end, and the heading turned. An
#  environment variable overrides any parameter of the law (GATE=30 ./hover-replica.py) or the craft's bias (PITCH_BIAS).
#  -v prints a trace of the first case.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
import math, os, sys

G = 9.8
PITCH_SS = 0.41                                            # sin(attitude) per unit of pitch input (0.5 x 4.5 over 0.5 x 11)
PITCH_BIAS = float(os.environ.get("PITCH_BIAS", "-0.222"))  # the thrust point's nose-down bias; +0.222 of input holds level
PITCH_TAU = 0.4                                            # s, the attitude's response
YAW_RATE = 0.82                                            # rad/s at full steering (the yaw lever over the damping lever)
YAW_TAU = 0.1
LEVEL_TAU = 0.6                                            # s, the levelling torque's time constant in roll

# the law's parameters, as in airship.js
P = dict(HOME_GAIN=0.2, HOME_SPEED=8, HOME_DEAD=0.5, TRIM=0.222, KV=4.0, ATT_MAX=20, GATE=15, YAW_DEG=3.0, KI=0.05, IMAX=0.3,
         FOLD=4, STEER_DEG=30, EMAG_DEAD=0.05, EMAG_FULL=0.2)
for k in P:
    if k in os.environ: P[k] = float(os.environ[k])

def wrap(a):
    while a > 180: a -= 360
    while a < -180: a += 360
    return a
def clamp1(v): return max(-1.0, min(1.0, v))

class Craft:
    def __init__(self, heading, speed, vheading):
        self.x = self.z = 0.0
        self.h = math.radians(heading)
        self.vx = speed * math.sin(math.radians(vheading)); self.vz = -speed * math.cos(math.radians(vheading))
        self.theta = math.asin(PITCH_SS * PITCH_BIAS)   # nose-up positive; trimmed with no input the nose hangs a little down
        self.r = 0.0                                    # yaw rate, right positive
        self.phi = 0.0                                  # bank, the right wing down positive
        self.turned = 0.0
    def step(self, pitch, steering, dt):
        theta_ss = math.asin(max(-0.95, min(0.95, PITCH_SS * (clamp1(pitch) + PITCH_BIAS))))
        self.theta += (theta_ss - self.theta) * dt / PITCH_TAU
        self.r += (YAW_RATE * clamp1(steering) - self.r) * dt / YAW_TAU
        self.h += self.r * dt; self.turned += self.r * dt
        self.phi += (self.r * self.theta - self.phi / LEVEL_TAU) * dt
        a = -G * math.tan(self.theta)                   # along the nose: nose down accelerates forward
        side = G * math.sin(self.phi)                   # to the right, from the bank
        ax = a * math.sin(self.h) + side * math.cos(self.h); az = -a * math.cos(self.h) + side * math.sin(self.h)
        sp = math.hypot(self.vx, self.vz)
        ax -= 0.002 * sp * self.vx; az -= 0.002 * sp * self.vz
        self.vx += ax * dt; self.vz += az * dt
        self.x += self.vx * dt; self.z += self.vz * dt
    def sense(self, pad):
        dx, dz = self.x - pad[0], self.z - pad[1]
        return dict(padDistance=math.hypot(dx, dz), padBearing=math.degrees(math.atan2(-dx, dz)), heading=math.degrees(self.h),
                    vx=self.vx, vz=self.vz, horizontalSpeed=math.hypot(self.vx, self.vz), bank=math.degrees(self.phi),
                    pitchDeg=math.degrees(self.theta))

def law(s, st, dt):
    # the law of airship.js's aircraftAutopilot for a hover going to the pad (a.toPad), returning (pitch input, steering)
    trim = st.get("trim", 0.0)
    wanted = min(P["HOME_SPEED"], P["HOME_GAIN"] * max(0.0, s["padDistance"] - P["HOME_DEAD"]))
    b = math.radians(s["padBearing"]); h = math.radians(s["heading"])
    ex = wanted * math.sin(b) - s["vx"]; ez = -wanted * math.cos(b) - s["vz"]
    eAlong = ex * math.sin(h) - ez * math.cos(h); eSide = ex * math.cos(h) + ez * math.sin(h)
    eMag = math.hypot(eAlong, eSide)
    off = math.degrees(math.atan2(eSide, eAlong))
    if eMag < P["FOLD"]:
        if off > 90: off -= 180
        elif off < -90: off += 180
    steering = clamp1(off / P["STEER_DEG"]) * min(1.0, max(0.0, (eMag - P["EMAG_DEAD"]) / (P["EMAG_FULL"] - P["EMAG_DEAD"])))
    steering *= P["YAW_DEG"] / max(P["YAW_DEG"], abs(s["pitchDeg"]))
    attitude = max(-P["ATT_MAX"], min(P["ATT_MAX"], P["KV"] * eAlong)) if abs(off) < P["GATE"] else 0.0   # degrees nose-down
    trim = max(-P["IMAX"], min(P["IMAX"], trim + P["KI"] * ((-attitude) - s["pitchDeg"]) * dt))
    st["trim"] = trim
    return clamp1(P["TRIM"] + trim - attitude / 24.0), steering

def run(start, pad, seconds=60, dt=1 / 60, verbose=False, label=""):
    c = Craft(*start); st = {}; t = 0.0; reached = None; turned_at = 0.0; worst_after = 0.0
    while t < seconds:
        s = c.sense(pad)
        pitch, steering = law(s, st, dt)
        c.step(pitch, steering, dt)
        t += dt
        if verbose and int(round(t / dt)) % 90 == 0:
            print("  t %5.1f d %6.2f bear %6.0f head %6.0f hs %5.2f pitch %5.2f steer %5.2f att %5.1f bank %5.1f" % (
                t, s["padDistance"], s["padBearing"], s["heading"], s["horizontalSpeed"], pitch, steering, s["pitchDeg"], s["bank"]))
        if reached is None and s["padDistance"] < 2.0: reached, turned_at = t, c.turned
        if reached is not None: worst_after = max(worst_after, s["padDistance"])
    s = c.sense(pad)
    print("%-30s within 2 m at %s s; end %.2f m, %.2f m/s; turned %.0f deg (%.0f after reaching); worst after reaching %.1f m" % (
        label, ("%.1f" % reached) if reached else "never", s["padDistance"], s["horizontalSpeed"], math.degrees(c.turned),
        math.degrees(c.turned - turned_at) if reached else 0, worst_after))

def scenario(label, heading, speed, vheading, bearing, distance):
    b = math.radians(bearing)
    return label, (heading, speed, vheading), (distance * math.sin(b), -distance * math.cos(b))

if __name__ == "__main__":
    verbose = "-v" in sys.argv
    cases = [
        scenario("A: 24 m, pad 43 deg left", 172, 2.9, 172, 129, 24),      # where flight 5's brake ended
        scenario("B: 115 m, pad astern", 171, 3.0, 171, -4, 115),          # where flight 1's brake ended
        scenario("C: 8 m, overshooting at 4.8", -35, 4.8, 104, 116, 8),    # flight 5 entering its descent
        scenario("D: 3 m, drifting 3 m/s", 0, 3.0, 90, 180, 3),
        scenario("E: 60 m, pad ahead", 0, 0.0, 0, 0, 60),
    ]
    for i, (label, start, pad) in enumerate(cases):
        run(start, pad, verbose=verbose and i == 0, label=label)
