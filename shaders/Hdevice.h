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

// Immutable drawable metadata; bounds are in model space.
struct ModelDrawableInfo
{
    float4x4 modelFromMesh;
    float4   boundsMin;
    float4   boundsMax;
    uint     meshIndex;
    uint     materialIndex;
    uint     firstIndex; // Offset into ModelBufferInfo.indexAddress; already folded into ModelMeshInfo.indexAddress.
    uint     indexCount;
};

// whole model data, including geometry data and texture data, flatting format.
struct GpuSceneModelData
{
    ModelBufferInfo buffers;
    uint64_t        drawableAddress;
    uint            drawableCount;
    uint            textureCount;
};

struct GpuSceneInstanceData
{
    float4x4 worldFromModel;
    float4x4 modelFromWorld;
    float4   customData;
};

struct GpuSceneDrawData
{
    uint modelID;
    uint drawableID;
    uint instanceBase;
    uint instanceCount;
};

struct GpuSceneDrawCandidate
{
    uint drawID;
    uint bucketID;
};

struct GpuSceneDrawBucket
{
    uint firstCommand; // Element offset, shared by indirect commands and visibleDrawIDs.
    uint capacity;
    uint countIndex; // Element offset in the uint count array.
    uint _padding;
};

// Byte layout matches VkDrawIndirectCommand (16 bytes). Shaders pull indices and vertices.
struct GpuSceneIndirectDrawCommand
{
    uint vertexCount; // Drawable indexCount: one vertex invocation per index entry.
    uint instanceCount;
    uint firstVertex; // Zero: ModelMeshInfo.indexAddress already points to this mesh's indices.
    uint firstInstance;
};

// One root per frame slot. Geometry and texture-info addresses remain asset-owned.
struct GpuSceneData
{
    uint64_t modelAddress;
    uint64_t drawAddress;
    uint64_t instanceAddress;
    uint64_t candidateAddress;
    uint64_t bucketAddress;
    uint64_t indirectAddress;
    uint64_t countAddress;
    uint64_t visibleDrawIDAddress;
    uint     modelCount;
    uint     drawCount; // Addressable slots, including free slots; iterate candidates to render.
    uint     instanceCount;
    uint     bucketCount;
    uint     candidateCount;
    uint     _padding0;
    uint     _padding1;
    uint     _padding2;
};

#ifdef __cplusplus
static_assert(sizeof(ModelVertexStreamInfo) == 48);
static_assert(sizeof(ModelMeshInfo) == 24);
static_assert(sizeof(ModelTextureInfo) == 32);
static_assert(sizeof(ModelBufferInfo) == 96);
static_assert(sizeof(ModelDrawableInfo) == 112);
static_assert(sizeof(GpuSceneModelData) == 112);
static_assert(sizeof(GpuSceneInstanceData) == 144);
static_assert(sizeof(GpuSceneDrawData) == 16);
static_assert(sizeof(GpuSceneDrawCandidate) == 8);
static_assert(sizeof(GpuSceneDrawBucket) == 16);
static_assert(sizeof(GpuSceneIndirectDrawCommand) == 16);
static_assert(sizeof(GpuSceneData) == 96);
#endif

#endif // HDEVICE_H
