//
//  StreamTests.cpp
//  tests/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "StreamTests.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <SnapshotBuffer.h>
#include <VehicleState.h>

// Add additional qtest functionality (the include order is important!)
#include <test-utils/QTestExtensions.h>

QTEST_MAIN(StreamTests)

namespace {
    VehicleState stateAt(uint64_t time, const glm::vec3& position, uint32_t sequence = 0) {
        VehicleState s;
        s.id = QUuid("{11111111-2222-3333-4444-555555555555}");
        s.time = time;
        s.step = (uint32_t)(time / 11111);
        s.position = position;
        s.rotation = glm::angleAxis(0.001f * (float)sequence, glm::vec3(0.0f, 1.0f, 0.0f));
        s.velocity = glm::vec3(1.0f, 0.0f, 0.0f);
        s.angularVelocity = glm::vec3(0.0f, 0.1f, 0.0f);
        s.input.sequence = sequence;
        s.input.throttle = 0.5f;
        s.input.steering = -0.25f;
        s.engineRPM = 1500.0f + (float)sequence;
        s.gear = 2;
        s.ballast = 100.0f;
        s.fill = 0.9f;
        s.tilt = 0.3f;
        s.tick = sequence + 100;
        s.margin = (int8_t)((int)(sequence % 7) - 3);
        return s;
    }
}

void StreamTests::statePacketRoundTrip() {
    std::vector<VehicleState> states;
    for (int i = 0; i < 3; ++i) {
        states.push_back(stateAt(1000000ull * (i + 1), glm::vec3((float)i, 2.0f, -3.0f), 10 + i));
    }
    states[1].gear = -1;
    states[2].input.pitch = 1.0f;
    states[2].input.lift = -1.0f;
    states[2].input.trim = 0.5f;
    QByteArray payload = VehicleStatePacket::pack(states);
    QCOMPARE(payload.size(), 1 + 3 * VehicleStatePacket::STATE_BYTES);

    std::vector<VehicleState> out;
    QVERIFY(VehicleStatePacket::unpack(payload, out));
    QCOMPARE(out.size(), (size_t)3);
    for (int i = 0; i < 3; ++i) {
        QCOMPARE(out[i].id, states[i].id);
        QCOMPARE(out[i].time, states[i].time);
        QCOMPARE(out[i].step, states[i].step);
        QCOMPARE(out[i].position, states[i].position);
        QCOMPARE(out[i].rotation.w, states[i].rotation.w);
        QCOMPARE(out[i].rotation.y, states[i].rotation.y);
        QCOMPARE(out[i].velocity, states[i].velocity);
        QCOMPARE(out[i].angularVelocity, states[i].angularVelocity);
        QCOMPARE(out[i].input.sequence, states[i].input.sequence);
        QCOMPARE(out[i].input.throttle, states[i].input.throttle);
        QCOMPARE(out[i].input.steering, states[i].input.steering);
        QCOMPARE(out[i].engineRPM, states[i].engineRPM);
        QCOMPARE(out[i].gear, states[i].gear);
        QCOMPARE(out[i].ballast, states[i].ballast);
        QCOMPARE(out[i].fill, states[i].fill);
        QCOMPARE(out[i].tilt, states[i].tilt);
        QCOMPARE(out[i].tick, states[i].tick);
        QCOMPARE(out[i].margin, states[i].margin);
    }
    QCOMPARE(out[1].gear, (int8_t)-1);
    QCOMPARE(out[2].input.pitch, 1.0f);
    QCOMPARE(out[2].input.lift, -1.0f);
    QCOMPARE(out[2].input.trim, 0.5f);

    // more than a packet holds: the first MAX_STATES go
    std::vector<VehicleState> many;
    for (int i = 0; i < VehicleStatePacket::MAX_STATES + 3; ++i) {
        many.push_back(stateAt(1000ull * i, glm::vec3((float)i), i));
    }
    QVERIFY(VehicleStatePacket::unpack(VehicleStatePacket::pack(many), out));
    QCOMPARE(out.size(), (size_t)VehicleStatePacket::MAX_STATES);
    QCOMPARE(out.back().input.sequence, (uint32_t)(VehicleStatePacket::MAX_STATES - 1));

    QVERIFY(VehicleStatePacket::unpack(VehicleStatePacket::pack({}), out));
    QVERIFY(out.empty());
}

