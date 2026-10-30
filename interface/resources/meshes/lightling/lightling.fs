//
//  lightling.fs: the Lightling's light, a Darkride procedural material (version 3) on the default avatar's skin.
//  iChannel0, baked by lines.py: red a drawn line's bright core, green its glow, blue how far along its line
//  (repeating every 0.4 m, each line with its own offset). iChannel1: red the line's colour along the body (0 cyan,
//  0.5 violet, 1 warm orange), green the skin's wireframe, blue 1 where a line holds steady (the mouth's).
//  - The skin is a dark ink-blue made of light: unlit (no shading, no highlights), with the hair's faint glow where
//    it turns from the camera, so its shape still reads. On it the drawn lines glow in their colours, a pulse runs
//    along each like current through a neon tube (the face's lines hold steady, so expressions stay easy to read), and
//    they hum very faintly. Far off, a line is kept about a pixel wide, so it doesn't vanish.
//  - With WIREFRAME: the skin's wireframe, seen only in a broad, soft wave of light rising slowly up the body. Its
//    lines are a texel wide, and a texel covers far more skin on the body's texture than on the face's, so they are
//    dimmed where texels are big, to look the same everywhere. The skin's back faces aren't drawn.
//  - With OUTLINES: wherever the skin turns away from the camera (the silhouette of an arm, a thigh, the head) a thin
//    line of light about a pixel and a half wide, kept that width by its own screen-space rate of change, as toon
//    shaders' rim outlines are; bands of light run up it and it shimmers.
//  Cores stay coloured rather than white-hot, so the lines keep their colour in daylight; in a world with bloom the
//  glow blooms. Made by skunkworks (2026-10-28); the default avatar from 1.1.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

const bool OUTLINES = true;
const bool WIREFRAME = false;
const vec3 CYAN = vec3(0.0, 0.62, 1.0);
const vec3 VIOLET = vec3(0.55, 0.2, 1.0);
const vec3 ORANGE = vec3(1.0, 0.45, 0.12);
const vec3 SKIN = vec3(0.018, 0.024, 0.06);
const vec3 WIRE = vec3(0.45, 0.85, 1.0);
const vec3 RIM = vec3(0.04, 0.044, 0.12);    // the hair's edge glow (its glow colour at 0.12)
const float SPEED = 0.5;          // pulses a second along each 0.4 m stretch of line
const float GLOW_M = 0.005;       // lines.py's glow falls off as exp(-distance / 5 mm)
const float OUTLINE_AT = 0.86;    // how edge-on the skin is where the outline sits (1 is the silhouette itself)
const float WAVE_EVERY = 11.0;    // seconds for the wireframe's wave to rise from below the feet to above the head
const float WAVE_WIDTH = 0.45;    // m: how broad the wave is
const float WIRE_REST = 0.0;      // how bright the wireframe is between waves
const float WIRE_WAVE = 0.3;      // and in the middle of one
const float WIRE_TEXEL = 0.0004;  // m: the texel size the wireframe's brightness is set for (the face's is about this)

// the engine reads channel pictures as colour (sRGB) and hands them over linear: back to the numbers lines.py wrote
vec3 undoSRGB(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}

vec3 lineColour(float t) {
    return t < 0.5 ? mix(CYAN, VIOLET, t * 2.0) : mix(VIOLET, ORANGE, t * 2.0 - 1.0);
}

float getProceduralFragment(inout ProceduralFragment frag) {
    vec3 lines = undoSRGB(texture(iChannel0, _texCoord0).rgb);
    vec3 more = undoSRGB(texture(iChannel1, _texCoord0).rgb);
    float core = lines.r;
    float glow = lines.g;
    float steady = more.b;

    // far off a pixel spans more skin than the thin core is wide and the core falls between pixels: widen it from the
    // glow, which says how far the line is, to about a pixel. Measured in metres on the skin, not in texels, as the
    // face's texture is many times finer than the body's
    float pixel = length(fwidth(_positionES.xyz));
    if (pixel > 0.0015) {
        float edge = exp(-0.5 * pixel / GLOW_M);
        core = max(core, smoothstep(edge * 0.85, edge * 1.2, glow) * 0.6);
    }

    float p = fract(lines.b - iGlobalTime * SPEED);
    float pulse = mix(pow(p, 14.0), 0.18, steady);                      // the head as p nears 1, a fading tail behind
    float hum = 0.94 + 0.06 * sin(iGlobalTime * 53.0 + lines.b * 31.0);  // neon's faint flicker
    vec3 light = lineColour(more.r) * (core * (0.4 + 2.4 * pulse) * hum + glow * 0.15 * (1.0 + 1.5 * pulse));

    // the wireframe: faint, with a broad soft wave rising slowly through it (the model's own height, feet at 0), and
    // the same weight of line everywhere (metres a texel covers here: a pixel's metres over a pixel's texels)
    float rise = fract(iGlobalTime / WAVE_EVERY) * 2.8 - 0.6;
    float wave = exp(-pow((_positionMS.y - rise) / WAVE_WIDTH, 2.0));
    float texelM = length(fwidth(_positionES.xyz)) / max(length(fwidth(_texCoord0)) * iChannelResolution[1].x, 1e-6);
    float even = clamp(WIRE_TEXEL / max(texelM, 1e-6), 0.6, 1.5);   // the body's texels are big: not below 0.6
    if (WIREFRAME) {
        light += WIRE * more.g * even * (WIRE_REST + WIRE_WAVE * wave);
    }

    if (OUTLINES) {
        TransformCamera cam = getTransformCamera();
        vec3 normalES = normalize((cam._view * vec4(normalize(frag.normal), 0.0)).xyz);
        float edgeOn = 1.0 - abs(dot(normalES, normalize(-_positionES.xyz)));
        float width = fwidth(edgeOn) * 1.5;
        float outline = smoothstep(OUTLINE_AT - width, OUTLINE_AT, edgeOn);
        vec3 positionWS = (cam._viewInverse * _positionES).xyz;
        float rising = pow(0.5 + 0.5 * sin(positionWS.y * 9.0 - iGlobalTime * 3.2), 6.0);      // bands running up
        float shimmer = 0.7 + 0.3 * sin(iGlobalTime * 23.0 + dot(positionWS, vec3(31.7, 17.3, 23.9))) *
                        sin(iGlobalTime * 7.1 + positionWS.y * 40.0);
        light += lineColour(0.25 + 0.2 * sin(positionWS.y * 2.0 + iGlobalTime * 0.7)) * outline * (0.45 + 1.6 * rising) * shimmer;
    }

    // made of light, not lit like a surface: unlit, as the hair is, and like the hair a faint glow as the skin turns
    // from the camera, so its shape still reads
    TransformCamera view = getTransformCamera();
    vec3 towardsES = normalize((view._view * vec4(normalize(frag.normal), 0.0)).xyz);
    float rim = pow(1.0 - abs(dot(towardsES, normalize(-_positionES.xyz))), 2.0);
    frag.diffuse = SKIN + RIM * rim + light;
    frag.unlit = true;
    return 0.0;
}
