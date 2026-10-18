#define BITFIELD highp int
#define LAYOUT(X) layout(X)
#define LAYOUT_STD140(X) layout(std140, X)
#ifdef VULKAN
    #define gl_InstanceID  gl_InstanceIndex
    #define gl_VertexID  gl_VertexIndex
#endif
#extension GL_EXT_texture_buffer : enable
#if defined(HAVE_EXT_clip_cull_distance) && !defined(VULKAN)
#extension GL_EXT_clip_cull_distance : enable
#endif
precision highp float;
// ints are mediump by default in fragment shaders, which Adreno (Android) phones really make 16 bits: bit packing like
// the light clusters' (LightClusterGrid.slh) breaks, and every surface gets hundreds of lights
precision highp int;
precision highp samplerBuffer;
precision highp sampler2DShadow;
precision highp sampler2DArrayShadow;
precision highp sampler2DArray;
