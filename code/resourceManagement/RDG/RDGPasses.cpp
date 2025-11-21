#include "RDGPasses.hpp"
#include "RDG.h"

namespace Play::RDG
{

const uint32_t ATTACHMENT_DEPTH_STENCIL = 0xFFFFFFFF;
RenderPassBuilder::RenderPassBuilder(RDGBuilder* builder, RenderPassNodeRef node) : _builder(builder), _node(node), _dag(builder->getDag()) {}

std::atomic_int PresentPassNode::creationCount{0};
PresentPassNode::PresentPassNode(uint32_t id, std::string name) : PassNode(id, std::move(name), NodeType::eRenderPass)
{
    if (creationCount != 0)
    {
        LOGW("PresentPassNode should only be created once!");
        assert(false);
        return;
    }
    creationCount.fetch_add(1);
}

RenderPassBuilder& RenderPassBuilder::color(uint32_t slotIdx, RDGTextureRef texHandle, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                            VkImageLayout initLayout, VkImageLayout finalLayout)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.isAttachment                 = true;
    accessInfo.accessMask                   = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp                       = loadOp;
    accessInfo.storeOp                      = storeOp;
    accessInfo.attachSlotIdx                = slotIdx;
    accessInfo.layout                       = initLayout;
    accessInfo.attachFinalLayout            = finalLayout;
    accessInfo.stageMask                    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    _node->_textureStates[texHandle]        = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};

    return *this;
}