void StreamTests::requestPacketRoundTrip() {
    std::vector<QUuid> ids { QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid() };
    std::vector<QUuid> out;
    QVERIFY(VehicleStateRequestPacket::unpack(VehicleStateRequestPacket::pack(ids), out));
    QCOMPARE(out.size(), (size_t)3);
    QCOMPARE(out[0], ids[0]);
    QCOMPARE(out[2], ids[2]);
    QVERIFY(VehicleStateRequestPacket::unpack(VehicleStateRequestPacket::pack({}), out));
    QVERIFY(out.empty());
}

void StreamTests::malformedPackets() {
    std::vector<VehicleState> states;
    QVERIFY(!VehicleStatePacket::unpack(QByteArray(), states));
    QByteArray payload = VehicleStatePacket::pack({ stateAt(1, glm::vec3(0.0f)) });
    payload.chop(5);
    QVERIFY(!VehicleStatePacket::unpack(payload, states));
    QVERIFY(states.empty());
    QByteArray tooMany;
    tooMany.append((char)(VehicleStatePacket::MAX_STATES + 1));
    QVERIFY(!VehicleStatePacket::unpack(tooMany, states));

    std::vector<QUuid> ids;
    QVERIFY(!VehicleStateRequestPacket::unpack(QByteArray(), ids));
    QByteArray request = VehicleStateRequestPacket::pack({ QUuid::createUuid() });
    request.chop(1);
    QVERIFY(!VehicleStateRequestPacket::unpack(request, ids));
}

void StreamTests::interpolation() {
    SnapshotBuffer buffer;
    const uint64_t T = 1000000;   // 1 s
    const uint64_t DT = 33333;    // 30 Hz
    VehicleState a = stateAt(T, glm::vec3(0.0f, 1.0f, 0.0f), 1);
    VehicleState b = stateAt(T + DT, glm::vec3(1.0f, 1.0f, 0.0f), 2);
    b.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    a.rotation = glm::quat();
    b.engineRPM = 2000.0f;
    a.engineRPM = 1000.0f;
    b.gear = 3;
    a.gear = 2;
    b.input.sequence = 2;
    QVERIFY(buffer.push(a, T + 5000));
    QVERIFY(buffer.push(b, T + DT + 5000));
    QCOMPARE(buffer.size(), (size_t)2);

    VehicleState s;
    QVERIFY(buffer.sample(T + DT / 2, s));
    QCOMPARE_WITH_ABS_ERROR(s.position.x, 0.5f, 1.0e-4f);
    QCOMPARE(s.position.y, 1.0f);
    float angle = glm::degrees(2.0f * acosf(glm::clamp(s.rotation.w, -1.0f, 1.0f)));
    QCOMPARE_WITH_ABS_ERROR(angle, 45.0f, 0.1f);
    QCOMPARE_WITH_ABS_ERROR(s.engineRPM, 1500.0f, 0.5f);
    QCOMPARE(s.gear, (int8_t)2);                 // the discrete parts from the earlier state
    QCOMPARE(s.input.sequence, (uint32_t)1);
    QCOMPARE(s.time, T + DT / 2);
    QCOMPARE(s.velocity, a.velocity);

    // at the earlier state exactly: that state
    QVERIFY(buffer.sample(T, s));
    QCOMPARE(s.position.x, 0.0f);
    // a quarter of the way
    QVERIFY(buffer.sample(T + DT / 4, s));
    QCOMPARE_WITH_ABS_ERROR(s.position.x, 0.25f, 1.0e-3f);
}

void StreamTests::lateAndEarly() {
    SnapshotBuffer buffer;
    VehicleState s;
    const uint64_t T = 1000000;
    const uint64_t DT = 33333;
    // nothing, or one state: nothing to interpolate between
    QVERIFY(!buffer.sample(T, s));
    QVERIFY(buffer.push(stateAt(T, glm::vec3(0.0f), 1), T));
    QVERIFY(!buffer.sample(T, s));
    QVERIFY(buffer.push(stateAt(T + DT, glm::vec3(1.0f), 2), T + DT));
    // at or past the newest: late, the caller extrapolates
    QVERIFY(!buffer.sample(T + DT, s));
    QVERIFY(!buffer.sample(T + 2 * DT, s));
    // before the oldest: too early
    QVERIFY(!buffer.sample(T - 1, s));
    // between: fine
    QVERIFY(buffer.sample(T + 1, s));
    QCOMPARE(buffer.newest().input.sequence, (uint32_t)2);
}

