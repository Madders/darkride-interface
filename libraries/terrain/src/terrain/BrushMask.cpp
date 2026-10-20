//
//  BrushMask.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "BrushMask.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace terrain;

namespace {

// Hardness raises the picture's greys to a power: half leaves them, 1 makes a mid grey nearly white, 0 nearly black.
const float HARDNESS_RANGE = 4.0f;

} // namespace

BrushMask::BrushMask(int width, int height, std::vector<float> values) {
    if (width <= 0 || height <= 0 || values.size() != (size_t)width * (size_t)height) {
        return;
    }
    _width = width;
    _height = height;
    _values = std::move(values);
    for (float& value : _values) {
        value = glm::clamp(value, 0.0f, 1.0f);
    }
}

float BrushMask::sample(float u, float v) const {
    if (isEmpty() || u <= -1.0f || u >= 1.0f || v <= -1.0f || v >= 1.0f) {
        return 0.0f;
    }
    // the pixels' centres lie at (i + 0.5) / width across the square; between them the value is interpolated, and
    // the edge pixels' values hold to the square's edge
    const float px = (u + 1.0f) * 0.5f * (float)_width - 0.5f;
    const float py = (v + 1.0f) * 0.5f * (float)_height - 0.5f;
    const int x0 = (int)std::floor(px);
    const int y0 = (int)std::floor(py);
    const float fx = px - (float)x0;
    const float fy = py - (float)y0;
    auto at = [this](int x, int y) {
        x = glm::clamp(x, 0, _width - 1);
        y = glm::clamp(y, 0, _height - 1);
        return _values[(size_t)y * _width + x];
    };
    return glm::mix(glm::mix(at(x0, y0), at(x0 + 1, y0), fx), glm::mix(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx), fy);
}

float BrushMask::weightAt(const glm::vec2& offset, float angle, float hardness) const {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float u = offset.x * c - offset.y * s;
    const float w = offset.x * s + offset.y * c;
    const float value = sample(u, -w);   // the picture's top is toward -z
    if (value <= 0.0f) {
        return 0.0f;
    }
    const float gamma = std::exp2(HARDNESS_RANGE * (0.5f - glm::clamp(hardness, 0.0f, 1.0f)));
    return gamma == 1.0f ? value : std::pow(value, gamma);
}

float BrushMask::reach(float angle) {
    return std::abs(std::cos(angle)) + std::abs(std::sin(angle));
}

