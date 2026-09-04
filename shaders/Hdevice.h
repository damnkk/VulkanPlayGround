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

// GPU-only model data. Keep these structures POD so the host can upload them
// directly and shaders can follow their device addresses without descriptors.
struct ModelVertexStreamInfo
{
    uint64_t positionAddress;
    uint64_t normalAddress;
    uint64_t tangentAddress;
    uint64_t texCoord0Address;
    uint64_t texCoord1Address;
    uint64_t colorAddress;
};

struct ModelMeshInfo
{
    uint64_t vertexStreamAddress;
    uint64_t indexAddress;
    uint     indexCount;
    uint     materialIndex;
};

struct ModelTextureInfo
{
    float2 offset;
    float2 scale;
    float  rotation;
    int    textureIndex;
    uint   texCoord;
    uint   _padding;
};

struct ModelBufferInfo
{
    uint64_t positionAddress;
    uint64_t normalAddress;
    uint64_t tangentAddress;
    uint64_t texCoord0Address;
    uint64_t texCoord1Address;
    uint64_t colorAddress;
    uint64_t indexAddress;
    uint64_t meshInfoAddress;
    uint64_t materialAddress;
    uint64_t textureInfoAddress;
    uint     meshCount;
    uint     materialCount;
    uint     textureInfoCount;
    uint     textureOffset;
};

#ifdef __cplusplus
static_assert(sizeof(ModelVertexStreamInfo) == 48);
static_assert(sizeof(ModelMeshInfo) == 24);
static_assert(sizeof(ModelTextureInfo) == 32);
static_assert(sizeof(ModelBufferInfo) == 96);
#endif

#endif // HDEVICE_H
