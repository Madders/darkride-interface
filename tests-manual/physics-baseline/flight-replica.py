#!/usr/bin/env python3
"""A replica of the tilt-rotor aircraft (libraries/vehicles/src/AircraftModel.cpp with aircraft-rig.js's spec) as a
rigid body, for designing flight assistance off line: forces() is AircraftModel::forces line for line, step() a 90 Hz
substep of the body (Jolt's: velocity from force, spin from torque through the world inertia, angular damping, then
the pose). check() compares the replica's wing with the model's own telemetry over a recorded flight
(results/2026-10-14-round8-aircraft-serverless.json.gz, "live" in each sample).

    flight-replica.py check [log.json.gz]
"""
import gzip
import json
import math
import sys

import numpy as np

SUBSTEP = 1.0 / 90.0
GRAVITY = 9.8

# aircraft-rig.js's SPEC
SPEC = dict(
    mass=1200.0,
    centerOfMass=np.array([0.0, -0.1, 0.0]),
    maxThrust=20000.0,
    thrustPoint=np.array([0.0, 0.2, 0.5]),
    wingArea=24.0,
    wingSpan=11.0,
    wingPoint=np.array([0.0, 0.2, 0.3]),
    elevatorArea=4.0,
    rudderArea=2.0,
    finPoint=np.array([0.0, 0.5, 4.5]),
    maxFinAngle=30.0,
    dragArea=np.array([3.0, 8.0, 0.8]),
    angularDragArea=np.array([800.0, 800.0, 600.0]),
)
BODY = np.array([2.4, 0.4, 5.0])          # the hull box, for the inertia
ANGULAR_DAMPING = 0.05                     # the entity's

# AircraftModel.h and AirshipModel.h
TILT_RATE = 0.35
STALL_ANGLE = 0.26
PROFILE_DRAG = 0.03
OSWALD = 0.8
AILERON_FRACTION = 0.4
RUDDER_FRACTION = 0.5
HOVER_PITCH_LEVER = 0.5
HOVER_YAW_LEVER = 0.25
HOVER_LEVEL_LEVER = 0.5
HOVER_DAMPING_LEVER = 0.3
FIN_LIFT_SLOPE = 3.0
FIN_STALL = 1.2

UP = np.array([0.0, 1.0, 0.0])
RIGHT = np.array([1.0, 0.0, 0.0])
FORWARD = np.array([0.0, 0.0, -1.0])


def clamp1(v):
    return max(-1.0, min(1.0, v))


# quaternions as (w, x, y, z)
def qmul(a, b):
    w1, x1, y1, z1 = a
    w2, x2, y2, z2 = b
    return np.array([w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                     w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2])


def qrot(q, v):
    w, x, y, z = q
    u = np.array([x, y, z])
    return v + 2.0 * np.cross(u, np.cross(u, v) + w * v)


def qconj(q):
    return np.array([q[0], -q[1], -q[2], -q[3]])


def qaxis(axis, angle):
    n = np.linalg.norm(axis)
    if n < 1e-12:
        return np.array([1.0, 0.0, 0.0, 0.0])
    s = math.sin(angle / 2) / n
    return np.array([math.cos(angle / 2), axis[0] * s, axis[1] * s, axis[2] * s])


def qmat(q):
    return np.column_stack([qrot(q, np.eye(3)[i]) for i in range(3)])


def air_density(altitude):
    h = min(max(altitude, 0.0), 11000.0)
    return 1.225 * (1.0 - 2.25577e-5 * h) ** 4.25588


def lift_coefficient(alpha, ar):
    slope = 2 * math.pi * ar / (ar + 2)
    return slope * alpha if abs(alpha) < STALL_ANGLE else math.sin(2 * alpha)


def drag_coefficient(alpha, cl, ar):
    induced = cl * cl / (math.pi * ar * OSWALD) if ar > 0 else 0.0
    stalled = 0.0 if abs(alpha) < STALL_ANGLE else 1.2 * math.sin(alpha) ** 2
    return PROFILE_DRAG + induced + stalled


def fin_force(normal, hinge, deflection, wind, area, rho):
    speed2 = float(np.dot(wind, wind))
    if area <= 0 or speed2 < 1e-6:
        return np.zeros(3)
    n = qrot(qaxis(hinge, deflection), normal)
    n = n / np.linalg.norm(n)
    sin_alpha = float(np.dot(wind, n)) / math.sqrt(speed2)
    c = max(-FIN_STALL, min(FIN_STALL, FIN_LIFT_SLOPE * sin_alpha))
    return 0.5 * rho * speed2 * area * c * n


