#ifndef COMMON_SLANG
#define COMMON_SLANG
#include "Hdevice.h"
#include "gltf_scene_io.h.slang"
#include "tonemap_io.h.slang" 

// shader extension
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension VK_EXT_uniform_buffer_standard_layout : enable
// resource descriptor declarations common to multiple shaders
// Global bindings (with lowest update frequency)
[vk::binding(0, 0)]
Texture2D<float4> g_GlobalNoiseTexture;
[vk::binding(1, 0)]
Texture2D<float4> g_GlobalLUTTexture;
[vk::binding(2, 0)]
SamplerState g_GlobalSampler_Point; // nearest / point sampling
[vk::binding(3, 0)]
SamplerState g_GlobalSampler_Linear; // linear filtering
[vk::binding(4, 0)]
ConstantBuffer<TonemapperData> g_TonemapperDataBuffer;
// Per-Scene bindings(update per view)
[vk::binding(0, 1)]
Texture2D<float4> s_SceneSkyTexture;
[vk::binding(1, 1)]
TextureCube<float4> s_SceneSkyBoxTexture;
[vk::binding(2, 1)]
Texture3D<float> s_SceneVolumeFogTexture;
[vk::binding(3, 1)]
Texture2D<float4> s_SceneTextures[];

#endif // COMMON_SLANG