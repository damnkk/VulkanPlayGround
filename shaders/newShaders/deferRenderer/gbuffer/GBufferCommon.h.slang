#ifndef GBUFFER_COMMON_H_SLANG
#define GBUFFER_COMMON_H_SLANG

struct GBufferGPUInstanceData
{
    float4x4 objectToWorld;
    float4x4 worldToObject;
    uint64_t meshInfoAddress;
    uint64_t materialAddress;
    uint64_t textureInfoAddress;
    uint meshInfoIndex;
    uint materialIndex;
    uint textureOffset;
    uint textureInfoCount;
    uint textureCount;
    uint flags;
};

#endif // GBUFFER_COMMON_H_SLANG