RenderPassBuilder& RenderPassBuilder::depthStencil(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                                   VkImageLayout initLayout, VkImageLayout finalLayout)
{
    TextureSubresourceAccessInfo subResources;
    TextureAccessInfo&           accessInfo = subResources.emplace_back();
    accessInfo.isAttachment                 = true;
    accessInfo.accessMask                   = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp                       = loadOp;
    accessInfo.storeOp                      = storeOp;
    accessInfo.attachSlotIdx                = ATTACHMENT_DEPTH_STENCIL;
    accessInfo.layout                       = initLayout;
    accessInfo.attachFinalLayout            = finalLayout;
    accessInfo.stageMask                    = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    _node->_textureStates[texHandle]        = {subResources, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = binding;
    accessInfo.isAttachment                 = false;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
    accessInfo.stageMask                    = stage;
    accessInfo.queueFamilyIndex             = queueFamilyIndex;
    _node->_textureStates[texture]          = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                           uint32_t offset, size_t size)
{
    BufferAccessInfo accessInfo;
    accessInfo.accessMask        = VK_ACCESS_2_UNIFORM_READ_BIT;
    accessInfo.stageMask         = stage;
    accessInfo.set               = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding           = binding;
    accessInfo.queueFamilyIndex  = queueFamilyIndex;
    accessInfo.offset            = offset;
    accessInfo.size              = size;
    _node->_bufferStates[buffer] = {accessInfo, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::readWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = binding;
    accessInfo.isAttachment                 = false;
    accessInfo.accessMask                   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_GENERAL;
    accessInfo.queueFamilyIndex             = queueFamilyIndex;
    _node->_textureStates[texture]          = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::readWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                                uint32_t offset, size_t size)
{
    BufferAccessInfo bufferAccess;
    bufferAccess.accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferAccess.stageMask        = stage;
    bufferAccess.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    bufferAccess.binding          = binding;
    bufferAccess.queueFamilyIndex = queueFamilyIndex;
    bufferAccess.offset           = offset;
    bufferAccess.size             = size;
    _node->_bufferStates[buffer]  = {bufferAccess, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::execute(std::function<void(RenderContext& context)> func)
{
    _node->setFunc(func);
    return *this;
}

ComputePassBuilder::ComputePassBuilder(RDGBuilder* builder, ComputePassNodeRef node) : _builder(builder), _node(node), _dag(builder->getDag()) {}

ComputePassBuilder& ComputePassBuilder::read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = binding;
    accessInfo.isAttachment                 = false;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
    accessInfo.stageMask                    = stage;
    accessInfo.queueFamilyIndex             = queueFamilyIndex;
    _node->_textureStates[texture]          = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

ComputePassBuilder& ComputePassBuilder::read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                             uint32_t offset, size_t size)
{
    BufferAccessInfo accessInfo;
    accessInfo.set     = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding = binding;
    accessInfo.accessMask =
        buffer->_info._usageFlags & VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT ? VK_ACCESS_2_UNIFORM_READ_BIT : VK_ACCESS_2_SHADER_READ_BIT;
    accessInfo.stageMask         = stage;
    accessInfo.queueFamilyIndex  = queueFamilyIndex;
    accessInfo.offset            = offset;
    accessInfo.size              = size;
    _node->_bufferStates[buffer] = {accessInfo, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

ComputePassBuilder& ComputePassBuilder::readWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = binding;
    accessInfo.isAttachment                 = false;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_GENERAL;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    accessInfo.stageMask                    = stage;
    accessInfo.queueFamilyIndex             = queueFamilyIndex;
    _node->_textureStates[texture]          = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

ComputePassBuilder& ComputePassBuilder::readWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                                  uint32_t offset, size_t size)
{
    BufferAccessInfo bufferAccess;
    bufferAccess.accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferAccess.stageMask        = stage;
    bufferAccess.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    bufferAccess.binding          = binding;
    bufferAccess.queueFamilyIndex = queueFamilyIndex;
    bufferAccess.offset           = offset;
    bufferAccess.size             = size;
    _node->_bufferStates[buffer]  = {bufferAccess, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

ComputePassBuilder& ComputePassBuilder::execute(std::function<void(RenderContext& context)> func)
{
    _node->setFunc(func);
    return *this;
}

ComputePassBuilder& ComputePassBuilder::async(bool isAsync)
{
    _node->setAsyncState(isAsync);
    return *this;
}

RTPassBuilder::RTPassBuilder(RDGBuilder* builder, RTPassNodeRef node) : _builder(builder), _node(node), _dag(builder->getDag()) {}

RTPassBuilder& RTPassBuilder::read(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = binding;
    accessInfo.isAttachment                 = false;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
    accessInfo.stageMask                    = stage;
    accessInfo.queueFamilyIndex             = queueFamilyIndex;
    _node->_textureStates[texture]          = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

RTPassBuilder& RTPassBuilder::read(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex, uint32_t offset,
                                   size_t size)
{
    BufferAccessInfo accessInfo;
    accessInfo.accessMask        = VK_ACCESS_2_UNIFORM_READ_BIT;
    accessInfo.stageMask         = stage;
    accessInfo.set               = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding           = binding;
    accessInfo.queueFamilyIndex  = queueFamilyIndex;
    accessInfo.offset            = offset;
    accessInfo.size              = size;
    _node->_bufferStates[buffer] = {accessInfo, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

RTPassBuilder& RTPassBuilder::readWrite(uint32_t binding, RDGTextureRef texture, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.set                          = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = binding;
    accessInfo.isAttachment                 = false;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_GENERAL;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    accessInfo.stageMask                    = stage;
    accessInfo.queueFamilyIndex             = queueFamilyIndex;
    _node->_textureStates[texture]          = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

RTPassBuilder& RTPassBuilder::readWrite(uint32_t binding, RDGBufferRef buffer, VkPipelineStageFlagBits2 stage, uint32_t queueFamilyIndex,
                                        uint32_t offset, size_t size)
{
    BufferAccessInfo bufferAccess;
    bufferAccess.accessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    bufferAccess.stageMask        = stage;
    bufferAccess.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    bufferAccess.binding          = binding;
    bufferAccess.queueFamilyIndex = queueFamilyIndex;
    bufferAccess.offset           = offset;
    bufferAccess.size             = size;
    _node->_bufferStates[buffer]  = {bufferAccess, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

RTPassBuilder& RTPassBuilder::execute(std::function<void(RenderContext& context)> func)
{
    _node->setFunc(func);
    return *this;
}

PresentPassBuilder::PresentPassBuilder(RDGBuilder* builder, PresentPassNode* node) : _builder(builder), _node(node), _dag(builder->getDag())
{
    RenderProgram* presentProgram = new RenderProgram(_builder->_element->getDevice());

    presentProgram->setFragModuleID(ShaderManager::Instance().getShaderIdByName("postProcessf"))
        .setVertexModuleID(ShaderManager::Instance().getShaderIdByName("postProcessv"))
        .finish();
    _node->setProgram(presentProgram);

    return;
}
PresentPassBuilder& PresentPassBuilder::color(RDGTextureRef texHandle)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.isAttachment                 = true;
    accessInfo.accessMask                   = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp                       = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    accessInfo.storeOp                      = VK_ATTACHMENT_STORE_OP_STORE;
    accessInfo.attachSlotIdx                = 0;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    accessInfo.attachFinalLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    _node->_textureStates[texHandle]        = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}

PresentPassBuilder& PresentPassBuilder::read(RDGTextureRef texHandle)
{
    TextureSubresourceAccessInfo subResource;
    TextureAccessInfo&           accessInfo = subResource.emplace_back();
    accessInfo.isAttachment                 = false;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    accessInfo.set                          = static_cast<uint32_t>(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding                      = 0;
    accessInfo.stageMask                    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    _node->_textureStates[texHandle]        = {subResource, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
    return *this;
}
PresentPassNode* PresentPassBuilder::finish()
{
    _node->setFunc([](RenderContext& context) { vkCmdDraw(context._currCmdBuffer, 3, 1, 0, 0); });
    return _node;
}
} // namespace Play::RDG