def forces(spec, tilt, inp, position, rotation, velocity, spin, wind=np.zeros(3)):
    """AircraftModel::forces: [(force, point)], torque (world), and telemetry."""
    out = []
    up, forward, right = qrot(rotation, UP), qrot(rotation, FORWARD), qrot(rotation, RIGHT)
    to_local = qconj(rotation)
    tilt = min(max(tilt, 0.0), 1.0)
    world_point = lambda local: position + qrot(rotation, local - spec['centerOfMass'])
    thrust_point = world_point(spec['thrustPoint'])
    fin_point = world_point(spec['finPoint'])
    rho = air_density(position[1])
    angle = tilt * math.pi / 2
    thrust_direction = forward * math.cos(angle) + up * math.sin(angle)
    thrust_direction /= np.linalg.norm(thrust_direction)
    thrust = min(max(inp.get('throttle', 0.0), 0.0), 1.0) * spec['maxThrust']
    if thrust > 0:
        out.append((thrust_direction * thrust, thrust_point))
    ar = spec['wingSpan'] ** 2 / spec['wingArea']
    aileron = clamp1(inp.get('steering', 0.0)) * math.radians(spec['maxFinAngle']) * AILERON_FRACTION
    lift_sum = 0.0
    tele = {}
    wing_wind = None
    for side in (0, 1):
        sign = 1.0 if side == 0 else -1.0
        local_point = spec['wingPoint'] + RIGHT * (sign * 0.25 * spec['wingSpan'])
        point = world_point(local_point)
        relative = wind - (velocity + np.cross(spin, point - position))
        local_wind = qrot(to_local, relative)
        if side == 0:
            wing_wind = relative
        flow = np.array([0.0, local_wind[1], local_wind[2]])
        speed2 = float(np.dot(flow, flow))
        if speed2 < 1e-4:
            continue
        speed = math.sqrt(speed2)
        alpha = math.atan2(flow[1], flow[2]) - sign * aileron
        cl = lift_coefficient(alpha, ar)
        cd = drag_coefficient(alpha, cl, ar)
        q = 0.5 * rho * speed2 * 0.5 * spec['wingArea']
        fd = flow / speed
        ld = np.array([0.0, fd[2], -fd[1]])
        out.append((qrot(rotation, ld * (q * cl) + fd * (q * cd)), point))
        lift_sum += q * cl
    local_wing_wind = qrot(to_local, wing_wind)
    tele['airspeed'] = float(np.linalg.norm(wing_wind))
    tele['angleOfAttack'] = math.degrees(math.atan2(local_wing_wind[1], local_wing_wind[2])) if tele['airspeed'] > 1e-3 else 0.0
    tele['wingLift'] = lift_sum
    wind_fins = wind - (velocity + np.cross(spin, fin_point - position))
    max_angle = math.radians(spec['maxFinAngle'])
    elevator = fin_force(up, right, -clamp1(inp.get('pitch', 0.0)) * max_angle, wind_fins, spec['elevatorArea'], rho)
    rudder = fin_force(right, up, clamp1(inp.get('steering', 0.0)) * max_angle * RUDDER_FRACTION, wind_fins, spec['rudderArea'], rho)
    out.append((elevator + rudder, fin_point))
    local_wind = qrot(to_local, wind - velocity)
    drag = 0.5 * rho * spec['dragArea'] * local_wind * np.abs(local_wind)
    out.append((qrot(rotation, drag), position))
    local_spin = qrot(to_local, spin)
    torque = -0.5 * rho * spec['angularDragArea'] * local_spin * np.abs(local_spin)
    if tilt > 0 and thrust > 0:
        authority = tilt * thrust
        tail_arm = abs(spec['finPoint'][2] - spec['centerOfMass'][2])
        torque[0] += clamp1(inp.get('pitch', 0.0)) * authority * HOVER_PITCH_LEVER * tail_arm
        torque[1] += -clamp1(inp.get('steering', 0.0)) * authority * HOVER_YAW_LEVER * spec['wingSpan']
        world_up_local = qrot(to_local, UP)
        torque += np.cross(UP, world_up_local) * (authority * HOVER_LEVEL_LEVER * spec['wingSpan'])
        torque -= local_spin * (authority * HOVER_DAMPING_LEVER * spec['wingSpan'])
    return out, qrot(rotation, torque), tele


