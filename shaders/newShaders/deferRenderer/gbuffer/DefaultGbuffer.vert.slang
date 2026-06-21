#include "common.slang"
#include "GBufferCommon.h.slang"

struct VertexInput
{
    uint vertexId : SV_VertexID;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
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
    float4* tangents = (float4*) vertexStream.tangentBufferAddress;
    float2* texCoords0 = (float2*) vertexStream.texCoord0BufferAddress;
    float2* texCoords1 = (float2*) vertexStream.texCoord1BufferAddress;

    float3 position = positions[vertexIndex];
    float3 normal = normals[vertexIndex];
    float4 tangent = tangents[vertexIndex];
    float2 uv = texCoords0[vertexIndex];
    float2 uv1 = texCoords1[vertexIndex];

    VertexOutput vout;
    vout.uv       = uv;
    vout.uv1      = uv1;
    vout.position = mul(mul(mul(float4(position, 1.0), instance.objectToWorld), camData->viewMatrix), camData->projMatrix);
    vout.normal   = normalize(mul(float4(normal, 0.0), instance.worldToObject).xyz);
    vout.tangent  = float4(normalize(mul(tangent.xyz, (float3x3) instance.objectToWorld)), tangent.w);
    return vout;
}

