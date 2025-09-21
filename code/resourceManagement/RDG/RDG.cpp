#include "RDG.h"
#include <stdexcept>
#include "queue"
#include "utils.hpp"
#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include "nvvk/debug_util.hpp"
namespace Play::RDG
{
void RDGTextureCache::regist(Texture* texture) {}

RDGTexture* RDGTextureCache::request(Texture* texture)
{
    return nullptr;
}

RDGTextureBuilder& RDGTextureBuilder::Format(VkFormat format)
{
    _desc._format = format;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::Type(VkImageType type)
{
    _desc._type = type;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::Extent(VkExtent3D extent)
{
    _desc._extent = extent;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::UsageFlags(VkImageUsageFlags usageFlags)
{
    _desc._usageFlags = usageFlags;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::AspectFlags(VkImageAspectFlags aspectFlags)
{
    _desc._aspectFlags = aspectFlags;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::SampleCount(VkSampleCountFlagBits sampleCount)
{
    _desc._sampleCount = sampleCount;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::MipmapLevel(uint32_t mipmapLevel)
{
    _desc._mipmapLevel = mipmapLevel;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::LayerCount(uint32_t layerCount)
{
    _desc._layerCount = layerCount;
    return *this;
}

TextureNodeRef RDGTextureBuilder::finish()
{
    auto* _texture = _textureNode->getRHI();
    if (!_texture)
    {
        throw std::runtime_error("Texture pointer is null.");
    }
    _texture->Format()      = _desc._format;
    _texture->Type()        = _desc._type;
    _texture->Extent()      = _desc._extent;
    _texture->UsageFlags()  = _desc._usageFlags;
    _texture->AspectFlags() = _desc._aspectFlags;
    _texture->SampleCount() = _desc._sampleCount;
    _texture->MipLevel()    = _desc._mipmapLevel;
    _texture->LayerCount()  = _desc._layerCount;
    return _textureNode;
}

RDGBufferBuilder& RDGBufferBuilder::Size(VkDeviceSize size)
{
    _desc._size = size;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Range(VkDeviceSize range)
{
    _desc._range = range;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::UsageFlags(VkBufferUsageFlags usageFlags)
{
    _desc._usageFlags = usageFlags;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Location(bool isDeviceLocal)
{
    _desc._location = isDeviceLocal ? BufferDesc::MemoryLocation::eDeviceLocal
                                    : BufferDesc::MemoryLocation::eHostVisible;
    return *this;
}

BufferNodeRef RDGBufferBuilder::finish()
{
    auto* _buffer = _bufferNode->getRHI();
    if (!_buffer)
    {
        throw std::runtime_error("Buffer pointer is null.");
    }
    _buffer->UsageFlags()  = _desc._usageFlags;
    _buffer->BufferSize()  = _desc._size;
    _buffer->BufferRange() = _desc._range;
    _buffer->BufferProperty() =
        _desc._location == BufferDesc::MemoryLocation::eDeviceLocal
            ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return _bufferNode;
}

void BlackBoard::registTexture(std::string name, TextureNodeRef texture)
{
}

void BlackBoard::registBuffer(std::string name, BufferNodeRef buffer)
{
}

void BlackBoard::registPass(std::string name, PassNode* pass)
{
}

TextureNodeRef BlackBoard::getTexture(std::string name)
{
    return _textureMap[name];
}

BufferNodeRef BlackBoard::getBuffer(std::string name)
{
    return _bufferMap[name];
}

PassNode* BlackBoard::getPass(std::string name)
{
    return _passMap[name];
}

RDGBuilder::RDGBuilder(PlayElement* element) {}

RDGBuilder::~RDGBuilder() {}

RenderPassBuilder RDGBuilder::createRenderPass(std::string name)
{
    RenderPassNodeRef nodeRef = _dag->addNode<RenderPassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(name, nodeRef);
    return RenderPassBuilder(this, nodeRef);
}

ComputePassBuilder RDGBuilder::createComputePass(std::string name)
{
    ComputePassNodeRef nodeRef = _dag->addNode<ComputePassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(name, nodeRef);
    return ComputePassBuilder(this, nodeRef);
}

RTPassBuilder RDGBuilder::createRTPass(std::string name)
{
    RTPassNodeRef nodeRef = _dag->addNode<RTPassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(name, nodeRef);
    return RTPassBuilder(this, nodeRef);
}

RDGTextureBuilder RDGBuilder::createTexture(std::string name)
{
    TextureNodeRef    node = _dag->addNode<TextureNode>(Texture::Create(name));
    RDGTextureBuilder builder(this, node);
    _blackBoard.registTexture(name, node);
    return builder;
}

RDGBufferBuilder RDGBuilder::createBuffer(std::string name)
{
    BufferNodeRef    node = _dag->addNode<BufferNode>(Buffer::Create(name));
    RDGBufferBuilder builder(this, node);
    _blackBoard.registBuffer(name, node);
    return builder;
}

TextureNodeRef RDGBuilder::getTexture(std::string name)
{
    return _blackBoard.getTexture(name);
}

BufferNodeRef RDGBuilder::getBuffer(std::string name)
{
    return _blackBoard.getBuffer(name);
}

void RDGBuilder::execute(RenderPassNode* pass) {}

void RDGBuilder::execute(ComputePassNode* pass) {}

void RDGBuilder::execute(RTPassNode* pass) {}
} // namespace Play::RDG