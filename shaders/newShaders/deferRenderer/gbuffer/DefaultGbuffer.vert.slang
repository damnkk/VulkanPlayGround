#include "GBufferCommon.h.slang"

struct VertexInput
{
    uint vertexID : SV_VulkanVertexID;
    uint instanceIndex : SV_VulkanInstanceID;
    uint drawIndex : SV_DrawIndex;
};

[shader("vertex")]
GBufferVertexOutput main(VertexInput vin)
{
    CameraData camera = ((CameraData*) g_gBufferPushConstant.perFrameConstant.cameraBufferDeviceAddress)[0];
    GpuSceneData scene = loadGpuScene(g_gBufferPushConstant.sceneConstant.sceneAddress);
    uint drawID = loadGpuSceneDrawID(scene, g_gBufferPushConstant.sceneConstant.bucketID, vin.drawIndex);
    GpuSceneDrawData draw = loadGpuSceneDraw(scene, drawID);
    GpuSceneModelData model = loadGpuSceneModel(scene, draw.modelID);
    ModelDrawableInfo drawable = loadGpuSceneDrawable(model, draw.drawableID);
    ModelMeshInfo mesh = loadGpuSceneMesh(model, drawable);
    ModelVertexStreamInfo streams = loadGpuSceneVertexStreams(mesh);
    GpuSceneInstanceData instance = loadGpuSceneInstance(scene, vin.instanceIndex);
    uint vertexIndex = loadGpuSceneVertexIndex(mesh, vin.vertexID);

    float3 position = ((float3*) streams.positionAddress)[vertexIndex];
    float3 normal = ((float3*) streams.normalAddress)[vertexIndex];
    float4 tangent = ((float4*) streams.tangentAddress)[vertexIndex];

    // Slang's row-major view of the uploaded GLM matrices uses row vectors.
    float4x4 worldFromMesh = mul(drawable.modelFromMesh, instance.worldFromModel);
    float3x3 linear = (float3x3) worldFromMesh;
    float3 cofactor0 = cross(linear[1], linear[2]);
    float3 cofactor1 = cross(linear[2], linear[0]);
    float3 cofactor2 = cross(linear[0], linear[1]);
    float handedness = dot(linear[0], cofactor0) < 0.0 ? -1.0 : 1.0;

    float4 localPosition = float4(position, 1.0);

    GBufferVertexOutput vout;
    vout.position = mul(mul(localPosition, worldFromMesh), camera.viewProjMatrix);
    // Convert UV jitter to NDC, applying a constant offset after perspective divide.
    vout.position.xy += 2.0 * camera.jitterUV * vout.position.w;
    // Static geometry: the world position is identical in both frames, so only
    // the camera matrices differ. With animation this becomes the previous
    // frame's deformed world position.
    vout.prevClip = mul(mul(localPosition, worldFromMesh), camera.prevViewProjMatrix);
    // Cofactors give the inverse-transpose direction, including drawable scale.
    vout.normal = normalize((normal.x * cofactor0 + normal.y * cofactor1 + normal.z * cofactor2) * handedness);
    vout.tangent = float4(normalize(mul(tangent.xyz, linear)), tangent.w * handedness);
    vout.uv = ((float2*) streams.texCoord0Address)[vertexIndex];
    vout.uv1 = ((float2*) streams.texCoord1Address)[vertexIndex];
    vout.drawID = drawID;
    vout.customData = instance.customData;
    return vout;
}
