//
//  lightling-hair.fs: the Lightling's hair, a Darkride procedural material (version 3). iChannel0 is the hair's own
//  picture, for its alpha only (cut-out strand tips, where a hairstyle has them). Made of light rather than lit like a
//  surface: unlit, its colour all its own, a deep violet, brighter toward any strand's cut-out edge and toward the
//  hair's silhouette. With OUTLINES, the same glowing outline as the skin's. No wireframe: hair cards are dense, and
//  their wireframe reads as a net. Made by skunkworks (2026-10-28); the default avatar from 1.1.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

const bool OUTLINES = true;
const vec3 CYAN = vec3(0.0, 0.62, 1.0);
const vec3 VIOLET = vec3(0.55, 0.2, 1.0);
const vec3 DEEP = vec3(0.022, 0.016, 0.06);
const float OUTLINE_AT = 0.86;

float getProceduralFragment(inout ProceduralFragment frag) {
    float alpha = texture(iChannel0, _texCoord0).a;
    if (alpha < 0.5) {
        discard;
    }
    TransformCamera cam = getTransformCamera();
    vec3 normalES = normalize((cam._view * vec4(normalize(frag.normal), 0.0)).xyz);
    float facing = abs(dot(normalES, normalize(-_positionES.xyz)));
    float rim = pow(1.0 - facing, 2.0);                       // brighter as the hair turns from the camera
    float strandEdge = 1.0 - smoothstep(0.5, 0.95, alpha);    // brighter toward a strand's cut-out edge
    vec3 glow = mix(CYAN, VIOLET, 0.6);
    vec3 colour = DEEP + glow * (0.12 * rim + 0.3 * strandEdge);

    if (OUTLINES) {
        float width = fwidth(1.0 - facing) * 1.5;
        float outline = smoothstep(OUTLINE_AT - width, OUTLINE_AT, 1.0 - facing);
        vec3 positionWS = (cam._viewInverse * _positionES).xyz;
        float rising = pow(0.5 + 0.5 * sin(positionWS.y * 9.0 - iGlobalTime * 3.2), 6.0);
        float shimmer = 0.7 + 0.3 * sin(iGlobalTime * 23.0 + dot(positionWS, vec3(31.7, 17.3, 23.9))) *
                        sin(iGlobalTime * 7.1 + positionWS.y * 40.0);
        colour += mix(CYAN, VIOLET, 0.5 + 0.3 * sin(positionWS.y * 2.0 + iGlobalTime * 0.7)) * outline * (0.4 + 1.4 * rising) * shimmer;
    }

    frag.diffuse = colour;      // unlit: this is the colour seen, no shading, no highlights
    frag.unlit = true;
    frag.alpha = 1.0;
    return 0.0;
}
