#ifndef GPU_SCENE_SLANG
#define GPU_SCENE_SLANG

#include "common.slang"

GpuSceneData loadGpuScene(uint64_t rootAddress)
{
    return ((GpuSceneData*) rootAddress)[0];
}

uint loadGpuSceneDrawID(GpuSceneData scene, uint bucketID, uint drawIndex)
{
    GpuSceneDrawBucket bucket = ((GpuSceneDrawBucket*) scene.bucketAddress)[bucketID];
    return ((uint*) scene.visibleDrawIDAddress)[bucket.firstCommand + drawIndex];
}

GpuSceneDrawData loadGpuSceneDraw(GpuSceneData scene, uint drawID)
{
    return ((GpuSceneDrawData*) scene.drawAddress)[drawID];
}

GpuSceneModelData loadGpuSceneModel(GpuSceneData scene, uint modelID)
{
    return ((GpuSceneModelData*) scene.modelAddress)[modelID];
}

ModelDrawableInfo loadGpuSceneDrawable(GpuSceneModelData model, uint drawableID)
{
    return ((ModelDrawableInfo*) model.drawableAddress)[drawableID];
}

ModelMeshInfo loadGpuSceneMesh(GpuSceneModelData model, ModelDrawableInfo drawable)
{
    return ((ModelMeshInfo*) model.buffers.meshInfoAddress)[drawable.meshIndex];
}

ModelVertexStreamInfo loadGpuSceneVertexStreams(ModelMeshInfo mesh)
{
    return ((ModelVertexStreamInfo*) mesh.vertexStreamAddress)[0];
}

// Non-indexed VertexIndex starts at zero (firstVertex == 0). indexAddress is
// mesh-local; adding drawable.firstIndex again would apply the offset twice.
uint loadGpuSceneVertexIndex(ModelMeshInfo mesh, uint vertexID)
{
    return ((uint*) mesh.indexAddress)[vertexID];
}

// instanceIndex is Vulkan's InstanceIndex, which already includes firstInstance.
GpuSceneInstanceData loadGpuSceneInstance(GpuSceneData scene, uint instanceIndex)
{
    return ((GpuSceneInstanceData*) scene.instanceAddress)[instanceIndex];
}

GltfShadeMaterial loadGpuSceneMaterial(GpuSceneModelData model, ModelDrawableInfo drawable)
{
    return ((GltfShadeMaterial*) model.buffers.materialAddress)[drawable.materialIndex];
}

bool loadGpuSceneTextureInfo(GpuSceneModelData model, uint textureInfoIndex, out ModelTextureInfo info)
{
    info = (ModelTextureInfo) 0;
    info.textureIndex = -1;
    // VPGLoader's material texture-info slot zero means no texture.
    if (textureInfoIndex == 0 || textureInfoIndex >= model.buffers.textureInfoCount) return false;
    info = ((ModelTextureInfo*) model.buffers.textureInfoAddress)[textureInfoIndex];
    return info.textureIndex >= 0 && uint(info.textureIndex) < model.textureCount;
}

float4 sampleGpuSceneTexture(GpuSceneModelData model, ModelTextureInfo info, float2 uv0, float2 uv1)
{
    float2 uv = (info.texCoord == 1 ? uv1 : uv0) * info.scale;
    float sine;
    float cosine;
    sincos(info.rotation, sine, cosine);
    uv = float2(cosine * uv.x - sine * uv.y, sine * uv.x + cosine * uv.y) + info.offset;
    uint textureIndex = model.buffers.textureOffset + uint(info.textureIndex);
    return s_SceneTextures[NonUniformResourceIndex(textureIndex)].Sample(g_GlobalSampler_Linear, uv);
}

#endif // GPU_SCENE_SLANG
