#pragma once
#include "common.slang"
#include "PConstantType.h.slang"
#include "constants.h.slang"
#include "tonemap_functions.h.slang"
#include "sky_functions.h.slang"
[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> g_PerFrameConstant;
struct FragmentInput
{
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
};

float3 tonemapReinhardJodie(float3 color)
{
    float  l  = dot(color, float3(0.2126, 0.7152, 0.0722));
    float3 tc = color / (color + 1.0);
    return lerp(color / (l + 1.0), tc, tc);
}

[shader("fragment")]
void main(FragmentInput fragIn, out FragmentOutput fragOut)
{
    float3      ndc         = float3(fragIn.uv * 2.0 - 1.0, 1.0);
    CameraData* cameraData  = (CameraData*) g_PerFrameConstant.cameraBufferDeviceAddress;
    float4      worldVector = mul(mul(float4(ndc, 1.0), cameraData->invProjMatrix), cameraData->invViewMatrix);
    worldVector /= worldVector.w;
    float3 dir = normalize(worldVector.xyz);
    // 3d vector to 2d uv
    float2 skyUV;
    skyUV.x           = -(0.5 + atan2(dir.z, dir.x) / (2.0 * M_PI));
    skyUV.y           = -(0.5 - asin(dir.y) / M_PI);
    fragOut.color     = float4(skyUV, 0.0, 1.0);
    fragOut.color     = s_SceneSkyTexture.Sample(g_GlobalSampler_Linear, skyUV);
    fragOut.color.xyz = toLinear(fragOut.color.xyz);
}

