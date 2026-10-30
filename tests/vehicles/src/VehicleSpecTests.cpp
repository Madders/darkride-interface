//
//  VehicleSpecTests.cpp
//  tests/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "VehicleSpecTests.h"

#include <cmath>

#include <VehicleInput.h>
#include <WheeledVehicleSpec.h>

// Add additional qtest functionality (the include order is important!)
#include <test-utils/QTestExtensions.h>

QTEST_MAIN(VehicleSpecTests)

namespace {
    // the Series III's four wheels: wheelbase 2.235 m, track 1.308 m, 16 inch wheels (PHYSICS.md round 6)
    const QString SERIES_III_WHEELS =
        "[{\"name\":\"FL\",\"center\":[-0.654,0.1,-1.1175],\"radius\":0.37,\"width\":0.16,\"steer\":1},"
        " {\"name\":\"FR\",\"center\":{\"x\":0.654,\"y\":0.1,\"z\":-1.1175},\"radius\":0.37,\"width\":0.16,\"steer\":1},"
        " {\"name\":\"RL\",\"center\":[-0.654,0.1,1.1175],\"radius\":0.37,\"width\":0.16},"
        " {\"name\":\"RR\",\"center\":[0.654,0.1,1.1175],\"radius\":0.37,\"width\":0.16,\"handBrake\":1}]";
}

