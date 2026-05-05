#include "common.slang"
#include "PConstantType.h.slang"
#include "constants.h.slang"
#include "raymarching.slang"

[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> g_PerFrameConstant;

[vk::binding(0, 3)]
RWTexture2D<float4> g_SkyViewLutOutput;

[vk::binding(1, 3)]
Texture2D<float4> g_TransmittanceLut;

[vk::binding(2, 3)]
Texture2D<float4> g_MultiScatteringLut;

[vk::binding(3, 3)]
ConstantBuffer<AtmosphereParameters> u_atmosParameter;

float3 IntegrateScatteredLuminanceSkyViewLutL(in float3 WorldPos, in float3 WorldDir, in float3 SunDir)
{
    float3 earthO  = float3(0.0f, 0.0f, 0.0f);
    float  tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, u_atmosParameter.BottomRadius);
    float  tTop    = raySphereIntersectNearest(WorldPos, WorldDir, earthO, u_atmosParameter.TopRadius);
    float  tMax    = 0.0f;

    if (tBottom < 0.0f)
    {
        if (tTop < 0.0f)
        {
            return 0.0f;
        }

        tMax = tTop;
    }
    else
    {
        if (tTop > 0.0f)
        {
            tMax = min(tTop, tBottom);
        }
    }

    float SampleCount      = lerp(4.0, 25.0, saturate(tMax * 0.01));
    float SampleCountFloor = floor(SampleCount);
    float tMaxFloor        = tMax * SampleCountFloor / SampleCount;

    const float uniformPhase       = 1.0 / (4.0 * M_PI);
    float       cosTheta           = dot(SunDir, WorldDir);
    float       MiePhaseValue      = hgPhase(u_atmosParameter.MiePhaseG, -cosTheta);
    float       RayleighPhaseValue = RayleighPhase(cosTheta);

    float3 globalL = 1.0f;

    float3 L          = 0.0f;
    float3 throughput = 1.0f;

    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        float t0 = s / SampleCountFloor;
        float t1 = (s + 1.0f) / SampleCountFloor;

        t0 = t0 * t0;
        t1 = t1 * t1;

        t0 = tMaxFloor * t0;
        if (t1 > 1.0f)
        {
            t1 = tMax;
        }
        else
        {
            t1 = tMaxFloor * t1;
        }

        float  t  = t0 + (t1 - t0) * SampleSegmentT;
        float  dt = t1 - t0;
        float3 P  = WorldPos + t * WorldDir;

        MediumSampleRGB medium              = sampleMediumRGB(P, u_atmosParameter);
        float3          SampleOpticalDepth  = medium.extinction * dt;
        float3          SampleTransmittance = exp(-SampleOpticalDepth);

        float  pHeight           = length(P);
        float3 UpVector          = P / pHeight;
        float  SunZenithCosAngle = dot(SunDir, UpVector);
        float2 uv;
        LutTransmittanceParamsToUv(u_atmosParameter, pHeight, SunZenithCosAngle, uv);
        float3 TransmittanceToSun = g_TransmittanceLut.SampleLevel(g_GlobalSampler_Linear_Clamp, uv, 0).rgb;

        float3 PhaseTimesScattering =
			medium.scatteringMie * MiePhaseValue +
			medium.scatteringRay * RayleighPhaseValue;

        float tEarth      = raySphereIntersectNearest(
			P,
			SunDir,
			earthO + 0.01 * UpVector,
			u_atmosParameter.BottomRadius);
        float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        float3 multiScatteredLuminance = 0.0f;
        // #if MULTISCATAPPROX_ENABLED
        //         multiScatteredLuminance = GetMultipleScattering(
        // 			u_atmosParameter,
        // 			medium.scattering,
        // 			medium.extinction,
        // 			P,
        // 			SunZenithCosAngle);
        // #endif

        float shadow = 1.0f;

        float3 S = globalL * (
			earthShadow * shadow * TransmittanceToSun * PhaseTimesScattering +
			multiScatteredLuminance * medium.scattering);

        float3 Sint = (S - S * SampleTransmittance) / medium.extinction;
        L += throughput * Sint;
        throughput *= SampleTransmittance;
    }

    return L;
}

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    uint2 dimensions;
    g_SkyViewLutOutput.GetDimensions(dimensions.x, dimensions.y);
    if (dispatchThreadId.x >= dimensions.x || dispatchThreadId.y >= dimensions.y)
        return;

    float2      uv             = (float2(dispatchThreadId.xy) + 0.5f) / float2(dimensions.x, dimensions.y);
    float3      ClipSpace      = float3((dispatchThreadId.xy / float2(192.0, 108.0)) * float2(2.0, -2.0) - float2(1.0, -1.0), 1.0);
    CameraData* cameraData     = (CameraData*) g_PerFrameConstant.cameraBufferDeviceAddress;
    float4      HViewPos       = mul(cameraData->invProjMatrix, float4(ClipSpace, 1.0));
    float3      WorldDir       = normalize(mul((float3x3) cameraData->invViewMatrix, HViewPos.xyz / HViewPos.w));
    float3      WorldPos       = cameraData->cameraPosition + float3(0, 0, u_atmosParameter.BottomRadius);

    float  viewHeight      = length(WorldPos);

    float  viewZenithCosAngle;
    float  lightViewCosAngle; 
    UvToSkyViewLutParams(u_atmosParameter, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

    float3 sunDir = normalize(GetSunDirection(u_atmosParameter.sun_dir.x, u_atmosParameter.sun_dir.y));

    WorldPos                  = float3(0.0, 0.0, viewHeight);
    float viewZenithSinAngle  = sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle);
    WorldDir = float3(viewZenithSinAngle * lightViewCosAngle,
                    viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle),
                    viewZenithCosAngle);

    if (!MoveToTopAtmosphere(WorldPos, WorldDir, u_atmosParameter.TopRadius))
    {
        // Ray is not intersecting the atmosphere
        g_SkyViewLutOutput[dispatchThreadId.xy] = float4(0, 0, 0, 1);
        return;
    }

    float3 L = IntegrateScatteredLuminanceSkyViewLutL(WorldPos, WorldDir, sunDir);

    g_SkyViewLutOutput[dispatchThreadId.xy] = float4(L, 1.0f);
}
