#include "Model.h"
#include "nvutils/logger.hpp"
#include "core/runtime/VulkanRuntime.h"
namespace Play
{

void Model::onLoadAsset()
{
    if (!std::filesystem::exists(this->_filePath))
    {
        LOGW("file {%s} is not exist");
        return;
    }
    _loadedModel = vpgloader::ModelLoader::Load(_filePath, {});

    {
        // uploading to gpu
        uint64_t positionDataSize   = _loadedModel->geometry.positions.size() * sizeof(_loadedModel->geometry.positions[0]);
        uint64_t normalDataSize     = _loadedModel->geometry.normals.size() * sizeof(_loadedModel->geometry.normals[0]);
        uint64_t tangentsDataSize   = _loadedModel->geometry.tangents.size() * sizeof(_loadedModel->geometry.tangents[0]);
        uint64_t texCoords0DataSize = _loadedModel->geometry.texCoords0.size() * sizeof(_loadedModel->geometry.texCoords0[0]);
        uint64_t texCoords1DataSize = _loadedModel->geometry.texCoords1.size() * sizeof(_loadedModel->geometry.texCoords1[0]);
        uint64_t colorsDataSize     = _loadedModel->geometry.colors.size() * sizeof(_loadedModel->geometry.colors[0]);
        uint64_t indicesDataSize    = _loadedModel->geometry.indices.size() * sizeof(_loadedModel->geometry.indices[0]);
        _renderData.assetBuffer     = RefPtr<Buffer>(new Buffer());
        VkBufferCreateInfo bufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.size += _loadedModel->geometry.positions.size() * sizeof(_loadedModel->geometry.positions[0]);
        bufferCreateInfo.size += _loadedModel->geometry.normals.size() * sizeof(_loadedModel->geometry.normals[0]);
        PlayResourceManager::Instance().createBuffer(*_renderData.assetBuffer, const VkBufferCreateInfo& bufferInfo,
                                                     const VmaAllocationCreateInfo& allocInfo)

            VkCommandBuffer cmdbuf = vkDriver->createTempCmdBuffer();
        nvvk::BufferRange   range;

        PlayResourceManager::Instance().appendBufferRange(range, nullptr);
    }
}

void Model::onSaveAsset() {}

} // namespace Play