void StreamTests::orderAndDuplicates() {
    SnapshotBuffer buffer;
    const uint64_t T = 10000000;   // 10 s, so that "five seconds older" is still a time
    const uint64_t DT = 33333;
    QVERIFY(buffer.push(stateAt(T, glm::vec3(0.0f), 1), T));
    QVERIFY(buffer.push(stateAt(T + 2 * DT, glm::vec3(2.0f), 3), T + 2 * DT));
    // out of order: slotted in; sampling between the first two then uses it
    QVERIFY(buffer.push(stateAt(T + DT, glm::vec3(1.0f), 2), T + 2 * DT + 1000));
    QCOMPARE(buffer.size(), (size_t)3);
    QCOMPARE(buffer.oldest().input.sequence, (uint32_t)1);
    QCOMPARE(buffer.newest().input.sequence, (uint32_t)3);
    VehicleState s;
    QVERIFY(buffer.sample(T + DT / 2, s));
    QCOMPARE_WITH_ABS_ERROR(s.position.x, 0.5f, 1.0e-4f);
    // a duplicate time: refused
    QVERIFY(!buffer.push(stateAt(T + DT, glm::vec3(9.0f), 2), T + 3 * DT));
    QVERIFY(!buffer.push(stateAt(T + 2 * DT, glm::vec3(9.0f), 3), T + 3 * DT));
    QCOMPARE(buffer.size(), (size_t)3);
    // far older than what is kept: refused; and old ones fall off the front as the newest advances
    QVERIFY(!buffer.push(stateAt(T - 5000000, glm::vec3(0.0f), 0), T + 3 * DT));
    QVERIFY(buffer.push(stateAt(T + 3000000, glm::vec3(3.0f), 4), T + 3000000));
    QCOMPARE(buffer.size(), (size_t)1);
    QCOMPARE(buffer.oldest().input.sequence, (uint32_t)4);
    // never more than MAX_STATES
    for (int i = 1; i <= SnapshotBuffer::MAX_STATES + 10; ++i) {
        buffer.push(stateAt(T + 3000000 + i * 1000, glm::vec3((float)i), 4 + i), T + 3000000 + i * 1000);
    }
    QCOMPARE((int)buffer.size(), SnapshotBuffer::MAX_STATES);
    buffer.clear();
    QVERIFY(buffer.empty());
}

void StreamTests::delayFromJitter() {
    // a steady 30 Hz stream: the base delay
    SnapshotBuffer steady;
    const uint64_t DT = 33333;
    for (int i = 0; i < 200; ++i) {
        steady.push(stateAt(1000000 + i * DT, glm::vec3((float)i), i), 1000000 + i * DT);
    }
    QCOMPARE_WITH_ABS_ERROR(steady.interval(), 0.0333f, 0.001f);
    QCOMPARE_WITH_ABS_ERROR(steady.jitter(), 0.0f, 1.0e-4f);
    QCOMPARE(steady.delay(), SnapshotBuffer::BASE_DELAY);

    // the same stream with every other packet 20 ms late: the delay grows by the jitter margin
    SnapshotBuffer jittery;
    for (int i = 0; i < 200; ++i) {
        uint64_t arrival = 1000000 + i * DT + (i % 2 ? 20000 : 0);
        jittery.push(stateAt(1000000 + i * DT, glm::vec3((float)i), i), arrival);
    }
    QVERIFY(jittery.jitter() > 0.015f);
    QVERIFY(jittery.delay() > SnapshotBuffer::BASE_DELAY + 0.04f);
    QVERIFY(jittery.delay() <= SnapshotBuffer::MAX_DELAY);

    // a slow stream (10 Hz): at least a few intervals behind, so that two states still bracket
    SnapshotBuffer slow;
    for (int i = 0; i < 100; ++i) {
        slow.push(stateAt(1000000 + i * 100000, glm::vec3((float)i), i), 1000000 + i * 100000);
    }
    QCOMPARE_WITH_ABS_ERROR(slow.delay(), 0.15f, 0.005f);
}
