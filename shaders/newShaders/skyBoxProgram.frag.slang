#pragma once
#include "common.slang"
#include "PConstantType.h.slang"
#include "constants.h.slang"
#include "tonemap_functions.h.slang"
#include "sky_functions.h.slang"
[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> g_PerFrameConstant;

[vk::binding(0, 3)]
Texture2D<float4> g_TransmittanceLut;
[vk::binding(1, 3)]
ConstantBuffer<AtmosParameter> u_atmosParameter;
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

float3 getSunDirection(float2 sunSitDeg)
{
    // sunSit.x: azimuth around the Y axis, using +X as the 0 degree reference direction.
    // sunSit.y: elevation above the horizon.
    float azimuth   = radians(sunSitDeg.x);
    float elevation = -radians(sunSitDeg.y);
    float cosElev   = cos(elevation);
    return normalize(float3(cosElev * cos(azimuth), sin(elevation), cosElev * sin(azimuth)));
}

float3 GetSunDisk(in AtmosParameter param, float3 eyePos, float3 viewDir, float3 lightDir)
{
    // 璁＄畻鍏ュ皠鍏夌収
    float  cosineTheta = clamp(dot(viewDir, -lightDir), -1.0, 1.0);
    float  theta       = acos(cosineTheta) * (180.0 / M_PI);
    float3 sumLumiance = param.sunColor * param.sunLightIntensity;
    if (theta < param.sunDiskAngle)
        return sumLumiance;
    return float3(0.0);
}

float3 RayleighCoefficient(in AtmosParameter param, float h)
{
    const float3 sigma = float3(5.802, 13.558, 33.1) * 1e-6;
    float        H_R   = param.rayleighScatteringScalarHeight;
    float        rho_h = exp(-h / H_R);
    return sigma * rho_h;
}

float RayleiPhase(in AtmosParameter param, float cosTheta)
{
    return (3.0 / (16.0 * M_PI)) * (1.0 + cosTheta * cosTheta);
}

float3 MieCoefficient(in AtmosParameter param, float h)
{
    const float3 sigma = (3.996 * 1e-6).xxx;
    float        H_M   = param.mieScatteringScalarHeight;
    float        rho_h = exp(-h / H_M);
    return sigma * rho_h;
}

float MiePhase(in AtmosParameter param, float cosTheta)
{
    float g = param.mieAnisotropy;
    float a = 3.0 / (8.0 * M_PI);
    float b = (1.0 - g * g) / (2.0 + g * g);
    float c = 1.0 + cosTheta * cosTheta;
    float d = pow(1.0 + g * g - 2 * g * cosTheta, 1.5);
    return a * b * (c / d);
}

float3 scattering(in AtmosParameter param, float3 p, float3 inDir, float3 outDir)
{
    float  cosTheta = dot(inDir, outDir);
    float  h        = length(p) - param.planetRadius;
    float3 rayleigh = RayleighCoefficient(param, h) * RayleiPhase(param, cosTheta);
    float3 mie      = MieCoefficient(param, h) * MiePhase(param, cosTheta);
    return rayleigh + mie;
}

// 吸收分为米氏吸收和臭氧吸收
float3 MieAbsorption(in AtmosParameter param, float h)
{
    const float3 sigma = (4.4 * 1e-6).xxx;
    float        H_M   = param.mieScatteringScalarHeight;
    float        rho_h = exp(-(h / H_M));
    return sigma * rho_h;
}

float3 OzonAbsorption(in AtmosParameter param, float h)
{
    float center = param.OZoneLevelCenterHeight;
    float width  = param.OZoneLevelWidth;
    float rho    = max(0, 1.0 - (abs(h - center) / width));
    return (float3(0.65, 1.881, 0.085)) * 1e-6 * rho;
}

float3 Transmittance(in AtmosParameter param, float3 p1, float3 p2)
{
    const int N_SAMPLE = 32;
    float3    dir      = normalize(p2 - p1);
    float     distance = length(p2 - p1);
    float     ds       = distance / float(N_SAMPLE);
    float3    sum      = 0.0;
    float3    p        = p1 + 0.5 * ds * dir;
    for (int i = 0; i < N_SAMPLE; ++i)
    {
        float  h          = length(p) - param.planetRadius;
        float3 scattering = RayleighCoefficient(param, h) + MieCoefficient(param, h);
        float3 absorption = OzonAbsorption(param, h) + MieAbsorption(param, h);
        float3 extinction = scattering + absorption;
        sum += extinction * ds;
        p += dir * ds;
    }
    return exp(-sum);
}

[shader("fragment")]
void main(FragmentInput fragIn, out FragmentOutput fragOut)
{
    float3      ndc         = float3(fragIn.uv * 2.0 - 1.0, 1.0);
    CameraData* cameraData  = (CameraData*) g_PerFrameConstant.cameraBufferDeviceAddress;
    float4      worldVector = mul(mul(float4(ndc, 1.0), cameraData->invProjMatrix), cameraData->invViewMatrix);
    worldVector /= worldVector.w;
    float3 dir    = normalize(worldVector.xyz);
    fragOut.color = float4(0, 0, 0, 1);
#ifdef USE_SKY_BOX_TEXTURE
    float2 skyUV;
    skyUV.x           = -(0.5 + atan2(dir.z, dir.x) / (2.0 * M_PI));
    skyUV.y           = -(0.5 - asin(dir.y) / M_PI);
    fragOut.color     = float4(skyUV, 0.0, 1.0);
    fragOut.color     = s_SceneSkyTexture.Sample(g_GlobalSampler_Linear, skyUV);
    fragOut.color.xyz = toLinear(fragOut.color.xyz);
#endif
    float3 sunDirection = getSunDirection(u_atmosParameter.sunSit);
    float  h            = cameraData->cameraPosition.y - u_atmosParameter.seaLevel + u_atmosParameter.planetRadius;
    float3 eyePos       = float3(0, h, 0);
    fragOut.color.xyz += GetSunDisk(u_atmosParameter, eyePos, dir, sunDirection);
    fragOut.color.xyz = g_TransmittanceLut.Sample(g_GlobalSampler_Linear, fragIn.uv).xyz;
}
