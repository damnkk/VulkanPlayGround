#include "RDGPasses.hpp"
#include "RDG.h"
#include "RenderPass.h"
#include <VulkanDriver.h>

namespace Play::RDG
{
PassNode::~PassNode()
{
    VkDescriptorSetLayout layout = this->_descBindings.getSetLayout();
    vkDestroyDescriptorSetLayout(vkDriver->getDevice(), layout, nullptr);
}
const uint32_t ATTACHMENT_DEPTH   = 0xFFFFFFFF;
const uint32_t ATTACHMENT_STENCIL = 0xFFFFFFFF;
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
    config.needMultiThreadRecording = _needMultiThreadRecording;
    for (auto& state : _textureStates)
    {
        RDGTexture*              texture    = state.texture;
        const TextureAccessInfo& accessInfo = state.textureStates[0];
        if (!accessInfo.isAttachment) continue;
        const VkImageMemoryBarrier2& barrierInfo = state.barrierInfo;
        // if curr attachment's barrier is invalid, that means this is a Frame-to-Frame Dependency
        // RDG::Compile()can't infer barrier in this situation with only one invocation
        bool               isFrameToFrameDependency = !isImageBarrierValid(barrierInfo);
        TextureAccessInfo* finalAccessInfo          = texture->getFinalAccessInfo();
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

        if (accessInfo.attachSlotIdx == ATTACHMENT_DEPTH)
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
            depthStencilAttachment.initialLayout           = accessInfo.layout;
            depthStencilAttachment.finalLayout             = accessInfo.attachFinalLayout;
            depthStencilAttachment.clearValue.depthStencil = {1.0f, 0};
            config.depthAttachment                         = depthStencilAttachment;
        }
        else if (accessInfo.attachSlotIdx == ATTACHMENT_STENCIL)
        {
            RenderPassAttachment stencilAttachment;
            stencilAttachment.image     = texture->getRHI()->image;
            stencilAttachment.imageView = texture->getRHI()->descriptor.imageView;
            stencilAttachment.format    = texture->getRHI()->format;
            stencilAttachment.samples   = texture->getRHI()->SampleCount();
            stencilAttachment.slotIndex = accessInfo.attachSlotIdx;
            stencilAttachment.loadOp    = accessInfo.loadOp;
            stencilAttachment.storeOp   = accessInfo.storeOp;
            // 这里是一个通用数据结构体，必须填写accessInfo记录的layout信息，因为vkRenderPass对initlayout的准度要求更高，而barrier有时可用undefineLayout。
            stencilAttachment.initialLayout           = accessInfo.layout;
            stencilAttachment.finalLayout             = accessInfo.attachFinalLayout;
            stencilAttachment.clearValue.depthStencil = {1.0f, 0};
            config.stencilAttachment                  = stencilAttachment;
        }
        else
        {
            RenderPassAttachment colorAttachment;
            colorAttachment.image            = texture->getRHI()->image;
            colorAttachment.imageView        = texture->getRHI()->descriptor.imageView;
            colorAttachment.format           = texture->getRHI()->format;
            colorAttachment.samples          = texture->getRHI()->SampleCount();
            colorAttachment.slotIndex        = accessInfo.attachSlotIdx;
            colorAttachment.loadOp           = accessInfo.loadOp;
            colorAttachment.storeOp          = accessInfo.storeOp;
            colorAttachment.initialLayout    = accessInfo.layout;
            colorAttachment.finalLayout      = accessInfo.attachFinalLayout;
            colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            config.colorAttachments.emplace_back(colorAttachment);
        }
    }
    std::sort(config.colorAttachments.begin(), config.colorAttachments.end(),
              [](const RenderPassAttachment& a, const RenderPassAttachment& b) { return a.slotIndex < b.slotIndex; });
    _renderPass->init(config);
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
    accessInfo.queueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    accessInfo.stageMask                    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    return addTextureState(texHandle, std::move(subResource));
}

RenderPassBuilder& RenderPassBuilder::depth(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initLayout,
                                            VkImageLayout finalLayout)
{
    TextureSubresourceAccessInfo subResources;
    TextureAccessInfo&           accessInfo = subResources.emplace_back();
    accessInfo.isAttachment                 = true;
    accessInfo.accessMask                   = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp                       = loadOp;
    accessInfo.storeOp                      = storeOp;
    accessInfo.attachSlotIdx                = ATTACHMENT_DEPTH;
    accessInfo.layout                       = initLayout;
    accessInfo.attachFinalLayout            = finalLayout;
    accessInfo.queueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    accessInfo.stageMask                    = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    return addTextureState(texHandle, std::move(subResources));
}

RenderPassBuilder& RenderPassBuilder::stencil(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                              VkImageLayout initLayout, VkImageLayout finalLayout)
{
    TextureSubresourceAccessInfo subResources;
    TextureAccessInfo&           accessInfo = subResources.emplace_back();
    accessInfo.isAttachment                 = true;
    accessInfo.accessMask                   = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp                       = loadOp;
    accessInfo.storeOp                      = storeOp;
    accessInfo.attachSlotIdx                = ATTACHMENT_STENCIL;
    accessInfo.layout                       = initLayout;
    accessInfo.attachFinalLayout            = finalLayout;
    accessInfo.queueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    accessInfo.stageMask                    = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    return addTextureState(texHandle, std::move(subResources));
}

ComputePassBuilder& ComputePassBuilder::async(bool isAsync)
{
    _node->setAsyncState(isAsync);
    return *this;
}
} // namespace Play::RDG
