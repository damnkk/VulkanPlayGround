#include "GpuScene.h"
#include "core/Profiling.h"

namespace Play
{

namespace
{

void appendModelRenderable(ModelAsset& asset, uint32_t submeshIndex, uint32_t nodeIndex, const glm::mat4& localToModel, bool& hasBounds)
{
    if (submeshIndex >= asset.submeshes.size())
    {
        return;
    }

    const ModelSubmeshAsset& submesh = asset.submeshes[submeshIndex];
    if (submesh.meshID == INVALID_SCENE_ID)
    {
        return;
    }

    ModelRenderableTemplate renderable;
    renderable.submeshIndex = submeshIndex;
    renderable.nodeIndex    = nodeIndex;
    renderable.localToModel = localToModel;
    renderable.modelBounds  = transformAABB(submesh.bbox, localToModel);

    if (hasBounds)
    {
        expandAABB(asset.bbox, renderable.modelBounds);
    }
    else
    {
        asset.bbox = renderable.modelBounds;
        hasBounds  = true;
    }

    asset.renderables.push_back(renderable);
}

void collectModelNodeRenderables(ModelAsset& asset, uint32_t nodeIndex, const glm::mat4& parentTransform, bool& hasBounds)
{
    if (nodeIndex >= asset.nodes.size())
    {
        return;
    }

    const ModelNodeAsset& node = asset.nodes[nodeIndex];
    glm::mat4            nodeToModel = parentTransform;
    if (node.transformIdx != INVALID_SCENE_ID && node.transformIdx < asset.transforms.size())
    {
        nodeToModel = parentTransform * asset.transforms[node.transformIdx];
    }

    for (uint32_t submeshIndex : node.submeshIdx)
    {
        appendModelRenderable(asset, submeshIndex, nodeIndex, nodeToModel, hasBounds);
    }

    uint32_t childIndex = node.firstChild;
    while (childIndex != INVALID_SCENE_ID && childIndex < asset.nodes.size())
    {
        const uint32_t nextSibling = asset.nodes[childIndex].nextSibling;
        collectModelNodeRenderables(asset, childIndex, nodeToModel, hasBounds);
        childIndex = nextSibling;
    }
}

void buildModelRenderables(ModelAsset& asset)
{
    PLAY_PROFILE_SCOPE("GpuScene::buildModelRenderables");

    asset.renderables.clear();

    bool hasBounds = false;
    if (asset.rootNode != INVALID_SCENE_ID)
    {
        collectModelNodeRenderables(asset, asset.rootNode, glm::mat4(1.0f), hasBounds);
    }

    if (asset.renderables.empty())
    {
        for (uint32_t submeshIndex = 0; submeshIndex < asset.submeshes.size(); ++submeshIndex)
        {
            appendModelRenderable(asset, submeshIndex, INVALID_SCENE_ID, glm::mat4(1.0f), hasBounds);
        }
    }

    if (!hasBounds)
    {
        asset.bbox = {};
    }
}

} // namespace

void GpuScene::clear()
{
    PLAY_PROFILE_SCOPE("GpuScene::clear");

    std::lock_guard<std::mutex> lock(_registrationMutex);

    const bool rasterEnabled = _rasterData.enabled;
    const bool rtEnabled     = _rtData.enabled;

    _common                 = {};
    _rasterData             = {};
    _rtData                 = {};
    _rasterData.enabled     = rasterEnabled;
    _rtData.enabled         = rtEnabled;
    _models.clear();
    _modelGpuResources.clear();
    _modelRanges.clear();
    _sceneTextures.clear();
    _sceneTextureSources.clear();
    _sourceSceneRevision = 0;
}

ModelAssetID GpuScene::registerModel(UploadedModel&& uploadedModel)
{
    PLAY_PROFILE_SCOPE("GpuScene::registerModel");

    std::lock_guard<std::mutex> lock(_registrationMutex);

    const uint32_t materialBase    = static_cast<uint32_t>(_common.materials.size());
    const uint32_t meshInfoBase    = static_cast<uint32_t>(_common.meshInfos.size());
    const uint32_t textureBase      = static_cast<uint32_t>(_sceneTextures.size());
    uint32_t       textureCount     = 0;
    {
        PLAY_PROFILE_SCOPE("GpuScene::registerModel append textures");
        textureCount = appendSceneTextures(uploadedModel.textures);
    }
    const uint32_t textureInfoCount =
        uploadedModel.textureInfos.empty() ? 0 : static_cast<uint32_t>(uploadedModel.textureInfos.size() - 1);

    std::vector<MeshInfo> uploadedMeshInfos;
    {
        PLAY_PROFILE_SCOPE("GpuScene::registerModel remap mesh infos");
        uploadedMeshInfos.reserve(uploadedModel.meshInfos.size());
        for (MeshInfo meshInfo : uploadedModel.meshInfos)
        {
            if (meshInfo.materialIdx != INVALID_SCENE_ID)
            {
                meshInfo.materialIdx += materialBase;
            }
            uploadedMeshInfos.push_back(meshInfo);
        }
    }

    {
        PLAY_PROFILE_SCOPE("GpuScene::registerModel append materials");
        for (const shaderio::GltfShadeMaterial& material : uploadedModel.materials)
        {
            _common.materials.push_back(material);
        }

        for (const MeshInfo& meshInfo : uploadedMeshInfos)
        {
            _common.meshInfos.push_back(meshInfo);
        }
    }

    {
        PLAY_PROFILE_SCOPE("GpuScene::registerModel remap submeshes");
        for (ModelSubmeshAsset& submesh : uploadedModel.asset.submeshes)
        {
            if (submesh.meshID != INVALID_SCENE_ID)
            {
                submesh.meshID += meshInfoBase;
            }
        }
    }

    buildModelRenderables(uploadedModel.asset);

    GpuModelRange range;
    range.firstMeshInfo    = meshInfoBase;
    range.meshInfoCount    = static_cast<uint32_t>(uploadedModel.meshInfos.size());
    range.firstMaterial    = materialBase;
    range.materialCount    = static_cast<uint32_t>(uploadedModel.materials.size());
    range.textureInfoCount = textureInfoCount;
    range.firstTexture     = textureBase;
    range.textureCount     = textureCount;

    const uint32_t modelIndex = static_cast<uint32_t>(_models.size());
    _models.push_back(std::move(uploadedModel.asset));
    _modelGpuResources.push_back(std::move(uploadedModel.resources));
    _modelRanges.push_back(range);

    {
        PLAY_PROFILE_SCOPE("GpuScene::registerModel backend registration");
        registerRasterData(_models.back(), range);
        registerRayTracingData(_modelGpuResources.back(), range);
    }

    ModelAssetID id;
    id.index      = modelIndex;
    id.generation = _models[modelIndex].generation;
    return id;
}

void GpuScene::updateTransforms(const CpuScene& scene)
{
    _sourceSceneRevision = scene.getRevision();
}

uint32_t GpuScene::appendSceneTextures(std::vector<UploadedModelTexture>& textures)
{
    PLAY_PROFILE_SCOPE("GpuScene::appendSceneTextures");

    const uint32_t textureBase = static_cast<uint32_t>(_sceneTextures.size());
    for (UploadedModelTexture& texture : textures)
    {
        if (!texture.isResident())
        {
            continue;
        }

        _sceneTextureSources.push_back(std::move(texture.sourcePath));
        _sceneTextures.push_back(std::move(texture.texture));
    }
    return static_cast<uint32_t>(_sceneTextures.size()) - textureBase;
}

void GpuScene::registerRasterData(const ModelAsset& model, const GpuModelRange& range)
{
    (void) model;
    (void) range;
}

void GpuScene::registerRayTracingData(const ModelGpuResources& resources, const GpuModelRange& range)
{
    (void) range;
    if (!_rtData.enabled)
    {
        return;
    }

    _rtData.accelerationStructures.insert(_rtData.accelerationStructures.end(), resources.accelerationStructures.begin(),
                                          resources.accelerationStructures.end());
}

} // namespace Play
