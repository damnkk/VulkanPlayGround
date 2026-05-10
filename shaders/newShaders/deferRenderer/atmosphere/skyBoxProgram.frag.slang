#include "common.slang"
#include "PConstantType.h.slang"
#include "constants.h.slang"
#include "tonemap_functions.h.slang"
#include "sky_functions.h.slang"
#include "raymarching.slang"
[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> g_PerFrameConstant;

[vk::binding(0, 3)]
Texture2D<float4> g_SkyViewLut;

[vk::binding(1, 3)]
ConstantBuffer<AtmosphereParameters> u_atmosParameter;
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
    float3      clipSpace  = float3(fragIn.uv * float2(2.0, 2.0) - float2(1.0, 1.0), 1.0);
    CameraData* cameraData = (CameraData*) g_PerFrameConstant.cameraBufferDeviceAddress;
    float4      HViewPos   = mul(float4(clipSpace, 1.0), cameraData->invProjMatrix);
    float3      WorldDir   = normalize(mul(HViewPos.xyz / HViewPos.w, (float3x3) cameraData->invViewMatrix));
    float3      WorldPos   = cameraData->cameraPosition + float3(0, u_atmosParameter.BottomRadius, 0);

    float  viewHeight = length(WorldPos);
    float3 L          = 0;
    if (viewHeight < u_atmosParameter.TopRadius) {
        float2 skyviewLutUV;
        float3 UpVector           = normalize(WorldPos);
        float  viewZenithCosAngle = dot(WorldDir, UpVector);
        float3 sideVector         = normalize(cross(UpVector, WorldDir));
        float3 forwardVector      = normalize(cross(sideVector, UpVector));
        float3 sun_direction      = normalize(GetSunDirection(u_atmosParameter.sun_dir.x, u_atmosParameter.sun_dir.y));
        float2 lightOnPlane       = normalize(float2(dot(sun_direction, forwardVector), dot(sun_direction, sideVector)));
        float  lightVieCosAngle   = lightOnPlane.x;
        bool   intersectGround    = raySphereIntersectNearest(WorldPos, WorldDir, float3(0, 0, 0), u_atmosParameter.BottomRadius) >= 0.0f;
        SkyViewLutParamsToUv(u_atmosParameter, intersectGround, viewZenithCosAngle, lightVieCosAngle, viewHeight, skyviewLutUV);
        fragOut.color = float4(g_SkyViewLut.Sample(g_GlobalSampler_Linear_Clamp, skyviewLutUV).rgb + GetSunLuminance(WorldPos, WorldDir, sun_direction, u_atmosParameter.BottomRadius), 1.0);
        // fragOut.color.xyz = sun_direction;
        // fragOut.color.xyz = 100.0 * fragOut.color.xyz;
        // fragOut.color = g_SkyViewLut.Sample(g_GlobalSampler_Linear_Clamp, fragIn.uv)*100;
        return;
    }
    fragOut.color = float4(0.0, 0.0, 0.0, 1.0);
}