void VehicleSpecTests::wheelsFromJSON() {
    std::vector<WheelSpec> wheels;
    QString error;
    QVERIFY(WheeledVehicleSpec::parseWheels(SERIES_III_WHEELS, wheels, &error));
    QCOMPARE((int)wheels.size(), 4);
    QCOMPARE(wheels[0].name, QString("FL"));
    QCOMPARE(wheels[1].center.x, 0.654f);   // an object with x, y, z reads as a list does
    QCOMPARE(wheels[0].radius, 0.37f);
    QCOMPARE(wheels[0].steer, 1.0f);
    QCOMPARE(wheels[2].steer, 0.0f);
    QCOMPARE(wheels[3].handBrake, 1.0f);
    QCOMPARE(wheels[2].handBrake, 0.0f);
    QCOMPARE(wheels[0].brake, 1.0f);
    QCOMPARE(wheels[0].drive, -1);           // unsaid: the drive type decides
    QCOMPARE(wheels[0].suspensionDistance, 0.2f);

    // not a list, bad JSON, and a wheel without a centre
    QVERIFY(!WheeledVehicleSpec::parseWheels("{\"a\":1}", wheels, &error));
    QVERIFY(wheels.empty());
    QVERIFY(!WheeledVehicleSpec::parseWheels("[{", wheels, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(WheeledVehicleSpec::parseWheels("[{\"radius\":1}, {\"center\":[0,0,0]}]", wheels, &error));
    QCOMPARE((int)wheels.size(), 1);
    QCOMPARE(wheels[0].name, QString("wheel0"));
    // nothing is an empty list
    QVERIFY(WheeledVehicleSpec::parseWheels("", wheels, &error));
    QVERIFY(wheels.empty());
}

void VehicleSpecTests::axlesAndDrive() {
    WheeledVehicleSpec spec;
    QVERIFY(WheeledVehicleSpec::parseWheels(SERIES_III_WHEELS, spec.wheels));
    std::vector<std::pair<int, int>> axles = spec.axles();
    QCOMPARE((int)axles.size(), 2);
    // front first (the smaller z), left before right
    QCOMPARE(axles[0].first, 0);
    QCOMPARE(axles[0].second, 1);
    QCOMPARE(axles[1].first, 2);
    QCOMPARE(axles[1].second, 3);

    spec.driveType = WheeledVehicleSpec::DriveType::REAR;
    QVERIFY(!spec.isDriven(0));
    QVERIFY(spec.isDriven(2));
    QVERIFY(spec.isDriven(3));
    spec.driveType = WheeledVehicleSpec::DriveType::FRONT;
    QVERIFY(spec.isDriven(0));
    QVERIFY(!spec.isDriven(3));
    spec.driveType = WheeledVehicleSpec::DriveType::ALL;
    QVERIFY(spec.isDriven(0) && spec.isDriven(3));
    // a wheel's own say wins
    spec.wheels[3].drive = 0;
    QVERIFY(!spec.isDriven(3));
    QVERIFY(spec.isDriven(2));

    QCOMPARE((int)WheeledVehicleSpec::parseDriveType("4x4"), (int)WheeledVehicleSpec::DriveType::ALL);
    QCOMPARE((int)WheeledVehicleSpec::parseDriveType("Front"), (int)WheeledVehicleSpec::DriveType::FRONT);
    QCOMPARE((int)WheeledVehicleSpec::parseDriveType("anything"), (int)WheeledVehicleSpec::DriveType::REAR);

    // a three-wheeler: the lone wheel is its own axle
    std::vector<WheelSpec> trike;
    QVERIFY(WheeledVehicleSpec::parseWheels("[{\"center\":[0,0,-1]},{\"center\":[-0.6,0,1]},{\"center\":[0.6,0,1]}]", trike));
    spec.wheels = trike;
    axles = spec.axles();
    QCOMPARE((int)axles.size(), 2);
    QCOMPARE(axles[0].first, axles[0].second);
}

void VehicleSpecTests::seatsAndGears() {
    WheeledVehicleSpec spec;
    QVERIFY(WheeledVehicleSpec::parseSeats("[{\"name\":\"passenger\",\"position\":[0.4,0.5,-0.2]},"
                                          " {\"name\":\"driver\",\"position\":[-0.4,0.5,-0.2],\"driver\":true,\"yaw\":0}]", spec.seats));
    QCOMPARE((int)spec.seats.size(), 2);
    QCOMPARE(spec.driverSeat(), 1);
    QCOMPARE(spec.seats[0].position.x, 0.4f);
    QVERIFY(!spec.seats[0].driver);
    QVERIFY(spec.seats[1].driver);
    QVERIFY(WheeledVehicleSpec::parseSeats("", spec.seats));
    QCOMPARE(spec.driverSeat(), -1);

    std::vector<float> ratios;
    QVERIFY(WheeledVehicleSpec::parseGearRatios("[3.68, 2.22, 1.5, 1.0]", ratios));
    QCOMPARE((int)ratios.size(), 4);
    QCOMPARE(ratios[0], 3.68f);
    QVERIFY(WheeledVehicleSpec::parseGearRatios("[3.68, -1, \"x\"]", ratios));
    QCOMPARE((int)ratios.size(), 1);
}

void VehicleSpecTests::inputPacket() {
    QUuid id = QUuid::createUuid();
    std::vector<VehicleInput> inputs;
    for (uint32_t i = 1; i <= 12; ++i) {
        VehicleInput input;
        input.sequence = i;
        input.throttle = 0.1f * i;
        input.steering = -0.05f * i;
        input.brake = i % 2 ? 0.0f : 1.0f;
        input.handBrake = i == 12 ? 1.0f : 0.0f;
        inputs.push_back(input);
    }
    QByteArray payload = VehicleInputPacket::pack(id, inputs);
    QCOMPARE(payload.size(), 16 + 1 + VehicleInputPacket::MAX_INPUTS * VehicleInputPacket::INPUT_BYTES);

    QUuid readID;
    std::vector<VehicleInput> read;
    QVERIFY(VehicleInputPacket::unpack(payload, readID, read));
    QCOMPARE(readID, id);
    // only the last MAX_INPUTS travel, oldest first
    QCOMPARE((int)read.size(), VehicleInputPacket::MAX_INPUTS);
    QCOMPARE(read.front().sequence, 12u - VehicleInputPacket::MAX_INPUTS + 1);
    QCOMPARE(read.back().sequence, 12u);
    QCOMPARE(read.back().handBrake, 1.0f);
    QCOMPARE(read.back().throttle, 1.2f);
    QVERIFY(read.back().sameControls(inputs.back()));

    // a short payload and a bad count are refused
    QVERIFY(!VehicleInputPacket::unpack(payload.left(10), readID, read));
    QByteArray corrupt = payload;
    corrupt[16] = (char)200;
    QVERIFY(!VehicleInputPacket::unpack(corrupt, readID, read));
    QVERIFY(read.empty());
}

// The driver's steering (WheeledVehicleSpec::steeringFor): eased toward the ask, and held under a 1 g turn at speed.
// Kart 2's numbers: a 1.0 m wheelbase and 50 degrees of lock (tests-manual/physics-baseline/kart-rig.js).
void VehicleSpecTests::steering() {
    WheeledVehicleSpec kart;
    QVERIFY(WheeledVehicleSpec::parseWheels(
        "[{\"center\":[-0.37,0,-0.5],\"steer\":1},{\"center\":[0.37,0,-0.5],\"steer\":1},"
        " {\"center\":[-0.37,0,0.5]},{\"center\":[0.37,0,0.5]}]", kart.wheels));
    kart.maxSteerAngle = 50.0f;
    QCOMPARE_WITH_ABS_ERROR(kart.wheelbase(), 1.0f, 1.0e-6f);
    const float DT = 1.0f / 90.0f;
    auto hold = [&](float from, float wanted, float speed, float seconds) {
        float steering = from;
        for (int i = 0; i < (int)roundf(seconds / DT); ++i) {
            steering = kart.steeringFor(steering, wanted, speed, DT);
        }
        return steering;
    };

    // standing: full lock in 0.2 s, half of it in 0.1, and back to straight in 0.12; the same to the left
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, 1.0f, 0.0f, 0.1f), 0.5f, 0.01f);
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, 1.0f, 0.0f, 0.2f), 1.0f, 1.0e-5f);
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, -1.0f, 0.0f, 0.2f), -1.0f, 1.0e-5f);
    QCOMPARE_WITH_ABS_ERROR(hold(1.0f, 0.0f, 0.0f, 0.12f), 0.0f, 1.0e-5f);
    QVERIFY(hold(1.0f, 0.0f, 0.0f, 0.06f) < 0.55f);
    // full lock to full lock the other way: back to the centre at the quicker rate, then out at the slower
    QCOMPARE_WITH_ABS_ERROR(hold(1.0f, -1.0f, 0.0f, 0.12f), 0.0f, 0.02f);
    QCOMPARE_WITH_ABS_ERROR(hold(1.0f, -1.0f, 0.0f, 0.32f), -1.0f, 1.0e-5f);
    // at walking pace the whole lock: atan(g L / v^2) at 2 m/s is 68 degrees, over the kart's 50
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, 1.0f, 2.0f, 0.5f), 1.0f, 1.0e-5f);
    // at 16 m/s a 1 g turn: atan(9.8 / 256) = 2.19 degrees of the 50
    float limit = atanf(9.8f / 256.0f) / glm::radians(50.0f);
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, 1.0f, 16.0f, 0.5f), limit, 1.0e-4f);
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, -1.0f, 16.0f, 0.5f), -limit, 1.0e-4f);
    // speeding up in a full-lock turn takes the lock down with the speed
    QCOMPARE_WITH_ABS_ERROR(hold(1.0f, 1.0f, 16.0f, 0.5f), limit, 1.0e-4f);
    // half the stick asks for half the lock while that is under the limit
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, 0.5f, 3.0f, 0.5f), 0.5f, 1.0e-5f);
    // an ask past the stick's ends is full lock; no time passing changes nothing
    QCOMPARE_WITH_ABS_ERROR(hold(0.0f, 3.0f, 0.0f, 0.5f), 1.0f, 1.0e-5f);
    QCOMPARE(kart.steeringFor(0.3f, 1.0f, 0.0f, 0.0f), 0.3f);

    // one axle: no wheelbase, so no limit at speed, but still the rate
    WheeledVehicleSpec trike;
    QVERIFY(WheeledVehicleSpec::parseWheels("[{\"center\":[-0.3,0,0],\"steer\":1},{\"center\":[0.3,0,0],\"steer\":1}]", trike.wheels));
    QCOMPARE(trike.wheelbase(), 0.0f);
    QCOMPARE_WITH_ABS_ERROR(trike.steeringFor(0.0f, 1.0f, 30.0f, 0.1f), 0.5f, 1.0e-5f);
}
