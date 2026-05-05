#include "common.slang"
#include "PConstantType.h.slang"
#include "constants.h.slang"
#include "raymarching.slang"
#define PLANET_RADIUS_OFFSET 0.01f

[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> g_PerFrameConstant;

[vk::binding(0, 3)]
RWTexture2D<float4> g_MultiScatteringLutOutput;

[vk::binding(1, 3)]
Texture2D<float4> g_TransmittanceLut;

[vk::binding(2, 3)]
ConstantBuffer<AtmosphereParameters> u_atmosParameter;

struct MultiScatteringLuminanceResult
{
    float3 L;
    float3 MultiScatAs1;
};

MultiScatteringLuminanceResult IntegrateMultiScatteringLuminance(in float3 WorldPos, in float3 WorldDir, in float3 SunDir,
                                                                 in AtmosphereParameters Atmosphere, in float SampleCount)
{
    MultiScatteringLuminanceResult result = (MultiScatteringLuminanceResult) 0;

    float3 earthO  = float3(0.0f, 0.0f, 0.0f);
    float  tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.BottomRadius);
    float  tTop    = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.TopRadius);
    float  tMax    = 0.0f;
    if (tBottom < 0.0f)
    {
        if (tTop < 0.0f)
        {
            return result;
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

    const float uniformPhase = 1.0 / (4.0 * M_PI);

    float3 globalL = 1.0f;

    float3      L              = 0.0f;
    float3      throughput     = 1.0;
    float       t              = 0.0f;
    float       dt             = tMax / SampleCount;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        float NewT = tMax * (s + SampleSegmentT) / SampleCount;
        dt         = NewT - t;
        t          = NewT;

        float3       P                   = WorldPos + t * WorldDir;
        float3       extinction          = SampleMediumExtinction(P, Atmosphere);
        const float3 SampleOpticalDepth  = extinction * dt;
        const float3 SampleTransmittance = exp(-SampleOpticalDepth);

        float        pHeight           = length(P);
        const float3 UpVector          = P / pHeight;
        float        SunZenithCosAngle = dot(SunDir, UpVector);
        float2       uv;
        LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
        float3 TransmittanceToSun = g_TransmittanceLut.SampleLevel(g_GlobalSampler_Linear_Clamp, uv, 0).rgb;

        float tEarth      = raySphereIntersectNearest(P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius);
        float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const float3 scattering = SampleMediumScattering(P, Atmosphere);
        float3       S          = globalL * earthShadow * TransmittanceToSun * scattering * uniformPhase;
        float3       MS         = scattering * 1;
        float3       MSint      = (MS - MS * SampleTransmittance) / extinction;
        result.MultiScatAs1 += throughput * MSint;
        float3 Sint = (S - S * SampleTransmittance) / extinction;
        L += throughput * Sint;
        throughput *= SampleTransmittance;
    }

    if (tMax == tBottom && tBottom > 0.0)
    {
        float3 P       = WorldPos + tBottom * WorldDir;
        float  pHeight = length(P);

        const float3 UpVector          = P / pHeight;
        float        SunZenithCosAngle = dot(SunDir, UpVector);
        float2       uv;
        LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
        float3 TransmittanceToSun = g_TransmittanceLut.SampleLevel(g_GlobalSampler_Linear_Clamp, uv, 0).rgb;

        const float NdotL = saturate(dot(normalize(UpVector), normalize(SunDir)));
        L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / M_PI;
    }

    result.L = L;
    return result;
}

groupshared float3 MultiScatAs1SharedMem[64];
groupshared float3 LSharedMem[64];

