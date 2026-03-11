#include "common.slang"
#include "gaussian/gaussianLib.h.slang"

static const uint32_t kMaxSplatCount = 256;

[vk_binding(0, 3)]
RWStructuredBuffer<uint32_t> distances;
[vk_binding(1, 3)]
RWStructuredBuffer<IndrectBuffer> indirectBuffer;
[vk_binding(2, 3)]
RWStructuredBuffer<uint32_t> indicesBuffer;
[[vk::push_constant]]
ConstantBuffer<GaussianPushConstant> gaussianPushConstant;

uint encodeMinMaxFp32(float val)
{
    uint bits = asuint(val);
    bits ^= (int(bits) >> 31) | 0x80000000u;
    return bits;
}

#define RASTER_MESH_WORKGROUP_SIZE 32

[[numthreads(256, 1, 1)]]
[shader("compute")]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint32_t           invocationID = dispatchThreadID.x;
    CameraData*        camera       = (CameraData*) gaussianPushConstant.perFrameConstant.cameraBufferDeviceAddress;
    float3*            positions    = (float3*) gaussianPushConstant.sceneConstant.positionBufferDeviceAddress;
    GaussianSceneMeta* sceneMeta    = (GaussianSceneMeta*) gaussianPushConstant.sceneConstant.metaDataAddress;
    uint32_t           splatCount   = sceneMeta->splatCount;
    if (invocationID >= splatCount)
    {
        return;
    }
    float4 splatPos   = float4(positions[invocationID], 1.0);
    float4 viewPos    = mul(splatPos, camera->viewMatrix);
    float4 ndcPos     = mul(viewPos, camera->projMatrix);
    ndcPos            = ndcPos / ndcPos.w;
    const float depth = ndcPos.z;
    uint32_t    original;
    InterlockedAdd(indirectBuffer[0].instanceCount, 1, original);
    const uint instance_index     = original;
    distances[instance_index]     = encodeMinMaxFp32(-depth);
    indicesBuffer[instance_index] = invocationID;
    if (instance_index % RASTER_MESH_WORKGROUP_SIZE == 0)
    {
        InterlockedAdd(indirectBuffer[0].groupCountX, 1);
    }
}
