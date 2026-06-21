#ifndef GBUFFER_COMMON_H_SLANG
#define GBUFFER_COMMON_H_SLANG

struct MeshInfo
{
    uint64_t vertexBufferAddress;
    uint64_t IndexBufferAddress;
    uint indexCount;
    uint materialIdx;
};

struct VertexStreamInfo
{
    uint64_t positionBufferAddress;
    uint64_t normalBufferAddress;
    uint64_t tangentBufferAddress;
    uint64_t texCoord0BufferAddress;
    uint64_t texCoord1BufferAddress;
    uint64_t colorBufferAddress;
};

struct GBufferGPUInstanceData
{
    float4x4 objectToWorld;
    float4x4 worldToObject;
    uint64_t meshInfoAddress;
    uint64_t materialAddress;
    uint64_t textureInfoAddress;
    uint meshInfoIndex;
    uint materialIndex;
    uint textureInfoOffset;
    uint flags;
};

#endif // GBUFFER_COMMON_H_SLANG
