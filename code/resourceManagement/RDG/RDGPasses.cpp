#include "RDGPasses.hpp"
#include "RDG.h"
#include "RenderPass.h"
#include <VulkanDriver.h>

namespace Play::RDG
{
const uint32_t ATTACHMENT_DEPTH_STENCIL = 0xFFFFFFFF;
RenderPassNode::RenderPassNode(uint32_t id, std::string name) : PassNode(id, std::move(name), NodeType::eRenderPass) {}
void RenderPassNode::initRenderPass()
{
    if (_renderPass && !_renderPass->isDirty()) return;

    if (vkDriver->_enableDynamicRendering)
    {
        _renderPass = std::make_unique<DynamicRenderPass>(this);
    }
    else
    {
        _renderPass = std::make_unique<LegacyRenderPass>(this);
    }
    RenderPassConfig config;
    for (auto& [texture, state] : _textureStates)
    {
        const TextureAccessInfo& accessInfo = state.textureStates[0];
        if (!accessInfo.isAttachment) continue;
        const VkImageMemoryBarrier2& barrierInfo = state.barrierInfo;
        // if curr attachment's barrier is invalid, that means this is a Frame-to-Frame Dependency
        // RDG::Compile()can't infer barrier in this situation with only one invocation
        bool               isFrameToFrameDependency = !isImageBarrierValid(barrierInfo);
        TextureAccessInfo* finalAccessInfo          = texture->getAttachmentFinalAccessInfo();
        // so we made the right barrier info at here
        if (isFrameToFrameDependency)
        {
            assert(finalAccessInfo);
            VkImageMemoryBarrier2& imageBarrier = state.barrierInfo;
            imageBarrier.sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imageBarrier.srcAccessMask          = finalAccessInfo->accessMask;
            imageBarrier.dstAccessMask          = accessInfo.accessMask;
            imageBarrier.srcStageMask           = finalAccessInfo->stageMask;
            imageBarrier.dstStageMask           = accessInfo.stageMask;
            imageBarrier.oldLayout              = finalAccessInfo->attachFinalLayout;
            imageBarrier.newLayout              = accessInfo.layout;
            imageBarrier.srcQueueFamilyIndex    = finalAccessInfo->queueFamilyIndex;
            imageBarrier.dstQueueFamilyIndex    = accessInfo.queueFamilyIndex;
            imageBarrier.subresourceRange       = {texture->_info._aspectFlags, 0, texture->_info._mipmapLevel, 0, texture->_info._layerCount};
            imageBarrier.image                  = texture->getRHI()->image;
        }

        if (accessInfo.attachSlotIdx == ATTACHMENT_DEPTH_STENCIL)
        {
            RenderPassAttachment depthStencilAttachment;
            depthStencilAttachment.image     = texture->getRHI()->image;
            depthStencilAttachment.imageView = texture->getRHI()->descriptor.imageView;
            depthStencilAttachment.format    = texture->getRHI()->format;
            depthStencilAttachment.samples   = texture->getRHI()->SampleCount();
            depthStencilAttachment.slotIndex = accessInfo.attachSlotIdx;
            depthStencilAttachment.loadOp    = accessInfo.loadOp;
            depthStencilAttachment.storeOp   = accessInfo.storeOp;
            // 这里是一个通用数据结构体，必须填写accessInfo记录的layout信息，因为vkRenderPass对initlayout的准度要求更高，而barrier有时可用undefineLayout。
            depthStencilAttachment.initialLayout = accessInfo.layout;
            depthStencilAttachment.finalLayout   = accessInfo.attachFinalLayout;
            config.depthStencilAttachment        = depthStencilAttachment;
        }
        else
        {
            RenderPassAttachment colorAttachment;
            colorAttachment.image         = texture->getRHI()->image;
            colorAttachment.imageView     = texture->getRHI()->descriptor.imageView;
            colorAttachment.format        = texture->getRHI()->format;
            colorAttachment.samples       = texture->getRHI()->SampleCount();
            colorAttachment.slotIndex     = accessInfo.attachSlotIdx;
            colorAttachment.loadOp        = accessInfo.loadOp;
            colorAttachment.storeOp       = accessInfo.storeOp;
            colorAttachment.initialLayout = accessInfo.layout;
            colorAttachment.finalLayout   = accessInfo.attachFinalLayout;
            config.colorAttachments.emplace_back(colorAttachment);
        }
    }
    std::sort(config.colorAttachments.begin(), config.colorAttachments.end(),
              [](const RenderPassAttachment& a, const RenderPassAttachment& b) { return a.slotIndex < b.slotIndex; });
    _renderPass->init(config);
}

RenderPassBuilder::RenderPassBuilder(RDGBuilder* builder, RenderPassNodeRef node) : _builder(builder), _node(node), _dag(builder->getDag()) {}

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
    accessInfo.queueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
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
    accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    accessInfo.isAttachment                 = false;
    accessInfo.layout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    accessInfo.accessMask                   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
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
    accessInfo.descriptorType    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
    accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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
    bufferAccess.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferAccess.stageMask        = stage;
    bufferAccess.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    bufferAccess.binding          = binding;
    bufferAccess.queueFamilyIndex = queueFamilyIndex;
    bufferAccess.offset           = offset;
    bufferAccess.size             = size;
    _node->_bufferStates[buffer]  = {bufferAccess, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

RenderPassBuilder& RenderPassBuilder::execute(std::function<void(PassNode* passNode, RenderContext& context)> func)
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
    accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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
    accessInfo.set            = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    accessInfo.binding        = binding;
    accessInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
    accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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
    bufferAccess.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferAccess.queueFamilyIndex = queueFamilyIndex;
    bufferAccess.offset           = offset;
    bufferAccess.size             = size;
    _node->_bufferStates[buffer]  = {bufferAccess, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

ComputePassBuilder& ComputePassBuilder::execute(std::function<void(PassNode* passNode, RenderContext& context)> func)
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
    accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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
    accessInfo.descriptorType    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
    accessInfo.descriptorType               = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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
    bufferAccess.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferAccess.set              = uint32_t(DescriptorEnum::ePerPassDescriptorSet);
    bufferAccess.binding          = binding;
    bufferAccess.queueFamilyIndex = queueFamilyIndex;
    bufferAccess.offset           = offset;
    bufferAccess.size             = size;
    _node->_bufferStates[buffer]  = {bufferAccess, {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}};
    return *this;
}

RTPassBuilder& RTPassBuilder::execute(std::function<void(PassNode* passNode, RenderContext& context)> func)
{
    _node->setFunc(func);
    return *this;
}
} // namespace Play::RDG