//
//  TrackGuideway.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TrackGuideway.h"

#include <algorithm>
#include <cmath>

#include "TrackCurve.h"

namespace {
    const int MAX_RINGS = 4096;                 // a long track gets its rings further apart rather than more of them
    const int MAX_PYLONS = 512;                 // and its pylons: whatever the length and spacing, the mesh stays small
    const float MIN_PYLON_SPACING = 1.0f;       // m: a spacing typed as 0.001 is not a million pylons (10-21 review)
    const float MIN_SIZE = 0.01f;               // m: no beam thinner than this
    const float PYLON_WIDTH_FRACTION = 0.6f;    // a pylon's side, as a fraction of the beam's width
    const float PYLON_OVERLAP = 0.05f;          // m the pylon reaches up into the beam, so no seam shows between them
    const float MIN_PYLON = 0.05f;              // m: a beam this close to the floor gets no pylon
    const glm::vec3 LOCAL_X(1.0f, 0.0f, 0.0f);
    const glm::vec3 LOCAL_Y(0.0f, 1.0f, 0.0f);
    const glm::vec3 LOCAL_Z(0.0f, 0.0f, 1.0f);

    struct Builder {
        TrackGuidewayMesh& mesh;

        uint32_t vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv, const glm::vec3& tangent) {
            mesh.vertices.push_back({ position, normal, uv, tangent });
            return (uint32_t)mesh.vertices.size() - 1;
        }

        void triangle(uint32_t a, uint32_t b, uint32_t c) {
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
        }

