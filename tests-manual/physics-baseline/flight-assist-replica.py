#!/usr/bin/env python3
"""The flight assistant (scripts/system/libraries/flightAssist.js) designed on the aircraft's replica
(flight-replica.py): the pilot's keys become an intent (a speed, a bank or a turn, a climb or descent), and the
assistant turns the intent and the craft's state into the model's inputs (throttle, steering, pitch, and the
nacelles' tilt) every frame. This file is the design copy; flightAssist.js is the same law in JavaScript.

    flight-assist-replica.py [course]      a scripted flight through every regime, with a summary per phase
"""
import importlib.util
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("fr", os.path.join(HERE, "flight-replica.py"))
fr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fr)

# ---- the assistant (mirrored in flightAssist.js) ----
WEIGHT = 1200 * 9.8
HOVER_THROTTLE = 0.59            # the thrust that carries the weight
HOVER_TRIM = 0.5 / (0.5 * 4.5)   # the pitch input that holds the nose level in hover (the thrust point is aft)
HOVER_ATTITUDE = 24.0            # degrees nose-down per unit of pitch input in hover
SPEED_RATE = 10.0                # m/s per second while W or S is held
MAX_SPEED = 55.0
HOVER_TOP = 12.0                 # m/s: below it the rotors stand up (hover); the pilot's speed there is a hover speed
WING_SPEED = 28.0                # m/s: above it the rotors lie forward (the wing carries the craft)
HOVER_MAX_ALONG = 14.0           # m/s: the fastest the hover law asks for along the nose
BANK_MAX = 40.0                  # degrees with A or D held
BANK_RATE = 60.0                 # degrees per second the wanted bank moves
CLIMB_ANGLE = 12.0               # degrees of flight path with Up or Down held, in flight
CLIMB_RATE = 4.0                 # m/s with Up or Down held, in hover
YAW_RATE = 40.0                  # degrees per second with A or D held, in hover
ALPHA_MAX = 11.0                 # degrees: the wing stalls at 15
TURN_ELEVATOR = 0.3               # pitch input per unit of (1/cos(bank) - 1)
LEVEL_RATE = 0.5                 # m/s: letting go, the craft levels off and holds the height where its climb fell below this


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def wrap(d):
    while d > 180:
        d -= 360
    while d < -180:
        d += 360
    return d


