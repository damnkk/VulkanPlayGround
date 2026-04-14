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

[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_gBufferPushConstant;

[shader("vertex")]
VertexOutput main(VertexInput vin)
{
    CameraData*         camData         = (CameraData*) g_gBufferPushConstant.perFrameConstant.cameraBufferDeviceAddress;
    GltfScene*          scene           = (GltfScene*) g_gBufferPushConstant.sceneConstant.sceneDescAddress;
    GltfRenderNode      renderNode      = scene->renderNodes[g_gBufferPushConstant.sceneConstant.renderNodeId];
    float4x4            modelMatrix     = renderNode.objectToWorld;
    GltfRenderPrimitive renderPrimitive = scene->renderPrimitives[renderNode.renderPrimID];
    // uint                triangleIdx     = vin.vertexId / 3;
    // uint                pointIdx        = vin.vertexId % 3;
    // uint                realIndex       = renderPrimitive.indices[triangleIdx][pointIdx];
    float3       position = renderPrimitive.vertexBuffer.positions[vin.vertexId];
    float3       normal   = renderPrimitive.vertexBuffer.normals[vin.vertexId];
    float2       uv       = renderPrimitive.vertexBuffer.texCoords0[vin.vertexId];
    VertexOutput vout;
    vout.uv       = uv;
    vout.position = mul(mul(mul(float4(position, 1.0), modelMatrix), camData->viewMatrix), camData->projMatrix);
    vout.normal   = mul(float4(normal, 0.0), transpose(renderNode.objectToWorld)).xyz;
    return vout;
}