[numthreads(1, 1, 64)]
[shader("compute")]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    uint2 dimensions;
    g_MultiScatteringLutOutput.GetDimensions(dimensions.x, dimensions.y);
    if (dispatchThreadId.x >= dimensions.x || dispatchThreadId.y >= dimensions.y)
        return;
    float2 pixPos            = float2(dispatchThreadId.xy) + 0.5f;
    float2 uv                = pixPos / float2(dimensions.x, dimensions.y);
    uv                       = float2(fromSubUvsToUnit(uv.x, dimensions.x), fromSubUvsToUnit(uv.y, dimensions.y));
    float  cosSunZenithAngle = uv.x * 2.0f - 1.0f;
    float3 sunDir            = float3(0.0f, sqrt(saturate(1.0f - cosSunZenithAngle * cosSunZenithAngle)), cosSunZenithAngle);
    float  viewHeight = u_atmosParameter.BottomRadius +
                        saturate(uv.y + PLANET_RADIUS_OFFSET) * (u_atmosParameter.TopRadius - u_atmosParameter.BottomRadius - PLANET_RADIUS_OFFSET);

    float3      worldPos            = float3(0.0f, 0.0f, viewHeight);
    float3      worldDir            = float3(0.0f, 0.0f, 1.0f);
    const float SampleCountIni      = 20.0; // a minimum set of step is required for accuracy unfortunately

    const float SphereSolidAngle = 4.0 * M_PI;
    const float IsotropicPhase   = 1.0 / SphereSolidAngle;
    const uint  SQRTSAMPLECOUNT  = 8;

    const float sqrtSample  = float(SQRTSAMPLECOUNT);
    uint        sampleIndex = dispatchThreadId.z;
    float       i           = 0.5f + float(sampleIndex / SQRTSAMPLECOUNT);
    float       j           = 0.5f + float(sampleIndex % SQRTSAMPLECOUNT);
    {
        float randA                               = i / sqrtSample;
        float randB                               = j / sqrtSample;
        float theta                               = 2.0f * M_PI * randA;
        float phi                                 = acos(1.0f - 2.0f * randB);
        float cosPhi                              = cos(phi);
        float sinPhi                              = sin(phi);
        float cosTheta                            = cos(theta);
        float sinTheta                            = sin(theta);
        worldDir.x                                = cosTheta * sinPhi;
        worldDir.y                                = sinTheta * sinPhi;
        worldDir.z                                = cosPhi;
        MultiScatteringLuminanceResult result     = IntegrateMultiScatteringLuminance(worldPos, worldDir, sunDir, u_atmosParameter, SampleCountIni);
        MultiScatAs1SharedMem[dispatchThreadId.z] = result.MultiScatAs1 * SphereSolidAngle / (sqrtSample * sqrtSample);
        LSharedMem[dispatchThreadId.z]            = result.L * SphereSolidAngle / (sqrtSample * sqrtSample);
    }
    GroupMemoryBarrierWithGroupSync();
    if (dispatchThreadId.z < 32)
    {
        MultiScatAs1SharedMem[dispatchThreadId.z] += MultiScatAs1SharedMem[dispatchThreadId.z + 32];
        LSharedMem[dispatchThreadId.z] += LSharedMem[dispatchThreadId.z + 32];
    }
    GroupMemoryBarrierWithGroupSync();

    if (dispatchThreadId.z < 16)
    {
        MultiScatAs1SharedMem[dispatchThreadId.z] += MultiScatAs1SharedMem[dispatchThreadId.z + 16];
        LSharedMem[dispatchThreadId.z] += LSharedMem[dispatchThreadId.z + 16];
    }

    GroupMemoryBarrierWithGroupSync();

    // 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
    if (dispatchThreadId.z < 8)
    {
        MultiScatAs1SharedMem[dispatchThreadId.z] += MultiScatAs1SharedMem[dispatchThreadId.z + 8];
        LSharedMem[dispatchThreadId.z] += LSharedMem[dispatchThreadId.z + 8];
    }
    GroupMemoryBarrierWithGroupSync();
    if (dispatchThreadId.z < 4)
    {
        MultiScatAs1SharedMem[dispatchThreadId.z] += MultiScatAs1SharedMem[dispatchThreadId.z + 4];
        LSharedMem[dispatchThreadId.z] += LSharedMem[dispatchThreadId.z + 4];
    }
    GroupMemoryBarrierWithGroupSync();
    if (dispatchThreadId.z < 2)
    {
        MultiScatAs1SharedMem[dispatchThreadId.z] += MultiScatAs1SharedMem[dispatchThreadId.z + 2];
        LSharedMem[dispatchThreadId.z] += LSharedMem[dispatchThreadId.z + 2];
    }
    GroupMemoryBarrierWithGroupSync();
    if (dispatchThreadId.z < 1)
    {
        MultiScatAs1SharedMem[dispatchThreadId.z] += MultiScatAs1SharedMem[dispatchThreadId.z + 1];
        LSharedMem[dispatchThreadId.z] += LSharedMem[dispatchThreadId.z + 1];
    }
    GroupMemoryBarrierWithGroupSync();
    if (dispatchThreadId.z > 0)
        return;

    float3 MultiScatAs1         = MultiScatAs1SharedMem[0] *IsotropicPhase;
    float3 InScatteredLuminance = LSharedMem[0] * IsotropicPhase;
    // For a serie, sum_{n=0}^{n=+inf} = 1 + r + r^2 + r^3 + ... + r^n = 1 / (1.0 - r), see https://en.wikipedia.org/wiki/Geometric_series
    const float3 r                                         = MultiScatAs1;
    const float3 SumOfAllMultiScatteringEventsContribution = 1.0f / (1.0 - r);
    float3       L                                         = InScatteredLuminance * SumOfAllMultiScatteringEventsContribution; // Equation 10 Psi_ms

    g_MultiScatteringLutOutput[dispatchThreadId.xy] = float4(L, 1.0);
}
