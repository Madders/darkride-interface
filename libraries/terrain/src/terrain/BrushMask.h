//
//  BrushMask.h
//  libraries/terrain/src/terrain
//
//  A brush's shape as a greyscale picture (a brush mask, as Unity's terrain tools and the paint programs have
//  them): laid over the brush's square, 2 radii a side, seen from above, white where the brush has its full
//  effect and black where none. The sculpt brushes (Brush.h) take their horizontal falloff from it in place of
//  the round one and the paint brush (PaintMap.h) its whole falloff; Dig and Fill, which cut a sphere, do not use
//  it. The picture can be turned about the brush's centre. Its hardness pushes the greys toward white (harder)
//  or black (softer), the picture as it is at half.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_BrushMask_h
#define hifi_terrain_BrushMask_h

#include <vector>

#include <glm/glm.hpp>

namespace terrain {

class BrushMask {
public:
    BrushMask() = default;
    // width * height values, 0 to 1, row by row from the picture's top; a picture with no values is no mask
    BrushMask(int width, int height, std::vector<float> values);

    bool isEmpty() const { return _values.empty(); }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    const std::vector<float>& getValues() const { return _values; }

    // The picture's value at a point of the brush's square, u across and v down the picture, -1 to 1 edge to edge,
    // read between the pixels; 0 beyond the square.
    float sample(float u, float v) const;
    // The value under a point of the brush, offset from its centre along x and z in radii, with the picture turned
    // by the angle (radians) about the centre and its greys pushed by the hardness (0 to 1, half for the picture
    // as it is). The picture's top is toward -z when unturned.
    float weightAt(const glm::vec2& offset, float angle, float hardness) const;
    // How far from the centre, in radii, the turned picture reaches along x and z: the corners of its square.
    static float reach(float angle);

    // The outline of the picture's shape: the closed curves along which its value crosses the level, read on a
    // sample of the picture `samples` a side (the picture's own pixels at most), in the brush's square (u, v as
    // sample() has them), 0 beyond the picture so every curve closes. For a brush's outline on the ground.
    std::vector<std::vector<glm::vec2>> outline(float level, int samples) const;

private:
    int _width { 0 };
    int _height { 0 };
    std::vector<float> _values;
};

} // namespace terrain

#endif // hifi_terrain_BrushMask_h
