#ifdef __cplusplus
#include "stdint.h"
using int2     = glm::ivec2;
using float2   = glm::vec2;
using float3   = glm::vec3;
using float4   = glm::vec4;
using float4x4 = glm::mat4;
using uint     = unsigned int;
#endif
#include "PConstantType.h.slang"
struct EngineConfig
{
};

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