"use strict";
//
//  flightAssist.js
//  scripts/system/libraries
//
//  Flight assistance for an aircraft (vehicle.type "aircraft"), so it flies the way the planes in San Andreas do: the
//  pilot's keys give an intent, and the assistant turns the intent and the craft's state into the model's inputs
//  (throttle, steering, pitch, and the nacelles' tilt, as "lift") every frame.
//
//    W / S        faster / slower: the speed wanted, which stays where it is left (X: stop, which ends in a hover)
//    A / D        bank left / right in flight, turn left / right in a hover; let go and it levels
//    Up / Down    climb / descend; let go and it levels off and holds that height
//
//  A tilt-rotor hovers below 12 m/s and flies on its wing from 28 m/s: the nacelles follow the airspeed, so taking
//  off is climbing from the ground, speeding up turns it into a plane, and slowing down turns it back into a hover.
//  The laws were designed on a replica of the aircraft (tests-manual/physics-baseline/flight-assist-replica.py, the
//  same law in Python, with the course it was tested on); keep the two the same.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

/* exported FlightAssist */

var FlightAssist = (function () {
    var WEIGHT_PER_KG = 9.8;
    var HOVER_THROTTLE = 0.59;            // the thrust that carries the weight (the test aircraft's, 1,200 kg on 20 kN)
    var HOVER_TRIM = 0.5 / (0.5 * 4.5);   // the pitch input that holds the nose level in hover (the thrust point is aft)
    var HOVER_ATTITUDE = 24.0;            // degrees nose-down per unit of pitch input in hover
    var SPEED_RATE = 10.0;                // m/s per second while W or S is held
    var MAX_SPEED = 55.0;
    var HOVER_TOP = 12.0;                 // m/s: below it the rotors stand up (hover)
    var WING_SPEED = 28.0;                // m/s: above it the rotors lie forward (the wing carries the craft)
    var HOVER_MAX_ALONG = 14.0;           // m/s: the fastest the hover law asks for along the nose
    var BANK_MAX = 40.0;                  // degrees with A or D held
    var BANK_RATE = 60.0;                 // degrees per second the wanted bank moves
    var CLIMB_ANGLE = 12.0;               // degrees of flight path with Up or Down held, in flight
    var CLIMB_RATE = 4.0;                 // m/s with Up or Down held, in hover
    var YAW_RATE = 40.0;                  // degrees per second with A or D held, in hover
    var ALPHA_MAX = 11.0;                 // degrees: the wing stalls at 15
    var TURN_ELEVATOR = 0.3;              // pitch input per unit of (1/cos(bank) - 1)
    var LEVEL_RATE = 0.5;                 // m/s: letting go, the craft levels off and holds the height where its climb fell below this

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }
    function wrap(d) { while (d > 180) { d -= 360; } while (d < -180) { d += 360; } return d; }
    function radians(d) { return d * Math.PI / 180; }
    function degrees(r) { return r * 180 / Math.PI; }

    // The craft's state as the laws read it. properties: the entity's position, rotation, velocity; live: the vehicle
    // simulation's telemetry for the craft this client drives (Entities.getVehicleStats().live), or null; aboveGround:
    // metres from its belly to the ground below; previous: the last state (for the rates), or null.
    function state(properties, live, aboveGround, previous, dt) {
        var rotation = properties.rotation;
        var front = Quat.getFront(rotation), right = Quat.getRight(rotation), up = Quat.getUp(rotation);
        var v = properties.velocity;
        var flatLength = Math.max(1e-6, Math.sqrt(front.x * front.x + front.z * front.z));
        var s = {
            height: properties.position.y,
            aboveGround: aboveGround,
            climbRate: v.y,
            speedAlong: (v.x * front.x + v.z * front.z) / flatLength,
            airspeed: live && live.airspeed !== undefined ? live.airspeed : Vec3.length(v),
            alpha: live && live.angleOfAttack !== undefined ? live.angleOfAttack : 0,
            wingLift: live && live.wingLift !== undefined ? live.wingLift : 0,
            tilt: live && live.tilt !== undefined ? live.tilt : 0,
            pitchDeg: degrees(Math.asin(clamp(front.y, -1, 1))),
            bank: degrees(Math.atan2(-right.y, up.y)),
            heading: degrees(Math.atan2(front.x, -front.z)),
            dt: dt
        };
        if (!previous || !(dt > 0)) {
            s.pitchRate = s.rollRate = s.yawRate = 0;
        } else {
            s.pitchRate = (s.pitchDeg - previous.pitchDeg) / dt;
            s.rollRate = wrap(s.bank - previous.bank) / dt;
            s.yawRate = wrap(s.heading - previous.heading) / dt;
        }
        return s;
    }

    function Assist(mass) {
        this.weight = (mass || 1200) * WEIGHT_PER_KG;
        this.speedTarget = 0;
        this.bankWanted = 0;
        this.holdHeight = null;
        this.hoverTrim = 0;      // the hover's pitch trim, learned from the measured attitude
        this.pitchTrim = 0;      // the flight's elevator trim, learned from the attitude error
        this.speedIntegral = 0;
        this.mode = "hover";
    }

    // keys: { faster, slower, left, right, up, down, stop }, each 0 or 1. Returns the model's inputs.
    Assist.prototype.controls = function (keys, s, dt) {
        var c = { throttle: 0, steering: 0, pitch: 0, lift: 0, trim: 0 };
        this.speedTarget = clamp(this.speedTarget + SPEED_RATE * dt * ((keys.faster || 0) - (keys.slower || 0)), 0, MAX_SPEED);
        if (keys.stop) {
            this.speedTarget = 0;
        }
        var turn = (keys.right || 0) - (keys.left || 0);
        var vertical = (keys.up || 0) - (keys.down || 0);
        var onGround = s.aboveGround < 0.6 && Math.abs(s.climbRate) < 0.5;

        // the nacelles by the airspeed: up below the hover's top speed, forward from the wing's
        var tiltWanted = clamp((WING_SPEED - s.airspeed) / (WING_SPEED - HOVER_TOP), 0, 1);
        if (this.speedTarget <= HOVER_TOP) {
            tiltWanted = s.airspeed < WING_SPEED + 4 ? 1 : tiltWanted;
        }
        var d = tiltWanted - s.tilt;
        c.lift = Math.abs(d) < 0.01 ? 0 : (d > 0 ? 1 : -1);

        // the height held once the pilot lets go: first level off (no climb), then hold where the climb stopped
        if (vertical !== 0 || onGround) {
            this.holdHeight = null;
        } else if (this.holdHeight === null && Math.abs(s.climbRate) < LEVEL_RATE) {
            this.holdHeight = s.height;
        }

        var hover = this.hover(s, dt, turn, vertical, onGround);
        var flight = this.flight(s, dt, turn, vertical);
        // between the two (the nacelles half way) the inputs are blended by the tilt
        var w = clamp((s.tilt - 0.3) / 0.4, 0, 1);
        w = w * w * (3 - 2 * w);
        this.mode = w > 0.5 ? "hover" : "flight";
        ["throttle", "steering", "pitch"].forEach(function (k) { c[k] = w * hover[k] + (1 - w) * flight[k]; });
        if (onGround && vertical <= 0 && this.speedTarget < 1) {
            c.throttle = 0;
        }
        c.throttle = clamp(c.throttle, 0, 1);
        c.steering = clamp(c.steering, -1, 1);
        c.pitch = clamp(c.pitch, -1, 1);
        return c;
    };

    Assist.prototype.hover = function (s, dt, turn, vertical, onGround) {
        var h = {};
        var climbWanted;
        if (vertical !== 0) {
            climbWanted = vertical * CLIMB_RATE;
        } else if (this.holdHeight === null) {
            climbWanted = 0;
        } else {
            climbWanted = clamp(0.15 * (this.holdHeight - s.height), -2, 2);
        }
        if (climbWanted < 0) {
            climbWanted = Math.max(climbWanted, -Math.max(0.4, 0.5 * s.aboveGround));   // gently onto the ground
        }
        var upward = Math.max(0.35, Math.sin(s.tilt * Math.PI / 2));
        var wingShare = clamp(s.wingLift / this.weight, 0, 1);
        h.throttle = (HOVER_THROTTLE * (1 - wingShare) + 0.08 * (climbWanted - s.climbRate)) / upward;
        // along the nose: the speed wanted, by the pitch attitude through the hover's torque balance (the trim fed
        // forward, the rest learned); no sideways thrust in a hover, so the pilot's A/D turn the nose
        var alongWanted = Math.min(this.speedTarget, HOVER_MAX_ALONG);
        var attitude = clamp(3.0 * (alongWanted - s.speedAlong), -20, 20);   // degrees nose-down wanted
        if (s.airspeed > 15) {
            // braking from forward flight the wing still flies: nose-up at most the flight path plus 8 degrees
            attitude = Math.max(attitude, -Math.max(0, ALPHA_MAX - 3 + (s.pitchDeg - s.alpha)));
        }
        if (!onGround) {
            this.hoverTrim = clamp(this.hoverTrim + 0.05 * (-attitude - s.pitchDeg) * dt, -0.3, 0.3);
        }
        h.pitch = HOVER_TRIM + this.hoverTrim - attitude / HOVER_ATTITUDE;
        // the turn, its rate held; held back while pitched (a yaw about a tilted axis banks the craft)
        h.steering = clamp(0.05 * (turn * YAW_RATE - s.yawRate), -1, 1) * 3.0 / Math.max(3.0, Math.abs(s.pitchDeg));
        return h;
    };

    Assist.prototype.flight = function (s, dt, turn, vertical) {
        var f = {};
        // the bank wanted moves toward the pilot's A/D (or back to level) at a rate
        var step = BANK_RATE * dt;
        this.bankWanted += clamp(turn * BANK_MAX - this.bankWanted, -step, step);
        f.steering = clamp(0.03 * (this.bankWanted - s.bank) - 0.005 * s.rollRate, -0.6, 0.6);
        // the flight path wanted: the pilot's climb or descent, else level off and hold the height; the attitude is
        // that path plus the wing's angle of attack, capped short of the stall
        var gammaWanted;
        if (vertical !== 0) {
            gammaWanted = vertical * CLIMB_ANGLE;
        } else {
            gammaWanted = this.holdHeight === null ? 0 : clamp(0.4 * (this.holdHeight - s.height), -6, 6);
        }
        if (s.airspeed < WING_SPEED) {
            gammaWanted = Math.min(gammaWanted, 0);   // too slow to climb on the wing: speed first
        }
        var pitchWanted = gammaWanted + clamp(s.alpha, -5, ALPHA_MAX);
        var error = pitchWanted - s.pitchDeg;
        this.pitchTrim = clamp(this.pitchTrim + 0.02 * error * dt, -0.5, 0.5);
        // a bank needs more lift for the same height: its elevator fed forward
        var bank = radians(Math.min(Math.abs(s.bank), 60));
        f.pitch = 0.08 * error - 0.01 * s.pitchRate + this.pitchTrim + TURN_ELEVATOR * (1 / Math.cos(bank) - 1);
        // the throttle holds the speed
        var e = Math.max(this.speedTarget, WING_SPEED) - s.airspeed;
        this.speedIntegral = clamp(this.speedIntegral + 0.01 * e * dt, -0.3, 0.6);
        f.throttle = 0.1 + 0.08 * e + this.speedIntegral + 0.4 * Math.sin(radians(clamp(gammaWanted, 0, 20)));
        return f;
    };

    return {
        state: state,
        create: function (mass) { return new Assist(mass); },
        HOVER_TOP: HOVER_TOP,
        WING_SPEED: WING_SPEED
    };
}());
// Script.include evaluates a file in the scope of the function that called it, so a var here is not seen by an including
// script whose include sits inside its own function (vehicles.js's): put it on the global object as well
Function("return this")().FlightAssist = FlightAssist;
