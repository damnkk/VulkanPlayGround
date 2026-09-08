#include "Model.h"
#include "nvutils/logger.hpp"
#include "nvvk/check_error.hpp"
#include "core/runtime/VulkanRuntime.h"
CEREAL_REGISTER_TYPE(Play::Model)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Play::Asset, Play::Model)
namespace Play
{

namespace
{
struct BufferRegion
{
    VkDeviceSize offset = 0;
    VkDeviceSize size   = 0;
};

BufferRegion appendAlignedRegion(VkDeviceSize& bufferSize, VkDeviceSize size)
{
    constexpr VkDeviceSize alignment = 16;
    if (size == 0)
    {
        return {};
    }

    bufferSize = (bufferSize + alignment - 1) & ~(alignment - 1);
    BufferRegion region{bufferSize, size};
    bufferSize += size;
    return region;
}
} // namespace

void Model::onLoadAsset()
{
    if (!std::filesystem::exists(this->_filePath))
    {
        LOGW("file {%s} does not exist\n", _filePath.c_str());
        return;
    }
    _loadedModel = vpgloader::ModelLoader::Load(_filePath, {});
    if (!_loadedModel)
    {
        LOGW("VPGLoader returned an empty model for {%s}\n", _filePath.c_str());
        return;
    }

    const vpgloader::ModelGeometryData& geometry = _loadedModel->geometry;

    VkDeviceSize       bufferSize         = 0;
    const BufferRegion positionRegion     = appendAlignedRegion(bufferSize, geometry.positions.size() * sizeof(geometry.positions[0]));
    const BufferRegion normalRegion       = appendAlignedRegion(bufferSize, geometry.normals.size() * sizeof(geometry.normals[0]));
    const BufferRegion tangentRegion      = appendAlignedRegion(bufferSize, geometry.tangents.size() * sizeof(geometry.tangents[0]));
    const BufferRegion texCoord0Region    = appendAlignedRegion(bufferSize, geometry.texCoords0.size() * sizeof(geometry.texCoords0[0]));
    const BufferRegion texCoord1Region    = appendAlignedRegion(bufferSize, geometry.texCoords1.size() * sizeof(geometry.texCoords1[0]));
    const BufferRegion colorRegion        = appendAlignedRegion(bufferSize, geometry.colors.size() * sizeof(geometry.colors[0]));
    const BufferRegion indexRegion        = appendAlignedRegion(bufferSize, geometry.indices.size() * sizeof(geometry.indices[0]));
    const BufferRegion vertexStreamRegion = appendAlignedRegion(bufferSize, _loadedModel->meshes.size() * sizeof(ModelVertexStreamInfo));
    const BufferRegion meshInfoRegion     = appendAlignedRegion(bufferSize, _loadedModel->meshes.size() * sizeof(ModelMeshInfo));
    const BufferRegion materialRegion     = appendAlignedRegion(bufferSize, _loadedModel->materials.size() * sizeof(_loadedModel->materials[0]));
    const BufferRegion textureInfoRegion  = appendAlignedRegion(bufferSize, _loadedModel->textureInfos.size() * sizeof(ModelTextureInfo));
    if (bufferSize == 0)
    {
        LOGW("Model {%s} contains no buffer data\n", _filePath.c_str());
        return;
    }

    _renderData.assetBuffer = RefPtr<Buffer>(new Buffer(
        "Model asset buffer", VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
        bufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    const uint64_t baseAddress = _renderData.assetBuffer->address;
    auto regionAddress         = [baseAddress](const BufferRegion& region) -> uint64_t { return region.size == 0 ? 0 : baseAddress + region.offset; };

    _renderData.modelDesc.positionAddress    = regionAddress(positionRegion);
    _renderData.modelDesc.normalAddress      = regionAddress(normalRegion);
    _renderData.modelDesc.tangentAddress     = regionAddress(tangentRegion);
    _renderData.modelDesc.texCoord0Address   = regionAddress(texCoord0Region);
    _renderData.modelDesc.texCoord1Address   = regionAddress(texCoord1Region);
    _renderData.modelDesc.colorAddress       = regionAddress(colorRegion);
    _renderData.modelDesc.indexAddress       = regionAddress(indexRegion);
    _renderData.modelDesc.meshInfoAddress    = regionAddress(meshInfoRegion);
    _renderData.modelDesc.materialAddress    = regionAddress(materialRegion);
    _renderData.modelDesc.textureInfoAddress = regionAddress(textureInfoRegion);
    _renderData.modelDesc.meshCount          = static_cast<uint32_t>(_loadedModel->meshes.size());
    _renderData.modelDesc.materialCount      = static_cast<uint32_t>(_loadedModel->materials.size());
    _renderData.modelDesc.textureInfoCount   = static_cast<uint32_t>(_loadedModel->textureInfos.size());

    std::vector<ModelVertexStreamInfo> vertexStreams(_loadedModel->meshes.size());
    std::vector<ModelMeshInfo>         meshInfos(_loadedModel->meshes.size());
    for (size_t meshIndex = 0; meshIndex < _loadedModel->meshes.size(); ++meshIndex)
    {
        const vpgloader::ModelMeshAsset& sourceMesh   = _loadedModel->meshes[meshIndex];
        ModelVertexStreamInfo&           vertexStream = vertexStreams[meshIndex];
        vertexStream.positionAddress  = _renderData.modelDesc.positionAddress + sourceMesh.firstVertex * sizeof(geometry.positions[0]);
        vertexStream.normalAddress    = _renderData.modelDesc.normalAddress + sourceMesh.firstVertex * sizeof(geometry.normals[0]);
        vertexStream.tangentAddress   = _renderData.modelDesc.tangentAddress + sourceMesh.firstVertex * sizeof(geometry.tangents[0]);
        vertexStream.texCoord0Address = _renderData.modelDesc.texCoord0Address + sourceMesh.firstVertex * sizeof(geometry.texCoords0[0]);
        vertexStream.texCoord1Address = _renderData.modelDesc.texCoord1Address + sourceMesh.firstVertex * sizeof(geometry.texCoords1[0]);
        vertexStream.colorAddress     = _renderData.modelDesc.colorAddress + sourceMesh.firstVertex * sizeof(geometry.colors[0]);

        ModelMeshInfo& meshInfo      = meshInfos[meshIndex];
        meshInfo.vertexStreamAddress = regionAddress(vertexStreamRegion) + meshIndex * sizeof(ModelVertexStreamInfo);
        meshInfo.indexAddress        = _renderData.modelDesc.indexAddress + sourceMesh.firstIndex * sizeof(geometry.indices[0]);
        meshInfo.indexCount          = sourceMesh.indexCount;
        meshInfo.materialIndex       = sourceMesh.materialIndex;
    }

    std::vector<ModelTextureInfo> textureInfos(_loadedModel->textureInfos.size());
    for (size_t textureInfoIndex = 0; textureInfoIndex < _loadedModel->textureInfos.size(); ++textureInfoIndex)
    {
        const vpgloader::ModelTextureInfo& sourceInfo  = _loadedModel->textureInfos[textureInfoIndex];
        ModelTextureInfo&                  textureInfo = textureInfos[textureInfoIndex];
        textureInfo.offset                             = sourceInfo.offset;
        textureInfo.scale                              = sourceInfo.scale;
        textureInfo.rotation                           = sourceInfo.rotation;
        textureInfo.textureIndex = sourceInfo.textureIndex == vpgloader::InvalidModelIndex ? -1 : static_cast<int32_t>(sourceInfo.textureIndex);
        textureInfo.texCoord     = sourceInfo.texCoord > 1 ? 1 : sourceInfo.texCoord;
    }

    PlayResourceManager& uploader          = PlayResourceManager::Instance();
    const uint32_t       transferFamily    = vkDriver->getTransferQueue().familyIndex;
    const uint32_t       graphicsFamily    = vkDriver->getGfxQueue().familyIndex;
    const bool           transferOwnership = transferFamily != graphicsFamily;
    uploader.setEnableOwnerBarriers(transferOwnership, transferFamily, graphicsFamily);
    uploader.beginTransferOnly();

    auto appendRegion = [this, &uploader](const BufferRegion& region, const void* data)
    {
        if (region.size != 0)
        {
            NVVK_CHECK(uploader.appendBuffer(*_renderData.assetBuffer, region.offset, region.size, data));
        }
    };
    appendRegion(positionRegion, geometry.positions.data());
    appendRegion(normalRegion, geometry.normals.data());
    appendRegion(tangentRegion, geometry.tangents.data());
    appendRegion(texCoord0Region, geometry.texCoords0.data());
    appendRegion(texCoord1Region, geometry.texCoords1.data());
    appendRegion(colorRegion, geometry.colors.data());
    appendRegion(indexRegion, geometry.indices.data());
    appendRegion(vertexStreamRegion, vertexStreams.data());
    appendRegion(meshInfoRegion, meshInfos.data());
    appendRegion(materialRegion, _loadedModel->materials.data());
    appendRegion(textureInfoRegion, textureInfos.data());

    const nvvk::BarrierContainer acquireBarriers = uploader.getOwnerAcquisitionBarriers();
    VkCommandBuffer              transferCmd     = vkDriver->createTransferTempCmdBuffer();
    uploader.cmdUploadAppended(transferCmd);
    uploader.setEnableOwnerBarriers(false, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
    vkDriver->submitAndWaitTransferTempCmdBuffer(transferCmd);

    if (transferOwnership)
    {
        VkCommandBuffer graphicsCmd = vkDriver->createTempCmdBuffer();
        acquireBarriers.cmdPipelineBarrier(graphicsCmd, 0);
        vkDriver->submitAndWaitTempCmdBuffer(graphicsCmd);
    }
}

} // namespace Play