class Assist:
    def __init__(self):
        self.speed_target = 0.0
        self.bank_wanted = 0.0
        self.hold_height = None
        self.hover_trim = 0.0      # the hover's pitch trim, learned from the measured attitude
        self.pitch_trim = 0.0      # the flight's elevator trim, learned from the attitude error
        self.speed_integral = 0.0

    def controls(self, keys, s, dt):
        """keys: {'faster','slower','left','right','up','down','stop'} each 0 or 1; s: the state (see state());
        returns the model's inputs and what was wanted (for telemetry)."""
        c = {'throttle': 0.0, 'steering': 0.0, 'pitch': 0.0, 'lift': 0.0}
        # the speed wanted: W and S move it, X zeroes it
        self.speed_target = clamp(self.speed_target + SPEED_RATE * dt * (keys.get('faster', 0) - keys.get('slower', 0)),
                                  0.0, MAX_SPEED)
        if keys.get('stop'):
            self.speed_target = 0.0
        turn = keys.get('right', 0) - keys.get('left', 0)
        vertical = keys.get('up', 0) - keys.get('down', 0)
        on_ground = s['aboveGround'] < 0.6 and abs(s['climbRate']) < 0.5

        # the nacelles by the airspeed: up below the hover's top speed, forward from the wing's; while the pilot wants
        # to fly, the hover's speed keeps building until the wing takes over
        airspeed = s['airspeed']
        tilt_wanted = clamp((WING_SPEED - airspeed) / (WING_SPEED - HOVER_TOP), 0.0, 1.0)
        if self.speed_target <= HOVER_TOP:
            tilt_wanted = 1.0 if airspeed < WING_SPEED + 4 else tilt_wanted
        d = tilt_wanted - s['tilt']
        c['lift'] = 0.0 if abs(d) < 0.01 else (1.0 if d > 0 else -1.0)

        # the height held when the pilot is not climbing or descending
        # the height held once the pilot lets go: first level off (no climb), then hold where the climb stopped
        if vertical != 0 or on_ground:
            self.hold_height = None
        elif self.hold_height is None and abs(s['climbRate']) < LEVEL_RATE:
            self.hold_height = s['height']

        hover = self._hover(c, s, dt, turn, vertical, on_ground)
        flight = self._flight(c, s, dt, turn, vertical)
        # between the two (the nacelles half way) the inputs are blended by the tilt
        w = clamp((s['tilt'] - 0.3) / 0.4, 0.0, 1.0)
        w = w * w * (3 - 2 * w)
        for k in ('throttle', 'steering', 'pitch'):
            c[k] = w * hover[k] + (1 - w) * flight[k]
        if on_ground and vertical <= 0 and self.speed_target < 1:
            c['throttle'] = 0.0
        c['throttle'] = clamp(c['throttle'], 0.0, 1.0)
        c['steering'] = clamp(c['steering'], -1.0, 1.0)
        c['pitch'] = clamp(c['pitch'], -1.0, 1.0)
        return c

    def _hover(self, c, s, dt, turn, vertical, on_ground):
        h = {}
        if vertical != 0:
            climb_wanted = vertical * CLIMB_RATE
        elif self.hold_height is None:
            climb_wanted = 0.0
        else:
            climb_wanted = clamp(0.15 * (self.hold_height - s['height']), -2, 2)
        if climb_wanted < 0:
            climb_wanted = max(climb_wanted, -max(0.4, 0.5 * s['aboveGround']))   # gently onto the ground
        upward = max(0.35, math.sin(s['tilt'] * math.pi / 2))
        wing_share = clamp(s['wingLift'] / WEIGHT, 0.0, 1.0)
        h['throttle'] = (HOVER_THROTTLE * (1 - wing_share) + 0.08 * (climb_wanted - s['climbRate'])) / upward
        # along the nose: the speed wanted, by the pitch attitude through the hover's torque balance (the trim fed
        # forward, the rest learned); no sideways thrust in a hover, so the pilot's A/D turn the nose
        along_wanted = min(self.speed_target, HOVER_MAX_ALONG)
        e_along = along_wanted - s['speedAlong']
        attitude = clamp(3.0 * e_along, -20.0, 20.0)   # degrees nose-down wanted
        if s['airspeed'] > 15:
            # braking from forward flight the wing still flies: not past its stall
            attitude = max(attitude, -max(0.0, ALPHA_MAX - 3.0 + (s['pitchDeg'] - s['alpha'])))   # nose-up at most the path + 8
        if not on_ground:
            self.hover_trim = clamp(self.hover_trim + 0.05 * (-attitude - s['pitchDeg']) * dt, -0.3, 0.3)
        h['pitch'] = HOVER_TRIM + self.hover_trim - attitude / HOVER_ATTITUDE
        # the turn, with its rate damped; held back while pitched (a yaw about a tilted axis banks the craft)
        yaw_wanted = turn * YAW_RATE
        h['steering'] = clamp(0.05 * (yaw_wanted - s['yawRate']), -1, 1) * 3.0 / max(3.0, abs(s['pitchDeg']))
        return h

    def _flight(self, c, s, dt, turn, vertical):
        f = {}
        # the bank wanted moves toward the pilot's A/D (or back to level) at a rate
        target = turn * BANK_MAX
        step = BANK_RATE * dt
        self.bank_wanted += clamp(target - self.bank_wanted, -step, step)
        f['steering'] = clamp(0.03 * (self.bank_wanted - s['bank']) - 0.005 * s['rollRate'], -0.6, 0.6)
        # the flight path wanted: the pilot's climb or descent, else the height held; the attitude for it is that
        # path plus the wing's angle of attack, more in a bank (the lift tilts away), capped short of the stall
        if vertical != 0:
            gamma_wanted = vertical * CLIMB_ANGLE
        else:
            gamma_wanted = 0.0 if self.hold_height is None else clamp(0.4 * (self.hold_height - s['height']), -6.0, 6.0)
        if s['airspeed'] < WING_SPEED:
            gamma_wanted = min(gamma_wanted, 0.0)        # too slow to climb on the wing: speed first
        alpha = clamp(s['alpha'], -5.0, ALPHA_MAX)
        pitch_wanted = gamma_wanted + alpha
        error = pitch_wanted - s['pitchDeg']
        self.pitch_trim = clamp(self.pitch_trim + 0.02 * error * dt, -0.5, 0.5)
        # a bank needs more lift for the same height: the elevator for it, fed forward, so the trim does not wind up in a
        # turn and balloon the craft when it rolls level
        bank = math.radians(min(abs(s['bank']), 60.0))
        turn_elevator = TURN_ELEVATOR * (1.0 / math.cos(bank) - 1.0)
        f['pitch'] = 0.08 * error - 0.01 * s['pitchRate'] + self.pitch_trim + turn_elevator
        # the throttle holds the speed
        speed_wanted = max(self.speed_target, WING_SPEED)
        e = speed_wanted - s['airspeed']
        self.speed_integral = clamp(self.speed_integral + 0.01 * e * dt, -0.3, 0.6)
        f['throttle'] = 0.1 + 0.08 * e + self.speed_integral + 0.4 * math.sin(math.radians(clamp(gamma_wanted, 0, 20)))
        return f


