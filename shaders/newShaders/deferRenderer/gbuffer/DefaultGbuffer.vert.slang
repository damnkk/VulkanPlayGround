// Simple pass-through vertex shader for post-process full screen quad
#include "PConstantType.h.slang"
#include "Hdevice.h"
#include "gltf_scene_io.h.slang"

struct VertexInput
{
    uint vertexId : SV_VertexID;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

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

[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_gBufferPushConstant;

[shader("vertex")]
VertexOutput main(VertexInput vin)
{
    CameraData* camData = (CameraData*) g_gBufferPushConstant.perFrameConstant.cameraBufferDeviceAddress;

    GBufferGPUInstanceData* instances =
        (GBufferGPUInstanceData*) g_gBufferPushConstant.sceneConstant.instanceBufferAddress;
    GBufferGPUInstanceData instance = instances[uint(g_gBufferPushConstant.sceneConstant.instanceIndex)];

    MeshInfo meshInfo = ((MeshInfo*) instance.meshInfoAddress)[0];
    VertexStreamInfo vertexStream = ((VertexStreamInfo*) meshInfo.vertexBufferAddress)[0];

    uint* indices = (uint*) meshInfo.IndexBufferAddress;
    uint vertexIndex = indices[vin.vertexId];

    float3* positions = (float3*) vertexStream.positionBufferAddress;
    float3* normals = (float3*) vertexStream.normalBufferAddress;
    float2* texCoords0 = (float2*) vertexStream.texCoord0BufferAddress;

    float3 position = positions[vertexIndex];
    float3 normal = normals[vertexIndex];
    float2 uv = texCoords0[vertexIndex];

    VertexOutput vout;
    vout.uv       = uv;
    vout.position = mul(mul(mul(float4(position, 1.0), instance.objectToWorld), camData->viewMatrix), camData->projMatrix);
    vout.normal   = normalize(mul(float4(normal, 0.0), instance.worldToObject).xyz);
    return vout;
}

