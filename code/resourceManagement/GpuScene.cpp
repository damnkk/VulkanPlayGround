#include "GpuScene.h"

namespace Play
{

namespace
{

uint16_t remapTextureInfoIndex(uint16_t localIndex, const std::vector<uint16_t>& textureInfoRemap)
{
    if (localIndex == 0)
    {
        return 0;
    }
    if (localIndex >= textureInfoRemap.size())
    {
        return 0;
    }
    return textureInfoRemap[localIndex];
}

void remapMaterialTextureInfos(shaderio::GltfShadeMaterial& material, const std::vector<uint16_t>& textureInfoRemap)
{
    material.pbrBaseColorTexture             = remapTextureInfoIndex(material.pbrBaseColorTexture, textureInfoRemap);
    material.normalTexture                   = remapTextureInfoIndex(material.normalTexture, textureInfoRemap);
    material.pbrMetallicRoughnessTexture     = remapTextureInfoIndex(material.pbrMetallicRoughnessTexture, textureInfoRemap);
    material.emissiveTexture                 = remapTextureInfoIndex(material.emissiveTexture, textureInfoRemap);
    material.transmissionTexture             = remapTextureInfoIndex(material.transmissionTexture, textureInfoRemap);
    material.thicknessTexture                = remapTextureInfoIndex(material.thicknessTexture, textureInfoRemap);
    material.clearcoatTexture                = remapTextureInfoIndex(material.clearcoatTexture, textureInfoRemap);
    material.clearcoatRoughnessTexture       = remapTextureInfoIndex(material.clearcoatRoughnessTexture, textureInfoRemap);
    material.clearcoatNormalTexture          = remapTextureInfoIndex(material.clearcoatNormalTexture, textureInfoRemap);
    material.specularTexture                 = remapTextureInfoIndex(material.specularTexture, textureInfoRemap);
    material.specularColorTexture            = remapTextureInfoIndex(material.specularColorTexture, textureInfoRemap);
    material.iridescenceTexture              = remapTextureInfoIndex(material.iridescenceTexture, textureInfoRemap);
    material.iridescenceThicknessTexture     = remapTextureInfoIndex(material.iridescenceThicknessTexture, textureInfoRemap);
    material.anisotropyTexture               = remapTextureInfoIndex(material.anisotropyTexture, textureInfoRemap);
    material.sheenColorTexture               = remapTextureInfoIndex(material.sheenColorTexture, textureInfoRemap);
    material.sheenRoughnessTexture           = remapTextureInfoIndex(material.sheenRoughnessTexture, textureInfoRemap);
    material.occlusionTexture                = remapTextureInfoIndex(material.occlusionTexture, textureInfoRemap);
    material.pbrDiffuseTexture               = remapTextureInfoIndex(material.pbrDiffuseTexture, textureInfoRemap);
    material.pbrSpecularGlossinessTexture    = remapTextureInfoIndex(material.pbrSpecularGlossinessTexture, textureInfoRemap);
    material.diffuseTransmissionTexture      = remapTextureInfoIndex(material.diffuseTransmissionTexture, textureInfoRemap);
    material.diffuseTransmissionColorTexture = remapTextureInfoIndex(material.diffuseTransmissionColorTexture, textureInfoRemap);
}

} // namespace

void GpuScene::clear()
{
    const bool rasterEnabled = _rasterData.enabled;
    const bool rtEnabled     = _rtData.enabled;

    _common                 = {};
    _rasterData             = {};
    _rtData                 = {};
    _rasterData.enabled     = rasterEnabled;
    _rtData.enabled         = rtEnabled;
    _models.clear();
    _modelRanges.clear();
    _sceneTextures.clear();
    _sceneTextureSources.clear();
    _ownedBuffers.clear();
    _common.textureInfos.push_back(shaderio::defaultGltfTextureInfo());
    _sourceSceneRevision = 0;
}

ModelAssetID GpuScene::registerModel(ModelAssetPackage&& package)
{
    const uint32_t textureInfoBase = static_cast<uint32_t>(_common.textureInfos.size());
    const uint32_t materialBase    = static_cast<uint32_t>(_common.materials.size());
    const uint32_t meshInfoBase    = static_cast<uint32_t>(_common.meshInfos.size());

    std::vector<uint32_t> textureRemap;
    textureRemap.resize(package.textures.size(), INVALID_SCENE_ID);
    for (uint32_t textureIndex = 0; textureIndex < package.textures.size(); ++textureIndex)
    {
        textureRemap[textureIndex] = ensureSceneTexture(std::move(package.textures[textureIndex]));
    }

    std::vector<uint16_t> textureInfoRemap;
    textureInfoRemap.resize(package.textureInfos.size(), 0);
    for (uint32_t textureInfoIndex = 1; textureInfoIndex < package.textureInfos.size(); ++textureInfoIndex)
    {
        shaderio::GltfTextureInfo textureInfo = package.textureInfos[textureInfoIndex];
        if (textureInfo.index >= 0 && static_cast<uint32_t>(textureInfo.index) < textureRemap.size())
        {
            const uint32_t sceneTextureIndex = textureRemap[textureInfo.index];
            textureInfo.index = sceneTextureIndex == INVALID_SCENE_ID ? -1 : static_cast<int>(sceneTextureIndex);
        }
        else
        {
            textureInfo.index = -1;
        }

        const uint16_t globalTextureInfoIndex = static_cast<uint16_t>(_common.textureInfos.size());
        textureInfoRemap[textureInfoIndex]   = globalTextureInfoIndex;
        _common.textureInfos.push_back(textureInfo);
    }

    for (shaderio::GltfShadeMaterial material : package.materials)
    {
        remapMaterialTextureInfos(material, textureInfoRemap);
        _common.materials.push_back(material);
    }

    for (MeshInfo meshInfo : package.meshInfos)
    {
        if (meshInfo.materialIdx != INVALID_SCENE_ID)
        {
            meshInfo.materialIdx += materialBase;
        }
        _common.meshInfos.push_back(meshInfo);
    }

    for (ModelSubmeshAsset& submesh : package.asset.submeshes)
    {
        if (submesh.meshID != INVALID_SCENE_ID)
        {
            submesh.meshID += meshInfoBase;
        }
    }

    GpuModelRange range;
    range.firstMeshInfo    = meshInfoBase;
    range.meshInfoCount    = static_cast<uint32_t>(package.meshInfos.size());
    range.firstMaterial    = materialBase;
    range.materialCount    = static_cast<uint32_t>(package.materials.size());
    range.firstTextureInfo = textureInfoBase;
    range.textureInfoCount = static_cast<uint32_t>(_common.textureInfos.size()) - textureInfoBase;

    const uint32_t modelIndex = static_cast<uint32_t>(_models.size());
    _ownedBuffers.insert(_ownedBuffers.end(), package.ownedBuffers.begin(), package.ownedBuffers.end());
    _models.push_back(std::move(package.asset));
    _modelRanges.push_back(range);

    registerRasterData(_models.back(), range);
    registerRayTracingData(_models.back(), range);

    ModelAssetID id;
    id.index      = modelIndex;
    id.generation = _models[modelIndex].generation;
    return id;
}

void GpuScene::updateTransforms(const CpuScene& scene)
{
    _sourceSceneRevision = scene.getRevision();
}

uint32_t GpuScene::ensureSceneTexture(ModelTextureResource&& texture)
{
    if (!texture.isResident())
    {
        return INVALID_SCENE_ID;
    }

    for (uint32_t sceneTextureIndex = 0; sceneTextureIndex < _sceneTextures.size(); ++sceneTextureIndex)
    {
        if (!texture.sourcePath.empty() && _sceneTextureSources[sceneTextureIndex] == texture.sourcePath)
        {
            return sceneTextureIndex;
        }
        if (_sceneTextures[sceneTextureIndex].get() == texture.texture.get())
        {
            return sceneTextureIndex;
        }
    }

    const uint32_t sceneTextureIndex = static_cast<uint32_t>(_sceneTextures.size());
    _sceneTextureSources.push_back(texture.sourcePath);
    _sceneTextures.push_back(texture.texture);
    return sceneTextureIndex;
}

void GpuScene::registerRasterData(const ModelAsset& model, const GpuModelRange& range)
{
    (void) model;
    (void) range;
}

void GpuScene::registerRayTracingData(const ModelAsset& model, const GpuModelRange& range)
{
    (void) range;
    if (!_rtData.enabled)
    {
        return;
    }

    _rtData.accelerationStructures.insert(_rtData.accelerationStructures.end(), model.accelerationStructures.begin(), model.accelerationStructures.end());
}

} // namespace Play