# ---- the craft's state as the script reads it ----
def state(craft, ground, prev):
    q = craft.q
    f = fr.qrot(q, fr.FORWARD)
    v = craft.v
    tele = craft.tele or {}
    flat = np.array([f[0], 0.0, f[2]])
    flat = flat / max(1e-6, np.linalg.norm(flat))
    s = {
        'height': craft.p[1], 'aboveGround': craft.p[1] - 0.2 - ground, 'climbRate': v[1],
        'speedAlong': float(np.dot(v, flat)), 'airspeed': tele.get('airspeed', float(np.linalg.norm(v))),
        'alpha': tele.get('angleOfAttack', 0.0), 'wingLift': tele.get('wingLift', 0.0), 'tilt': craft.tilt,
        'pitchDeg': craft.pitch(), 'bank': craft.bank(), 'heading': craft.heading(),
    }
    if prev is None:
        s['pitchRate'] = s['rollRate'] = s['yawRate'] = 0.0
    else:
        dt = prev['dt']
        s['pitchRate'] = (s['pitchDeg'] - prev['pitchDeg']) / dt
        s['rollRate'] = wrap(s['bank'] - prev['bank']) / dt
        s['yawRate'] = wrap(s['heading'] - prev['heading']) / dt
    return s


COURSE = [
    # (name, seconds, keys held)
    ("on the ground", 3, {}),
    ("lift off (Up)", 6, {'up': 1}),
    ("hover, hands off", 4, {}),
    ("turn in hover (A)", 4, {'left': 1}),
    ("creep forward (W briefly)", 0.6, {'faster': 1}),
    ("creeping, hands off", 6, {}),
    ("stop in hover (X)", 0.1, {'stop': 1}),
    ("stopping, hands off", 6, {}),
    ("speed up (W)", 5, {'faster': 1}),
    ("transition, hands off", 20, {}),
    ("cruise, hands off", 10, {}),
    ("turn left (A)", 12, {'left': 1}),
    ("level, hands off", 8, {}),
    ("climb (Up)", 6, {'up': 1}),
    ("hold height, hands off", 8, {}),
    ("turn right (D)", 12, {'right': 1}),
    ("descend (Down)", 5, {'down': 1}),
    ("level, hands off 2", 8, {}),
    ("stop (X)", 1, {'stop': 1}),
    ("slowing to hover, hands off", 40, {}),
    ("hover, hands off 2", 6, {}),
    ("descend to land (Down)", 12, {'down': 1}),
    ("landed", 4, {}),
]


def run(course=COURSE, frame=1.0 / 60.0, verbose=True):
    ground = 0.0
    craft = fr.Craft(position=(0, 0.2, 0), tilt=1.0, ground=ground)
    assist = Assist()
    prev = None
    t = 0.0
    rows = []
    substeps_per_frame = frame / fr.SUBSTEP
    acc = 0.0
    craft.step({'throttle': 0.0})
    for name, seconds, keys in course:
        end = t + seconds
        while t < end - 1e-9:
            s = state(craft, ground, prev)
            s['dt'] = frame
            c = assist.controls(keys, s, frame)
            acc += substeps_per_frame
            while acc >= 1.0:
                craft.step(c)
                acc -= 1.0
            prev = s
            t += frame
            rows.append((name, t, s, c, assist.speed_target, assist.bank_wanted, assist.hold_height))
    if verbose:
        summarize(rows)
    return rows


def summarize(rows):
    print("%-28s %6s %7s %7s %7s %7s %7s %7s %7s %7s" % ("phase", "end s", "height", "speed", "tilt", "pitch",
                                                           "bank", "alpha", "maxAoA", "hold err"))
    by = []
    for r in rows:
        if not by or by[-1][0] != r[0]:
            by.append((r[0], []))
        by[-1][1].append(r)
    for name, rs in by:
        s, c = rs[-1][2], rs[-1][3]
        max_alpha = max(abs(r[2]['alpha']) for r in rs if r[2]['airspeed'] > 5) if any(r[2]['airspeed'] > 5 for r in rs) else 0.0
        held = [abs(r[2]['height'] - r[6]) for r in rs if r[6] is not None]
        hold = max(held) if held and ('hands off' in name or 'level' in name) else float('nan')
        print("%-28s %6.1f %7.1f %7.1f %7.2f %7.1f %7.1f %7.1f %7.1f %7.2f" % (
            name, rs[-1][1], s['height'], s['airspeed'], s['tilt'], s['pitchDeg'], s['bank'], s['alpha'], max_alpha, hold))


if __name__ == '__main__':
    run()
