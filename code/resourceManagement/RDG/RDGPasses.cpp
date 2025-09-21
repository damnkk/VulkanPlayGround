#include "RDGPasses.hpp"
#include "RDG.h"

namespace Play::RDG
{
RootSignature::RootSignature() {}

RootSignature::~RootSignature() {}

void RootSignature::addBinding(uint32_t set, uint32_t binding, VkDescriptorType descriptorType,
                               uint32_t descriptorCount, VkShaderStageFlags stageFlags,
                               const VkSampler*         pImmutableSamplers,
                               VkDescriptorBindingFlags bindingFlags)
{
    if (set >= _descritorBindings.size())
    {
        _descritorBindings.resize(set + 1);
    }
    auto& bindings = _descritorBindings[set];
    bindings.addBinding(binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers,
                        bindingFlags);
}

void RootSignature::clear()
{
    for (auto& bindings : _descritorBindings)
    {
        bindings.clear();
    }
}

void RootSignature::clear(uint32_t set)
{
    if (set < _descritorBindings.size())
    {
        _descritorBindings[set].clear();
    }
}

nvvk::DescriptorBindings& RootSignature::getBinding(uint32_t set)
{
    if (set >= _descritorBindings.size())
    {
        _descritorBindings.resize(set + 1);
    }
    return _descritorBindings[set];
}

const uint32_t ATTACHMENT_DEPTH_STENCIL = 0xFFFFFFFF;
RenderPassBuilder::RenderPassBuilder(RDGBuilder* builder, RenderPassNodeRef node) {}

RenderPassBuilder& RenderPassBuilder::color(uint32_t binding, TextureNodeRef texHandle,
                                            VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                            VkImageLayout initLayout, VkImageLayout finalLayout)
{
    assert(texHandle && texHandle->getRHI()->isValid());

    AttachmentEdge* inputEdge =
        _dag->createEdge<AttachmentEdge>(texHandle, this->_node, EdgeType::eRenderAttachment);
    inputEdge->layout              = initLayout;
    inputEdge->attachmentOp.loadOp = loadOp;
    inputEdge->slotIdx             = binding;

    AttachmentEdge* outputEdge =
        _dag->createEdge<AttachmentEdge>(this->_node, texHandle, EdgeType::eRenderAttachment);
    outputEdge->layout               = finalLayout;
    outputEdge->attachmentOp.storeOp = storeOp;
    outputEdge->slotIdx              = binding;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::depthStencil(TextureNodeRef      texHandle,
                                                   VkAttachmentLoadOp  loadOp,
                                                   VkAttachmentStoreOp storeOp,
                                                   VkImageLayout       initLayout,
                                                   VkImageLayout       finalLayout)
{
    assert(texHandle && texHandle->getRHI()->isValid());
    AttachmentEdge* inputEdge =
        _dag->createEdge<AttachmentEdge>(texHandle, this->_node, EdgeType::eRenderAttachment);
    inputEdge->layout              = initLayout;
    inputEdge->attachmentOp.loadOp = loadOp;
    inputEdge->slotIdx             = ATTACHMENT_DEPTH_STENCIL;

    AttachmentEdge* outputEdge =
        _dag->createEdge<AttachmentEdge>(this->_node, texHandle, EdgeType::eRenderAttachment);
    outputEdge->layout               = finalLayout;
    outputEdge->attachmentOp.storeOp = storeOp;
    outputEdge->slotIdx              = ATTACHMENT_DEPTH_STENCIL;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::read(uint32_t set, uint32_t binding, TextureNodeRef texture,
                                           VkPipelineStageFlagBits2 stage,
                                           uint32_t                 queueFamilyIndex)
{
    assert(texture && texture->getRHI()->isValid());
    TextureEdge* inputEdge =
        _dag->createEdge<TextureEdge>(texture, this->_node, EdgeType::eTexture);
    inputEdge->layout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT;
    inputEdge->stageMask        = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::read(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                                           VkPipelineStageFlagBits2 stage,
                                           uint32_t queueFamilyIndex, uint32_t offset,
                                           uint32_t size)
{
    assert(buffer && buffer->getRHI()->isValid());
    BufferEdge* inputEdge = _dag->createEdge<BufferEdge>(buffer, this->_node, EdgeType::eBuffer);
    inputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT;
    inputEdge->stageMask  = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    inputEdge->offset          = offset;
    inputEdge->size            = size;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::readWrite(uint32_t set, uint32_t binding,
                                                TextureNodeRef           texture,
                                                VkPipelineStageFlagBits2 stage,
                                                uint32_t                 queueFamilyIndex)
{
    assert(texture && texture->getRHI()->isValid());
    TextureEdge* inputEdge =
        _dag->createEdge<TextureEdge>(texture, this->_node, EdgeType::eTexture);
    inputEdge->layout           = VK_IMAGE_LAYOUT_GENERAL;
    inputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    inputEdge->stageMask        = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;

    TextureEdge* outputEdge =
        _dag->createEdge<TextureEdge>(this->_node, texture, EdgeType::eTexture);
    outputEdge->layout           = VK_IMAGE_LAYOUT_GENERAL;
    outputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    outputEdge->stageMask        = stage;
    outputEdge->queueFamilyIndex = queueFamilyIndex;

    outputEdge->set             = set;
    outputEdge->binding         = binding;
    outputEdge->descriptorIndex = 0;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::readWrite(uint32_t set, uint32_t binding,
                                                BufferNodeRef            buffer,
                                                VkPipelineStageFlagBits2 stage,
                                                uint32_t queueFamilyIndex, uint32_t offset,
                                                uint32_t size)
{
    assert(buffer && buffer->getRHI()->isValid());
    BufferEdge* inputEdge = _dag->createEdge<BufferEdge>(buffer, this->_node, EdgeType::eBuffer);
    inputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    inputEdge->stageMask  = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    inputEdge->offset          = offset;
    inputEdge->size            = size;

    BufferEdge* outputEdge = _dag->createEdge<BufferEdge>(this->_node, buffer, EdgeType::eBuffer);
    outputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    outputEdge->stageMask  = stage;
    outputEdge->queueFamilyIndex = queueFamilyIndex;

    outputEdge->set             = set;
    outputEdge->binding         = binding;
    outputEdge->descriptorIndex = 0;
    outputEdge->offset          = offset;
    outputEdge->size            = size;
    return *this;
}

ComputePassBuilder::ComputePassBuilder(RDGBuilder* builder, ComputePassNodeRef node)
    : _builder(builder), _node(node), _dag(builder->getDag())
{
}

ComputePassBuilder& ComputePassBuilder::read(uint32_t set, uint32_t binding, TextureNodeRef texture,
                                             VkPipelineStageFlagBits2 stage,
                                             uint32_t                 queueFamilyIndex)
{
    assert(texture && texture->getRHI()->isValid());
    TextureEdge* inputEdge =
        _dag->createEdge<TextureEdge>(texture, this->_node, EdgeType::eTexture);
    inputEdge->layout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT;
    inputEdge->stageMask        = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    return *this;
}

ComputePassBuilder& ComputePassBuilder::read(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                                             VkPipelineStageFlagBits2 stage,
                                             uint32_t queueFamilyIndex, uint32_t offset,
                                             uint32_t size)
{
    assert(buffer && buffer->getRHI()->isValid());
    BufferEdge* inputEdge = _dag->createEdge<BufferEdge>(buffer, this->_node, EdgeType::eBuffer);
    inputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT;
    inputEdge->stageMask  = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    inputEdge->offset          = offset;
    inputEdge->size            = size;
    return *this;
}

ComputePassBuilder& ComputePassBuilder::readWrite(uint32_t set, uint32_t binding,
                                                  TextureNodeRef           texture,
                                                  VkPipelineStageFlagBits2 stage,
                                                  uint32_t                 queueFamilyIndex)
{
    assert(texture && texture->getRHI()->isValid());
    TextureEdge* inputEdge =
        _dag->createEdge<TextureEdge>(texture, this->_node, EdgeType::eTexture);
    inputEdge->layout           = VK_IMAGE_LAYOUT_GENERAL;
    inputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    inputEdge->stageMask        = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;

    TextureEdge* outputEdge =
        _dag->createEdge<TextureEdge>(this->_node, texture, EdgeType::eTexture);
    outputEdge->layout           = VK_IMAGE_LAYOUT_GENERAL;
    outputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    outputEdge->stageMask        = stage;
    outputEdge->queueFamilyIndex = queueFamilyIndex;

    outputEdge->set             = set;
    outputEdge->binding         = binding;
    outputEdge->descriptorIndex = 0;
    return *this;
}

ComputePassBuilder& ComputePassBuilder::readWrite(uint32_t set, uint32_t binding,
                                                  BufferNodeRef            buffer,
                                                  VkPipelineStageFlagBits2 stage,
                                                  uint32_t queueFamilyIndex, uint32_t offset,
                                                  uint32_t size)
{
    assert(buffer && buffer->getRHI()->isValid());
    BufferEdge* inputEdge = _dag->createEdge<BufferEdge>(buffer, this->_node, EdgeType::eBuffer);
    inputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    inputEdge->stageMask  = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    inputEdge->offset          = offset;
    inputEdge->size            = size;

    BufferEdge* outputEdge = _dag->createEdge<BufferEdge>(this->_node, buffer, EdgeType::eBuffer);
    outputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    outputEdge->stageMask  = stage;
    outputEdge->queueFamilyIndex = queueFamilyIndex;

    outputEdge->set             = set;
    outputEdge->binding         = binding;
    outputEdge->descriptorIndex = 0;
    outputEdge->offset          = offset;
    outputEdge->size            = size;
    return *this;
}

RTPassBuilder::RTPassBuilder(RDGBuilder* builder, RTPassNodeRef node)
    : _builder(builder), _node(node), _dag(builder->getDag())
{
}

RTPassBuilder& RTPassBuilder::read(uint32_t set, uint32_t binding, TextureNodeRef texture,
                                   VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    assert(texture && texture->getRHI()->isValid());
    TextureEdge* inputEdge =
        _dag->createEdge<TextureEdge>(texture, this->_node, EdgeType::eTexture);
    inputEdge->layout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT;
    inputEdge->stageMask        = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    return *this;
}

RTPassBuilder& RTPassBuilder::read(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                                   VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                   uint32_t offset, uint32_t size)
{
    assert(buffer && buffer->getRHI()->isValid());
    BufferEdge* inputEdge = _dag->createEdge<BufferEdge>(buffer, this->_node, EdgeType::eBuffer);
    inputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT;
    inputEdge->stageMask  = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    inputEdge->offset          = offset;
    inputEdge->size            = size;
    return *this;
}

RTPassBuilder& RTPassBuilder::readWrite(uint32_t set, uint32_t binding, TextureNodeRef texture,
                                        VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    assert(texture && texture->getRHI()->isValid());
    TextureEdge* inputEdge =
        _dag->createEdge<TextureEdge>(texture, this->_node, EdgeType::eTexture);
    inputEdge->layout           = VK_IMAGE_LAYOUT_GENERAL;
    inputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    inputEdge->stageMask        = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;

    TextureEdge* outputEdge =
        _dag->createEdge<TextureEdge>(this->_node, texture, EdgeType::eTexture);
    outputEdge->layout           = VK_IMAGE_LAYOUT_GENERAL;
    outputEdge->accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    outputEdge->stageMask        = stage;
    outputEdge->queueFamilyIndex = queueFamilyIndex;

    outputEdge->set             = set;
    outputEdge->binding         = binding;
    outputEdge->descriptorIndex = 0;
    return *this;
}

RTPassBuilder& RTPassBuilder::readWrite(uint32_t set, uint32_t binding, BufferNodeRef buffer,
                                        VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                        uint32_t offset, uint32_t size)
{
    assert(buffer && buffer->getRHI()->isValid());
    BufferEdge* inputEdge = _dag->createEdge<BufferEdge>(buffer, this->_node, EdgeType::eBuffer);
    inputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    inputEdge->stageMask  = stage;
    inputEdge->queueFamilyIndex = queueFamilyIndex;

    inputEdge->set             = set;
    inputEdge->binding         = binding;
    inputEdge->descriptorIndex = 0;
    inputEdge->offset          = offset;
    inputEdge->size            = size;

    BufferEdge* outputEdge = _dag->createEdge<BufferEdge>(this->_node, buffer, EdgeType::eBuffer);
    outputEdge->accessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    outputEdge->stageMask  = stage;
    outputEdge->queueFamilyIndex = queueFamilyIndex;

    outputEdge->set             = set;
    outputEdge->binding         = binding;
    outputEdge->descriptorIndex = 0;
    outputEdge->offset          = offset;
    outputEdge->size            = size;
    return *this;
}
} // namespace Play::RDG