#include "GpuScene.h"
#include "PlayAllocator.h"

namespace Play
{

namespace
{

constexpr VkBufferUsageFlags2 kGpuSceneBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
constexpr VkDeviceSize kGeometrySectionAlignment = 16;

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
VkDeviceSize vectorByteSize(const std::vector<T>& values)
{
    return values.size() * sizeof(T);
}

template <typename T>
RefPtr<Buffer> createAndAppendBuffer(const std::string& name, const std::vector<T>& values, bool& hasPendingUpload)
{
    if (values.empty())
    {
        return nullptr;
    }

    RefPtr<Buffer> buffer =
        RefPtr<Buffer>(new Buffer(name, kGpuSceneBufferUsage, vectorByteSize(values), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    PlayResourceManager::Instance().appendBuffer(*buffer, 0, std::span(values.data(), values.size()));
    hasPendingUpload = true;
    return buffer;
}

template <typename T>
void placeGeometrySection(const std::vector<T>& values, VkDeviceSize& cursor, VkDeviceSize& offset, VkDeviceSize& size)
{
    if (values.empty())
    {
        offset = 0;
        size   = 0;
        return;
    }

    cursor = alignUp(cursor, kGeometrySectionAlignment);
    offset = cursor;
    size   = vectorByteSize(values);
    cursor += size;
}

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

void uploadModelGeometry(ModelAssetPackage& package, std::vector<MeshInfo>& meshInfos, bool& hasPendingUpload)
{
    ModelGeometryPayload& geometry = package.geometry;
    if (geometry.empty() || meshInfos.empty())
    {
        return;
    }

    VkDeviceSize positionsOffset  = 0;
    VkDeviceSize normalsOffset    = 0;
    VkDeviceSize tangentsOffset   = 0;
    VkDeviceSize texCoords0Offset = 0;
    VkDeviceSize texCoords1Offset = 0;
    VkDeviceSize colorsOffset     = 0;
    VkDeviceSize indicesOffset    = 0;

    VkDeviceSize positionsSize  = 0;
    VkDeviceSize normalsSize    = 0;
    VkDeviceSize tangentsSize   = 0;
    VkDeviceSize texCoords0Size = 0;
    VkDeviceSize texCoords1Size = 0;
    VkDeviceSize colorsSize     = 0;
    VkDeviceSize indicesSize    = 0;

    VkDeviceSize cursor = 0;
    placeGeometrySection(geometry.positions, cursor, positionsOffset, positionsSize);
    placeGeometrySection(geometry.normals, cursor, normalsOffset, normalsSize);
    placeGeometrySection(geometry.tangents, cursor, tangentsOffset, tangentsSize);
    placeGeometrySection(geometry.texCoords0, cursor, texCoords0Offset, texCoords0Size);
    placeGeometrySection(geometry.texCoords1, cursor, texCoords1Offset, texCoords1Size);
    placeGeometrySection(geometry.colors, cursor, colorsOffset, colorsSize);
    placeGeometrySection(geometry.indices, cursor, indicesOffset, indicesSize);

    if (cursor == 0)
    {
        return;
    }

    RefPtr<Buffer> geometryBuffer =
        RefPtr<Buffer>(new Buffer(package.asset.name + "_GeometryBuffer", kGpuSceneBufferUsage, cursor, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    PlayResourceManager& uploadManager = PlayResourceManager::Instance();
    if (positionsSize > 0) uploadManager.appendBuffer(*geometryBuffer, positionsOffset, std::span(geometry.positions.data(), geometry.positions.size()));
    if (normalsSize > 0) uploadManager.appendBuffer(*geometryBuffer, normalsOffset, std::span(geometry.normals.data(), geometry.normals.size()));
    if (tangentsSize > 0) uploadManager.appendBuffer(*geometryBuffer, tangentsOffset, std::span(geometry.tangents.data(), geometry.tangents.size()));
    if (texCoords0Size > 0)
        uploadManager.appendBuffer(*geometryBuffer, texCoords0Offset, std::span(geometry.texCoords0.data(), geometry.texCoords0.size()));
    if (texCoords1Size > 0)
        uploadManager.appendBuffer(*geometryBuffer, texCoords1Offset, std::span(geometry.texCoords1.data(), geometry.texCoords1.size()));
    if (colorsSize > 0) uploadManager.appendBuffer(*geometryBuffer, colorsOffset, std::span(geometry.colors.data(), geometry.colors.size()));
    if (indicesSize > 0) uploadManager.appendBuffer(*geometryBuffer, indicesOffset, std::span(geometry.indices.data(), geometry.indices.size()));

    std::vector<VertexStreamInfo> vertexStreams;
    vertexStreams.resize(geometry.ranges.size());
    for (uint32_t meshIndex = 0; meshIndex < geometry.ranges.size() && meshIndex < meshInfos.size(); ++meshIndex)
    {
        const ModelMeshRange& range = geometry.ranges[meshIndex];

        VertexStreamInfo stream;
        stream.positionBufferAddress  = geometryBuffer->address + positionsOffset + range.firstVertex * sizeof(glm::vec3);
        stream.normalBufferAddress    = geometryBuffer->address + normalsOffset + range.firstVertex * sizeof(glm::vec3);
        stream.tangentBufferAddress   = geometryBuffer->address + tangentsOffset + range.firstVertex * sizeof(glm::vec4);
        stream.texCoord0BufferAddress = geometryBuffer->address + texCoords0Offset + range.firstVertex * sizeof(glm::vec2);
        stream.texCoord1BufferAddress = geometryBuffer->address + texCoords1Offset + range.firstVertex * sizeof(glm::vec2);
        stream.colorBufferAddress     = geometryBuffer->address + colorsOffset + range.firstVertex * sizeof(uint32_t);
        vertexStreams[meshIndex]      = stream;

        meshInfos[meshIndex].IndexBufferAddress = geometryBuffer->address + indicesOffset + range.firstIndex * sizeof(uint32_t);
        meshInfos[meshIndex].indexCount         = range.indexCount;
    }

    RefPtr<Buffer> vertexStreamBuffer =
        createAndAppendBuffer(package.asset.name + "_VertexStreamBuffer", vertexStreams, hasPendingUpload);
    if (vertexStreamBuffer)
    {
        for (uint32_t meshIndex = 0; meshIndex < vertexStreams.size() && meshIndex < meshInfos.size(); ++meshIndex)
        {
            meshInfos[meshIndex].vertexBufferAddress = vertexStreamBuffer->address + meshIndex * sizeof(VertexStreamInfo);
        }
    }

    package.ownedBuffers.push_back(geometryBuffer);
    if (vertexStreamBuffer)
    {
        package.ownedBuffers.push_back(vertexStreamBuffer);
    }
    hasPendingUpload = true;
}

void submitPendingUploads(bool hasPendingUpload)
{
    if (!hasPendingUpload)
    {
        return;
    }

    PlayResourceManager& uploadManager = PlayResourceManager::Instance();
    VkCommandBuffer      cmd           = uploadManager.getTempCommandBuffer();
    uploadManager.cmdUploadAppended(cmd);
    uploadManager.submitAndWaitTempCmdBuffer(cmd);
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

    std::vector<shaderio::GltfShadeMaterial> uploadedMaterials;
    uploadedMaterials.reserve(package.materials.size());
    for (shaderio::GltfShadeMaterial material : package.materials)
    {
        remapMaterialTextureInfos(material, textureInfoRemap);
        uploadedMaterials.push_back(material);
    }

    std::vector<MeshInfo> uploadedMeshInfos;
    uploadedMeshInfos.reserve(package.meshInfos.size());
    for (MeshInfo meshInfo : package.meshInfos)
    {
        if (meshInfo.materialIdx != INVALID_SCENE_ID)
        {
            meshInfo.materialIdx += materialBase;
        }
        uploadedMeshInfos.push_back(meshInfo);
    }

    bool hasPendingUpload = false;
    uploadModelGeometry(package, uploadedMeshInfos, hasPendingUpload);
    package.asset.transformBuffer = createAndAppendBuffer(package.asset.name + "_TransformBuffer", package.asset.transforms, hasPendingUpload);
    package.asset.materialBuffer  = createAndAppendBuffer(package.asset.name + "_MaterialBuffer", uploadedMaterials, hasPendingUpload);
    package.asset.meshInfoBuffer  = createAndAppendBuffer(package.asset.name + "_MeshInfoBuffer", uploadedMeshInfos, hasPendingUpload);
    submitPendingUploads(hasPendingUpload);

    for (const shaderio::GltfShadeMaterial& material : uploadedMaterials)
    {
        _common.materials.push_back(material);
    }

    for (const MeshInfo& meshInfo : uploadedMeshInfos)
    {
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
    if (!texture.texture && !texture.sourcePath.empty())
    {
        texture.texture =
            RefPtr<Texture>(new Texture(texture.sourcePath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.mipLevels, texture.isSrgb));
    }

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
