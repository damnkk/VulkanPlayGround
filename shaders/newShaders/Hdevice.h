#ifndef HDEVICE_H
#define HDEVICE_H
#ifdef __cplusplus
#include "stdint.h"
using int2     = glm::ivec2;
using float2   = glm::vec2;
using float3   = glm::vec3;
using float4   = glm::vec4;
using float4x4 = glm::mat4;
using uint     = unsigned int;
#define DEFAULT(val) = val
#else
#define DEFAULT(val)
#endif
#include "PConstantType.h.slang"

struct CameraData
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4x4 viewProjMatrix;
    float4x4 invViewMatrix;
    float4x4 invProjMatrix;
    float4x4 invViewProjMatrix;
    float3   cameraPosition;
    float2   viewPortSize;
    float    WorldTime;
};

struct AtmosParameter
{
    float  seaLevel                        DEFAULT(0.0f);
    float  planetRadius                    DEFAULT(6360000.0f);
    float  atmosphereHeight                DEFAULT(100000.0f);
    float  sunLightIntensity               DEFAULT(10.0f);
    float3 sunColor                        DEFAULT(float3(1.0f, 1.0f, 1.0f));
    float  sunDiskAngle                    DEFAULT(0.53f);
    float  rayleighScatteringScale         DEFAULT(1.0f);
    float2 sunSit                          DEFAULT(float2(0.0f, 45.0f));
    float  rayleighScatteringScalarHeight  DEFAULT(8000.0f);
    float  mieScatteringScale              DEFAULT(1.0f);
    float  mieScatteringScalarHeight       DEFAULT(1200.0f);
    float  mieAnisotropy                   DEFAULT(0.8f);
    float  OZoneAbsorptionScale            DEFAULT(1.0f);
    float  OZoneLevelCenterHeight          DEFAULT(25000.0f);
    float  OZoneLevelWidth                 DEFAULT(15000.0f);
};
#endif // HDEVICE_H