class Craft:
    def __init__(self, spec=SPEC, position=(0, 100, 0), rotation=(1, 0, 0, 0), velocity=(0, 0, 0), spin=(0, 0, 0), tilt=0.0,
                 ground=None):
        self.spec = spec
        self.p = np.array(position, dtype=float)
        self.q = np.array(rotation, dtype=float)
        self.v = np.array(velocity, dtype=float)
        self.w = np.array(spin, dtype=float)
        self.tilt = tilt
        m = spec['mass']
        x, y, z = BODY
        self.inertia = np.diag([m / 12 * (y * y + z * z), m / 12 * (x * x + z * z), m / 12 * (x * x + y * y)])
        self.ground = ground       # a height, or None: a crude floor for the belly
        self.tele = {}

    def step(self, inp, dt=SUBSTEP):
        self.tilt = min(max(self.tilt + clamp1(inp.get('lift', 0.0)) * TILT_RATE * dt, 0.0), 1.0)
        fs, torque, self.tele = forces(self.spec, self.tilt, inp, self.p, self.q, self.v, self.w)
        f = np.array([0.0, -GRAVITY * self.spec['mass'], 0.0])
        t = np.array(torque)
        for force, point in fs:
            f += force
            t += np.cross(point - self.p, force)
        r = qmat(self.q)
        inv_world = r @ np.linalg.inv(self.inertia) @ r.T
        self.v += f / self.spec['mass'] * dt
        self.w += inv_world @ t * dt
        self.w *= max(0.0, 1.0 - ANGULAR_DAMPING * dt)
        if self.ground is not None and self.p[1] < self.ground + 0.2:
            # the belly on a flat floor: no going under, sliding with friction, a damped levelling
            self.p[1] = self.ground + 0.2
            if self.v[1] < 0:
                self.v[1] = 0.0
            horizontal = np.array([self.v[0], 0.0, self.v[2]])
            speed = np.linalg.norm(horizontal)
            if speed > 0:
                self.v -= horizontal / speed * min(speed, 0.8 * GRAVITY * dt)
            self.w *= 0.9
        self.p += self.v * dt
        angle = np.linalg.norm(self.w) * dt
        if angle > 0:
            self.q = qmul(qaxis(self.w, angle), self.q)
            self.q /= np.linalg.norm(self.q)

    # attitude in the scripts' terms (Quat.safeEulerAngles: pitch about x, yaw about y, roll about z, degrees)
    def heading(self):
        f = qrot(self.q, FORWARD)
        return math.degrees(math.atan2(f[0], -f[2]))

    def pitch(self):
        f = qrot(self.q, FORWARD)
        return math.degrees(math.asin(max(-1.0, min(1.0, f[1]))))

    def bank(self):
        r = qrot(self.q, RIGHT)
        u = qrot(self.q, UP)
        return math.degrees(math.atan2(-r[1], u[1]))


def check(path):
    d = json.load(gzip.open(path))
    rows = []
    for s in d['samples']:
        live = s.get('live') or {}
        if not s.get('rot') or live.get('wingLift') is None or not s.get('controls'):
            continue
        q = np.array([s['rot']['w'], s['rot']['x'], s['rot']['y'], s['rot']['z']])
        v = np.array([s['vel'][k] for k in 'xyz'])
        w = np.array([s['angVel'][k] for k in 'xyz'])
        p = np.array([s['pos'][k] for k in 'xyz'])
        # the samples' position is the entity's centre; the model's is the centre of mass
        com = p + qrot(q, SPEC['centerOfMass'])
        _, _, tele = forces(SPEC, live.get('tilt') or 0.0, s['controls'], com, q, v, w)
        rows.append((s['phase'], live['airspeed'], tele['airspeed'], live['angleOfAttack'], tele['angleOfAttack'],
                     live['wingLift'], tele['wingLift']))
    print(len(rows), "samples with telemetry")
    by = {}
    for r in rows:
        by.setdefault(r[0], []).append(r)
    print("%-12s %5s %16s %16s %22s" % ("phase", "n", "airspeed err", "alpha err (deg)", "lift err (N, rel)"))
    for phase, rs in by.items():
        a = np.array([r[1:] for r in rs], dtype=float)
        lift_rel = np.abs(a[:, 5] - a[:, 4]) / np.maximum(1000.0, np.abs(a[:, 4]))
        print("%-12s %5d %8.3f %7.3f %8.3f %7.3f %10.0f %8.3f" % (
            phase or "-", len(rs), np.mean(np.abs(a[:, 1] - a[:, 0])), np.max(np.abs(a[:, 1] - a[:, 0])),
            np.mean(np.abs(a[:, 3] - a[:, 2])), np.max(np.abs(a[:, 3] - a[:, 2])),
            np.mean(np.abs(a[:, 5] - a[:, 4])), np.mean(lift_rel)))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'check':
        check(sys.argv[2] if len(sys.argv) > 2 else 'results/2026-10-14-round8-aircraft-serverless.json.gz')
    else:
        print(__doc__)
