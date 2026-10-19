#define GPU_SSBO_TRANSFORM_OBJECT
#define GPU_CLIP_DEPTH_ZERO_TO_ONE
#define BITFIELD int
#define LAYOUT(X) layout(X)
#define LAYOUT_STD140(X) layout(std140, X)
#ifdef VULKAN
#define gl_InstanceID  gl_InstanceIndex
#define gl_VertexID  gl_VertexIndex
#endif