        // a quad with its corners in order round its edge, wound so that it faces along normal
        void quadFacing(uint32_t a, uint32_t b, uint32_t c, uint32_t d, const glm::vec3& normal) {
            const glm::vec3& pa = mesh.vertices[a].position;
            glm::vec3 faceNormal = glm::cross(mesh.vertices[b].position - pa, mesh.vertices[c].position - pa);
            if (glm::dot(faceNormal, normal) >= 0.0f) {
                triangle(a, b, c);
                triangle(a, c, d);
            } else {
                triangle(a, c, b);
                triangle(a, d, c);
            }
        }
    };

    // One side of the beam, as the pair of offsets (across, up) from the ride line of its two edges and its outward
    // normal, chosen so that cross(tangent, b - a) points outward: then the strip between consecutive rings winds
    // (a_i, a_i+1, b_i), (b_i, a_i+1, b_i+1) and faces out.
    struct BeamSide {
        glm::vec2 a;
        glm::vec2 b;
        glm::vec2 normal;   // (across, up)
        float span;         // the side's width, for the v coordinate
    };

    glm::vec3 inPose(const TrackCurve::Pose& pose, const glm::vec2& acrossUp) {
        glm::vec3 right = pose.rotation * LOCAL_X;
        return pose.position + right * acrossUp.x + pose.up * acrossUp.y;
    }

    glm::vec3 poseVector(const TrackCurve::Pose& pose, const glm::vec2& acrossUp) {
        glm::vec3 right = pose.rotation * LOCAL_X;
        return glm::normalize(right * acrossUp.x + pose.up * acrossUp.y);
    }

    void buildBeam(Builder& builder, const TrackCurve& curve, const TrackGuidewaySpec& spec) {
        const float w = std::max(spec.width, MIN_SIZE);
        const float h = std::max(spec.height, MIN_SIZE);
        const float c = spec.clearance;
        const float top = -c;
        const float bottom = -c - h;
        const BeamSide sides[4] = {
            { { +w * 0.5f, top }, { -w * 0.5f, top }, { 0.0f, 1.0f }, w },         // top
            { { +w * 0.5f, bottom }, { +w * 0.5f, top }, { 1.0f, 0.0f }, h },      // right
            { { -w * 0.5f, bottom }, { +w * 0.5f, bottom }, { 0.0f, -1.0f }, w },  // bottom
            { { -w * 0.5f, top }, { -w * 0.5f, bottom }, { -1.0f, 0.0f }, h }      // left
        };

        float length = curve.length();
        float spacing = std::max(std::max(spec.ringSpacing, MIN_SIZE), length / (float)MAX_RINGS);
        std::vector<TrackCurve::Pose> poses = curve.poses(spacing);
        if (poses.size() < 2) {
            return;
        }
        const size_t rings = poses.size();
        // the vertices: four sides, two per ring each, ring-major so a side's strip is a constant stride
        std::vector<uint32_t> first(rings);
        for (size_t i = 0; i < rings; ++i) {
            const TrackCurve::Pose& pose = poses[i];
            float u = length * (float)i / (float)(rings - 1);
            first[i] = (uint32_t)builder.mesh.vertices.size();
            for (const BeamSide& side : sides) {
                glm::vec3 normal = poseVector(pose, side.normal);
                builder.vertex(inPose(pose, side.a), normal, glm::vec2(u, 0.0f), pose.tangent);
                builder.vertex(inPose(pose, side.b), normal, glm::vec2(u, side.span), pose.tangent);
            }
        }
        for (size_t i = 0; i + 1 < rings; ++i) {
            for (int s = 0; s < 4; ++s) {
                uint32_t a0 = first[i] + 2 * s, b0 = a0 + 1;
                uint32_t a1 = first[i + 1] + 2 * s, b1 = a1 + 1;
                builder.triangle(a0, a1, b0);
                builder.triangle(b0, a1, b1);
            }
        }
        if (!curve.isClosed()) {
            // the ends of an open beam: a cap each, facing back along the track at the start and forward at the end
            const glm::vec2 corners[4] = { { -w * 0.5f, top }, { +w * 0.5f, top }, { +w * 0.5f, bottom }, { -w * 0.5f, bottom } };
            const glm::vec2 uvs[4] = { { 0.0f, 0.0f }, { w, 0.0f }, { w, h }, { 0.0f, h } };
            for (int end = 0; end < 2; ++end) {
                const TrackCurve::Pose& pose = end == 0 ? poses.front() : poses.back();
                glm::vec3 normal = end == 0 ? -pose.tangent : pose.tangent;
                glm::vec3 tangent = pose.rotation * LOCAL_X;
                uint32_t ids[4];
                for (int k = 0; k < 4; ++k) {
                    ids[k] = builder.vertex(inPose(pose, corners[k]), normal, uvs[k], tangent);
                }
                builder.quadFacing(ids[0], ids[1], ids[2], ids[3], normal);
            }
        }
    }

    // A pylon: a square post from the beam's bottom straight down the track's own -Y, whatever the bank, to the floor
    // level, with no top (it is inside the beam).
    void buildPylon(Builder& builder, const glm::vec3& beamBottom, float side, float footY) {
        const float half = side * 0.5f;
        const float topY = beamBottom.y + PYLON_OVERLAP;
        const float depth = beamBottom.y - footY;
        const glm::vec3& o = beamBottom;
        struct Face {
            glm::vec3 normal;
            glm::vec3 along;   // the face's horizontal direction, its u and tangent
        };
        const Face faces[4] = {
            { LOCAL_X, LOCAL_Z }, { -LOCAL_X, -LOCAL_Z }, { LOCAL_Z, -LOCAL_X }, { -LOCAL_Z, LOCAL_X }
        };
        for (const Face& face : faces) {
            glm::vec3 centre = glm::vec3(o.x, 0.0f, o.z) + face.normal * half;
            glm::vec3 left = centre - face.along * half;
            glm::vec3 right = centre + face.along * half;
            uint32_t a = builder.vertex(glm::vec3(left.x, topY, left.z), face.normal, glm::vec2(0.0f, 0.0f), face.along);
            uint32_t b = builder.vertex(glm::vec3(right.x, topY, right.z), face.normal, glm::vec2(side, 0.0f), face.along);
            uint32_t c = builder.vertex(glm::vec3(right.x, footY, right.z), face.normal, glm::vec2(side, depth + PYLON_OVERLAP), face.along);
            uint32_t d = builder.vertex(glm::vec3(left.x, footY, left.z), face.normal, glm::vec2(0.0f, depth + PYLON_OVERLAP), face.along);
            builder.quadFacing(a, b, c, d, face.normal);
        }
        // the foot
        uint32_t a = builder.vertex(glm::vec3(o.x - half, footY, o.z - half), -LOCAL_Y, glm::vec2(0.0f, 0.0f), LOCAL_X);
        uint32_t b = builder.vertex(glm::vec3(o.x + half, footY, o.z - half), -LOCAL_Y, glm::vec2(side, 0.0f), LOCAL_X);
        uint32_t c = builder.vertex(glm::vec3(o.x + half, footY, o.z + half), -LOCAL_Y, glm::vec2(side, side), LOCAL_X);
        uint32_t d = builder.vertex(glm::vec3(o.x - half, footY, o.z + half), -LOCAL_Y, glm::vec2(0.0f, side), LOCAL_X);
        builder.quadFacing(a, b, c, d, -LOCAL_Y);
    }

    void buildPylons(Builder& builder, const TrackCurve& curve, const TrackGuidewaySpec& spec) {
        if (spec.pylonSpacing <= 0.0f) {
            return;
        }
        float length = curve.length();
        // the count in double and clamped before it becomes an int: a long track at a small spacing must not overflow
        double wanted = std::round((double)length / (double)std::max(spec.pylonSpacing, MIN_PYLON_SPACING));
        int count = (int)std::min(std::max(wanted, 1.0), (double)MAX_PYLONS);
        float spacing = length / (float)count;
        float side = std::max(std::max(spec.width, MIN_SIZE) * PYLON_WIDTH_FRACTION, MIN_SIZE);
        // a loop has a pylon every spacing round; a shuttle track one at each end too, set in by half a pylon so it
        // stands under the beam's end rather than past it
        int last = curve.isClosed() ? count - 1 : count;
        for (int k = 0; k <= last; ++k) {
            float s = spacing * (float)k;
            if (!curve.isClosed()) {
                s = glm::clamp(s, std::min(0.5f * side, 0.5f * length), std::max(length - 0.5f * side, 0.5f * length));
            }
            TrackCurve::Pose pose = curve.poseAt(s);
            glm::vec3 beamBottom = inPose(pose, glm::vec2(0.0f, -spec.clearance - std::max(spec.height, MIN_SIZE)));
            if (beamBottom.y - spec.pylonFloor < MIN_PYLON) {
                continue;
            }
            buildPylon(builder, beamBottom, side, spec.pylonFloor);
        }
    }
}

