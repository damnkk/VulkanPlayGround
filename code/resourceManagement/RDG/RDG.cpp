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

RDGTextureBuilder& RDGTextureBuilder::Import(Texture* texture, VkAccessFlags2 accessMask,
                                             VkImageLayout layout, VkPipelineStageFlags2 stageMask,
                                             uint32_t queueFamilyIndex)
{
    this->_textureNode->setRHI(texture);
    this->_textureNode->_info._format      = texture->Format();
    this->_textureNode->_info._type        = texture->Type();
    this->_textureNode->_info._extent      = texture->Extent();
    this->_textureNode->_info._usageFlags  = texture->UsageFlags();
    this->_textureNode->_info._aspectFlags = texture->AspectFlags();
    this->_textureNode->_info._sampleCount = texture->SampleCount();
    this->_textureNode->_info._mipmapLevel = texture->MipLevel();
    this->_textureNode->_info._layerCount  = texture->LayerCount();
    InputPassNodeRef inputNode = this->_builder->createInputPass(texture->DebugName() + "_import");
    TextureEdge*     edge =
        this->_builder->getDag()->createEdge<TextureEdge>(inputNode, this->_textureNode);
    edge->accessMask       = accessMask;
    edge->layout           = layout;
    edge->stageMask        = stageMask;
    edge->queueFamilyIndex = queueFamilyIndex;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::Format(VkFormat format)
{
    _textureNode->_info._format = format;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::Type(VkImageType type)
{
    _textureNode->_info._type = type;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::Extent(VkExtent3D extent)
{
    _textureNode->_info._extent = extent;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::UsageFlags(VkImageUsageFlags usageFlags)
{
    _textureNode->_info._usageFlags = usageFlags;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::AspectFlags(VkImageAspectFlags aspectFlags)
{
    _textureNode->_info._aspectFlags = aspectFlags;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::SampleCount(VkSampleCountFlagBits sampleCount)
{
    _textureNode->_info._sampleCount = sampleCount;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::MipmapLevel(uint32_t mipmapLevel)
{
    _textureNode->_info._mipmapLevel = mipmapLevel;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::LayerCount(uint32_t layerCount)
{
    _textureNode->_info._layerCount = layerCount;
    return *this;
}

TextureNodeRef RDGTextureBuilder::finish()
{
    return _textureNode;
}

RDGBufferBuilder& RDGBufferBuilder::Size(VkDeviceSize size)
{
    _bufferNode->_info._size = size;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Range(VkDeviceSize range)
{
    _bufferNode->_info._range = range;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::UsageFlags(VkBufferUsageFlags usageFlags)
{
    _bufferNode->_info._usageFlags = usageFlags;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Location(bool isDeviceLocal)
{
    _bufferNode->_info._location = isDeviceLocal
                                       ? BufferNode::BufferDesc::MemoryLocation::eDeviceLocal
                                       : BufferNode::BufferDesc::MemoryLocation::eHostVisible;
    return *this;
}

BufferNodeRef RDGBufferBuilder::finish()
{
    return _bufferNode;
}

void BlackBoard::registTexture(std::string name, TextureNodeRef texture) {}

void BlackBoard::registBuffer(std::string name, BufferNodeRef buffer) {}

void BlackBoard::registPass(std::string name, PassNode* pass) {}

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

InputPassNodeRef RDGBuilder::createInputPass(std::string name)
{
    InputPassNodeRef nodeRef = _dag->addNode<InputPassNode>(std::move(name));
    return nodeRef;
}

RDGTextureBuilder RDGBuilder::createTexture(std::string name)
{
    TextureNodeRef    node = _dag->addNode<TextureNode>(name);
    RDGTextureBuilder builder(this, node);
    _blackBoard.registTexture(name, node);
    return builder;
}

RDGBufferBuilder RDGBuilder::createBuffer(std::string name)
{
    BufferNodeRef    node = _dag->addNode<BufferNode>(name);
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

VkCommandBuffer RenderContext::acquireCmdBuffer()
{
    return VK_NULL_HANDLE;
}
} // namespace Play::RDG