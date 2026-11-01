//
//  TrackCurve.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TrackCurve.h"

#include <algorithm>
#include <cmath>

#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
    const float ALPHA = 0.5f;                  // the centripetal parametrisation: knot intervals are sqrt(chord)
    const float MIN_KNOT = 1.0e-4f;            // a duplicated point gets a tiny knot interval instead of a zero one
    const float SAMPLE_SPACING = 0.05f;        // metres between arc-length samples, about
    const int MIN_SAMPLES_PER_SEGMENT = 8;
    const int MAX_SAMPLES_PER_SEGMENT = 1024;
    const float TANGENT_DU = 1.0e-3f;
    const float DEGENERATE_LENGTH2 = 1.0e-6f;
    const glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);
    const glm::vec3 WORLD_FRONT(0.0f, 0.0f, -1.0f);
    const glm::vec3 WORLD_RIGHT(1.0f, 0.0f, 0.0f);
    // polyline() and poses() return at most this many vertices, whatever spacing is asked for: a track thousands of
    // kilometres long sampled every half metre would be millions (10-21 review)
    const int MAX_POLYLINE_VERTICES = 65536;

    // the vertices for a length at about a spacing: two at least, the count in double before it becomes an int
    int vertexCount(float length, float spacing) {
        double segments = std::ceil((double)length / (double)spacing);
        return (int)std::min(std::max(segments, 2.0), (double)(MAX_POLYLINE_VERTICES - 1)) + 1;
    }
}

void TrackCurve::clear() {
    _points.clear();
    _bank.clear();
    _closed = false;
    _length = 0.0f;
    _samples.clear();
    _pointArcLengths.clear();
}

void TrackCurve::build(const std::vector<glm::vec3>& points, const std::vector<float>& bankDegrees, bool closed) {
    clear();
    if (points.size() < MIN_POINTS) {
        return;
    }
    _points.assign(points.begin(), points.begin() + std::min(points.size(), MAX_POINTS));
    bool distinct = false;
    for (size_t i = 1; i < _points.size() && !distinct; ++i) {
        distinct = glm::distance2(_points[i], _points[0]) > DEGENERATE_LENGTH2;
    }
    if (!distinct) {
        _points.clear();
        return;
    }
    _closed = closed && _points.size() >= 3;
    _bank.assign(_points.size(), 0.0f);
    for (size_t i = 0; i < _points.size() && i < bankDegrees.size(); ++i) {
        _bank[i] = glm::radians(bankDegrees[i]);
    }

    size_t segments = segmentCount();
    _pointArcLengths.assign(_points.size(), 0.0f);
    float s = 0.0f;
    glm::vec3 previousPosition = evaluate(0, 0.0f);
    glm::vec3 previousUp = WORLD_UP;
    for (size_t segment = 0; segment < segments; ++segment) {
        float chord = glm::distance(controlPoint((int)segment), controlPoint((int)segment + 1));
        int count = glm::clamp((int)std::ceil(chord / SAMPLE_SPACING), MIN_SAMPLES_PER_SEGMENT, MAX_SAMPLES_PER_SEGMENT);
        for (int k = 0; k < count; ++k) {
            float u = (float)k / (float)count;
            glm::vec3 position = evaluate((int)segment, u);
            if (!(segment == 0 && k == 0)) {
                s += glm::distance(position, previousPosition);
            }
            if (k == 0) {
                _pointArcLengths[segment] = s;   // the segment starts at control point `segment`
            }
            glm::vec3 tangent = tangentAt((int)segment, u);
            glm::vec3 up = levelUp(tangent, previousUp);
            _samples.push_back({ s, (int)segment, u, position, tangent, up, bankAt((int)segment, u) });
            previousPosition = position;
            previousUp = up;
        }
    }
    // the end: the last control point of an open track, the first again of a closed one
    int lastSegment = (int)segments - 1;
    glm::vec3 endPosition = evaluate(lastSegment, 1.0f);
    s += glm::distance(endPosition, previousPosition);
    glm::vec3 endTangent = tangentAt(lastSegment, 1.0f);
    _samples.push_back({ s, lastSegment, 1.0f, endPosition, endTangent, levelUp(endTangent, previousUp), bankAt(lastSegment, 1.0f) });
    if (!_closed) {
        _pointArcLengths[_points.size() - 1] = s;
    }
    _length = s;
}