std::vector<std::vector<glm::vec2>> BrushMask::outline(float level, int samples) const {
    std::vector<std::vector<glm::vec2>> curves;
    if (isEmpty() || samples < 2) {
        return curves;
    }
    // the picture read n samples a side, each the mean of its pixels, with a ring of 0 around them
    const int n = std::min(samples, std::max(_width, _height));
    const int side = n + 2;
    std::vector<float> field((size_t)side * side, 0.0f);
    for (int j = 0; j < n; ++j) {
        const int y0 = j * _height / n;
        const int y1 = std::max(y0 + 1, (j + 1) * _height / n);
        for (int i = 0; i < n; ++i) {
            const int x0 = i * _width / n;
            const int x1 = std::max(x0 + 1, (i + 1) * _width / n);
            float sum = 0.0f;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    sum += _values[(size_t)y * _width + x];
                }
            }
            field[(size_t)(j + 1) * side + (i + 1)] = sum / (float)((y1 - y0) * (x1 - x0));
        }
    }
    auto value = [&](int x, int y) { return field[(size_t)y * side + x]; };
    // Marching squares over the cells between the samples: each cell gives the pieces of curve crossing it, each
    // piece running between two of the cell's edges; an edge is named so both cells on it name it alike, and the
    // pieces are then joined edge to edge into closed curves (the ring of 0 closes every one).
    auto horizontalEdge = [side](int x, int y) { return ((int64_t)y * side + x) * 2; };
    auto verticalEdge = [side](int x, int y) { return ((int64_t)y * side + x) * 2 + 1; };
    struct Piece {
        int64_t edges[2];
    };
    std::vector<Piece> pieces;
    std::unordered_map<int64_t, glm::vec2> points;   // where the level crosses each edge, in samples
    auto crossing = [&](int64_t edge, int x, int y, bool horizontal) {
        if (points.count(edge) == 0) {
            const float a = value(x, y);
            const float b = horizontal ? value(x + 1, y) : value(x, y + 1);
            const float t = (b != a) ? glm::clamp((level - a) / (b - a), 0.0f, 1.0f) : 0.5f;
            points[edge] = horizontal ? glm::vec2((float)x + t, (float)y) : glm::vec2((float)x, (float)y + t);
        }
    };
    for (int y = 0; y < side - 1; ++y) {
        for (int x = 0; x < side - 1; ++x) {
            const bool a = value(x, y) >= level;           // top left
            const bool b = value(x + 1, y) >= level;       // top right
            const bool c = value(x + 1, y + 1) >= level;   // bottom right
            const bool d = value(x, y + 1) >= level;       // bottom left
            const int index = (a ? 1 : 0) | (b ? 2 : 0) | (c ? 4 : 0) | (d ? 8 : 0);
            if (index == 0 || index == 15) {
                continue;
            }
            const int64_t T = horizontalEdge(x, y);
            const int64_t B = horizontalEdge(x, y + 1);
            const int64_t L = verticalEdge(x, y);
            const int64_t R = verticalEdge(x + 1, y);
            auto add = [&](int64_t first, int64_t second) { pieces.push_back({ { first, second } }); };
            switch (index) {
                case 1: case 14: add(L, T); break;
                case 2: case 13: add(T, R); break;
                case 3: case 12: add(L, R); break;
                case 4: case 11: add(R, B); break;
                case 6: case 9: add(T, B); break;
                case 7: case 8: add(L, B); break;
                case 5: case 10: {
                    // two opposite corners inside: joined through the middle when it is inside too, else apart
                    const float middle = (value(x, y) + value(x + 1, y) + value(x + 1, y + 1) + value(x, y + 1)) * 0.25f;
                    const bool joined = (middle >= level) == (index == 5);
                    if (joined) {
                        add(T, R);
                        add(B, L);
                    } else {
                        add(L, T);
                        add(R, B);
                    }
                    break;
                }
            }
            crossing(T, x, y, true);
            crossing(B, x, y + 1, true);
            crossing(L, x, y, false);
            crossing(R, x + 1, y, false);
        }
    }
    std::unordered_map<int64_t, std::vector<int>> piecesAtEdge;
    for (int i = 0; i < (int)pieces.size(); ++i) {
        piecesAtEdge[pieces[i].edges[0]].push_back(i);
        piecesAtEdge[pieces[i].edges[1]].push_back(i);
    }
    auto toSquare = [n](const glm::vec2& sample) {
        // sample k is the (k - 1)th of n across the square, its centre at (k - 1 + 0.5) / n of the way
        return (sample - 0.5f) / (float)n * 2.0f - 1.0f;
    };
    std::vector<bool> used(pieces.size(), false);
    for (int start = 0; start < (int)pieces.size(); ++start) {
        if (used[start]) {
            continue;
        }
        std::vector<glm::vec2> curve;
        int current = start;
        const int64_t firstEdge = pieces[start].edges[0];
        int64_t edge = firstEdge;
        while (true) {
            used[current] = true;
            curve.push_back(toSquare(points[edge]));
            const Piece& piece = pieces[current];
            edge = piece.edges[0] == edge ? piece.edges[1] : piece.edges[0];
            if (edge == firstEdge) {
                break;   // closed
            }
            int next = -1;
            for (int candidate : piecesAtEdge[edge]) {
                if (candidate != current && !used[candidate]) {
                    next = candidate;
                    break;
                }
            }
            if (next < 0) {
                break;
            }
            current = next;
        }
        if (curve.size() >= 3) {
            curves.push_back(std::move(curve));
        }
    }
    return curves;
}
