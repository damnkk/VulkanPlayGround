#ifndef GBUFFER_COMMON_H_SLANG
#define GBUFFER_COMMON_H_SLANG

#include "GpuScene.slang"

struct GBufferVertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    nointerpolation uint drawID : TEXCOORD2;
    nointerpolation float4 customData : TEXCOORD3;
};

[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_gBufferPushConstant;

#endif // GBUFFER_COMMON_H_SLANG