size_t TrackCurve::segmentCount() const {
    if (_points.size() < MIN_POINTS) {
        return 0;
    }
    return _closed ? _points.size() : _points.size() - 1;
}

float TrackCurve::arcLengthAtPoint(size_t i) const {
    if (i < _pointArcLengths.size()) {
        return _pointArcLengths[i];
    }
    return _length;
}

float TrackCurve::normalize(float s) const {
    if (_length <= 0.0f) {
        return 0.0f;
    }
    if (_closed) {
        s = std::fmod(s, _length);
        if (s < 0.0f) {
            s += _length;
        }
        return s;
    }
    return glm::clamp(s, 0.0f, _length);
}

glm::vec3 TrackCurve::controlPoint(int i) const {
    int n = (int)_points.size();
    if (_closed) {
        return _points[((i % n) + n) % n];
    }
    if (i < 0) {
        // a phantom point beyond the start, continuing the first chord
        return _points[0] + (_points[0] - _points[1]);
    }
    if (i >= n) {
        return _points[n - 1] + (_points[n - 1] - _points[n - 2]);
    }
    return _points[i];
}

void TrackCurve::segmentKnots(int segment, float knots[4]) const {
    knots[0] = 0.0f;
    for (int j = 1; j < 4; ++j) {
        float chord = glm::distance(controlPoint(segment - 2 + j), controlPoint(segment - 1 + j));
        knots[j] = knots[j - 1] + std::max(std::pow(chord, ALPHA), MIN_KNOT);
    }
}

// Barry and Goldman's recursive evaluation of the Catmull-Rom segment from control point `segment` to the next
glm::vec3 TrackCurve::evaluate(int segment, float u) const {
    glm::vec3 p0 = controlPoint(segment - 1);
    glm::vec3 p1 = controlPoint(segment);
    glm::vec3 p2 = controlPoint(segment + 1);
    glm::vec3 p3 = controlPoint(segment + 2);
    float t[4];
    segmentKnots(segment, t);
    float x = t[1] + glm::clamp(u, 0.0f, 1.0f) * (t[2] - t[1]);

    glm::vec3 a1 = (t[1] - x) / (t[1] - t[0]) * p0 + (x - t[0]) / (t[1] - t[0]) * p1;
    glm::vec3 a2 = (t[2] - x) / (t[2] - t[1]) * p1 + (x - t[1]) / (t[2] - t[1]) * p2;
    glm::vec3 a3 = (t[3] - x) / (t[3] - t[2]) * p2 + (x - t[2]) / (t[3] - t[2]) * p3;
    glm::vec3 b1 = (t[2] - x) / (t[2] - t[0]) * a1 + (x - t[0]) / (t[2] - t[0]) * a2;
    glm::vec3 b2 = (t[3] - x) / (t[3] - t[1]) * a2 + (x - t[1]) / (t[3] - t[1]) * a3;
    return (t[2] - x) / (t[2] - t[1]) * b1 + (x - t[1]) / (t[2] - t[1]) * b2;
}

glm::vec3 TrackCurve::tangentAt(int segment, float u) const {
    float u0 = std::max(0.0f, u - TANGENT_DU);
    float u1 = std::min(1.0f, u + TANGENT_DU);
    glm::vec3 delta = evaluate(segment, u1) - evaluate(segment, u0);
    if (glm::length2(delta) < 1.0e-16f) {
        return WORLD_FRONT;
    }
    return glm::normalize(delta);
}

float TrackCurve::bankAt(int segment, float u) const {
    size_t n = _points.size();
    size_t next = ((size_t)segment + 1) % n;
    return glm::mix(_bank[segment], _bank[next], glm::clamp(u, 0.0f, 1.0f));
}

// The level up vector: the world's up made perpendicular to the tangent. Where the track runs vertical that has no
// answer, and the previous sample's up is carried along instead, so a loop stays continuous.
glm::vec3 TrackCurve::levelUp(const glm::vec3& tangent, const glm::vec3& previousUp) {
    glm::vec3 up = WORLD_UP - glm::dot(WORLD_UP, tangent) * tangent;
    if (glm::length2(up) < 1.0e-4f) {
        up = previousUp - glm::dot(previousUp, tangent) * tangent;
        if (glm::length2(up) < 1.0e-6f) {
            up = WORLD_RIGHT - glm::dot(WORLD_RIGHT, tangent) * tangent;
        }
    }
    return glm::normalize(up);
}