TrackGuidewaySpec::Profile TrackGuidewaySpec::profileFromName(const std::string& name) {
    if (name == "none") {
        return Profile::None;
    }
    return Profile::Beam;
}

const char* TrackGuidewaySpec::profileName(Profile profile) {
    switch (profile) {
        case Profile::None: return "none";
        case Profile::Beam: return "beam";
    }
    return "beam";
}

void TrackGuidewayMesh::clear() {
    vertices.clear();
    indices.clear();
    boundMin = boundMax = glm::vec3(0.0f);
}

void TrackGuideway::build(const TrackCurve& curve, const TrackGuidewaySpec& spec, TrackGuidewayMesh& mesh) {
    mesh.clear();
    if (!curve.isValid() || spec.profile == TrackGuidewaySpec::Profile::None) {
        return;
    }
    Builder builder { mesh };
    buildBeam(builder, curve, spec);
    buildPylons(builder, curve, spec);
    if (mesh.vertices.empty()) {
        mesh.clear();
        return;
    }
    mesh.boundMin = mesh.boundMax = mesh.vertices.front().position;
    for (const TrackGuidewayMesh::Vertex& v : mesh.vertices) {
        mesh.boundMin = glm::min(mesh.boundMin, v.position);
        mesh.boundMax = glm::max(mesh.boundMax, v.position);
    }
}
