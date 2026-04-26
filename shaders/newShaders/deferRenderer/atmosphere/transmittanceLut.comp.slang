#include "common.slang"
#include "PConstantType.h.slang"
#include "constants.h.slang"
#include "raymarching.slang"

[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> g_PerFrameConstant;

[vk::binding(0, 3)]
RWTexture2D<float4> g_TransmittanceLutOutput;

[vk::binding(1, 3)]
ConstantBuffer<AtmosphereParameters> u_atmosParameter;

float3 IntegrateOpticalDepthOnly(in float3 WorldPos, in float3 WorldDir, in AtmosphereParameters Atmosphere, in float SampleCount)
{
    float3 earthO  = float3(0.0f, 0.0f, 0.0f);
    float  tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.BottomRadius);
    float  tTop    = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.TopRadius);
    float  tMax    = 0.0f;
    if (tBottom < 0.0f)
    {
        if (tTop < 0.0f)
        {
            return 0.0f;
        }
        else
        {
            tMax = tTop;
        }
    }
    else
    {
        if (tTop > 0.0f)
        {
            tMax = min(tTop, tBottom);
        }
    }

    tMax = min(tMax, 9000000.0f);

    float       dt             = tMax / SampleCount;
    float3      OpticalDepth   = 0.0f;
    float       t              = 0.0f;
    float       tPrev          = 0.0;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        float NewT = tMax * (s + SampleSegmentT) / SampleCount;
        dt         = NewT - t;
        t          = NewT;

        // OpticalDepth = float3(t);
        // return OpticalDepth;
        float3 P = WorldPos + t * WorldDir;
        OpticalDepth += SampleMediumExtinction(P, Atmosphere) * dt;

        tPrev = t;
    }

    return OpticalDepth;
}

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    uint2 dimensions;
    g_TransmittanceLutOutput.GetDimensions(dimensions.x, dimensions.y);
    if (dispatchThreadId.x >= dimensions.x || dispatchThreadId.y >= dimensions.y)
        return;

    // Span the LUT parameter domain across the full [0, 1] range, including both edges.
    float2 resolution = float2(dimensions.x, dimensions.y);
    float2 uv         = float2(dispatchThreadId.x + 0.5, dispatchThreadId.y + 0.5) / resolution;
    float  viewHeight;
    float  viewZenithCosAngle;
    UvToLutTransmittanceParams(u_atmosParameter, viewHeight, viewZenithCosAngle, uv);
    float3      WorldPos                          = float3(0.0f, 0.0f, viewHeight);
    float3      WorldDir                          = float3(0.0f, sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle), viewZenithCosAngle);
    const float SampleCount                       = 40.0f; // Can go a low as 10 sample but energy lost starts to be visible.
    float3      transmittance                     = exp(-IntegrateOpticalDepthOnly(WorldPos, WorldDir, u_atmosParameter, SampleCount));
    g_TransmittanceLutOutput[dispatchThreadId.xy] = float4(transmittance, 1.0);
}