// The rotation whose -Z is the tangent and whose +Y is the up vector
glm::quat TrackCurve::rotationFrom(const glm::vec3& tangent, const glm::vec3& up) {
    glm::vec3 z = -tangent;
    glm::vec3 x = glm::cross(up, z);
    if (glm::length2(x) < 1.0e-8f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    x = glm::normalize(x);
    glm::vec3 y = glm::cross(z, x);
    return glm::normalize(glm::quat_cast(glm::mat3(x, y, z)));
}

TrackCurve::Pose TrackCurve::poseAt(float s) const {
    Pose pose;
    if (!isValid()) {
        return pose;
    }
    s = normalize(s);
    // the last sample at or before s
    auto after = std::upper_bound(_samples.begin(), _samples.end(), s,
                                  [](float value, const Sample& sample) { return value < sample.s; });
    size_t index = (size_t)std::max<ptrdiff_t>(0, (after - _samples.begin()) - 1);
    index = std::min(index, _samples.size() - 2);
    const Sample& a = _samples[index];
    const Sample& b = _samples[index + 1];
    float span = b.s - a.s;
    float f = span > 1.0e-9f ? glm::clamp((s - a.s) / span, 0.0f, 1.0f) : 0.0f;
    // b may be the first sample of the next segment: that is u = 1 of a's segment
    float bu = (b.segment == a.segment) ? b.u : 1.0f;
    float u = glm::mix(a.u, bu, f);

    pose.position = evaluate(a.segment, u);
    pose.tangent = tangentAt(a.segment, u);
    glm::vec3 carried = glm::normalize(glm::mix(a.up, b.up, f));
    glm::vec3 up = levelUp(pose.tangent, carried);
    float bank = glm::mix(a.bank, b.bank, f);
    if (std::fabs(bank) > 1.0e-6f) {
        up = glm::normalize(glm::angleAxis(bank, pose.tangent) * up);
    }
    pose.up = up;
    pose.rotation = rotationFrom(pose.tangent, up);
    return pose;
}

float TrackCurve::nearest(const glm::vec3& point) const {
    if (!isValid()) {
        return 0.0f;
    }
    size_t best = 0;
    float bestDistance2 = glm::distance2(point, _samples[0].position);
    for (size_t i = 1; i < _samples.size(); ++i) {
        float d2 = glm::distance2(point, _samples[i].position);
        if (d2 < bestDistance2) {
            bestDistance2 = d2;
            best = i;
        }
    }
    // refine between the neighbouring samples
    float low = _samples[best > 0 ? best - 1 : 0].s;
    float high = _samples[std::min(best + 1, _samples.size() - 1)].s;
    for (int iteration = 0; iteration < 24 && high - low > 1.0e-4f; ++iteration) {
        float third = (high - low) / 3.0f;
        float s1 = low + third;
        float s2 = high - third;
        if (glm::distance2(point, positionAt(s1)) < glm::distance2(point, positionAt(s2))) {
            high = s2;
        } else {
            low = s1;
        }
    }
    return normalize(0.5f * (low + high));
}

std::vector<glm::vec3> TrackCurve::polyline(float spacing) const {
    std::vector<glm::vec3> line;
    if (!isValid()) {
        return line;
    }
    spacing = std::max(spacing, 0.01f);
    int count = vertexCount(_length, spacing);
    line.reserve(count);
    for (int i = 0; i < count; ++i) {
        line.push_back(positionAt(_length * (float)i / (float)(count - 1)));
    }
    return line;
}

std::vector<TrackCurve::Pose> TrackCurve::poses(float spacing) const {
    std::vector<Pose> line;
    if (!isValid()) {
        return line;
    }
    spacing = std::max(spacing, 0.01f);
    int count = vertexCount(_length, spacing);
    line.reserve(count);
    for (int i = 0; i < count; ++i) {
        line.push_back(poseAt(_length * (float)i / (float)(count - 1)));
    }
    return line;
}
