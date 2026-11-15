#include "RDGPasses.hpp"
#include "RDG.h"
#include "RDGBarrierUtils.h"
#include "resourceManagement/vulkan/pipeline/RenderPass.h"
#include "core/runtime/VulkanRuntime.h"

namespace Play::RDG
{
PassNode::~PassNode()             = default;
const uint32_t ATTACHMENT_DEPTH   = 0xFFFFFFFF;
const uint32_t ATTACHMENT_STENCIL = 0xFFFFFFFF;
RenderPassNode::RenderPassNode(std::string name) : PassNode(std::move(name)) {}
RenderPassNode::~RenderPassNode() = default;
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
        const TextureAccessInfo& accessInfo = state.use;
        if (!accessInfo.isAttachment) continue;
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
    TextureAccessInfo accessInfo;
    accessInfo.isAttachment      = true;
    accessInfo.accessMask        = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp            = loadOp;
    accessInfo.storeOp           = storeOp;
    accessInfo.attachSlotIdx     = slotIdx;
    accessInfo.layout            = initLayout;
    accessInfo.attachFinalLayout = finalLayout;
    accessInfo.queueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    accessInfo.stageMask         = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    return addTextureState(texHandle, accessInfo);
}

RenderPassBuilder& RenderPassBuilder::depth(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout initLayout,
                                            VkImageLayout finalLayout)
{
    TextureAccessInfo accessInfo;
    accessInfo.isAttachment      = true;
    accessInfo.accessMask        = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp            = loadOp;
    accessInfo.storeOp           = storeOp;
    accessInfo.attachSlotIdx     = ATTACHMENT_DEPTH;
    accessInfo.layout            = initLayout;
    accessInfo.attachFinalLayout = finalLayout;
    accessInfo.queueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    accessInfo.stageMask         = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    return addTextureState(texHandle, accessInfo);
}

RenderPassBuilder& RenderPassBuilder::stencil(RDGTextureRef texHandle, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                                              VkImageLayout initLayout, VkImageLayout finalLayout)
{
    TextureAccessInfo accessInfo;
    accessInfo.isAttachment      = true;
    accessInfo.accessMask        = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    accessInfo.loadOp            = loadOp;
    accessInfo.storeOp           = storeOp;
    accessInfo.attachSlotIdx     = ATTACHMENT_STENCIL;
    accessInfo.layout            = initLayout;
    accessInfo.attachFinalLayout = finalLayout;
    accessInfo.queueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    accessInfo.stageMask         = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    return addTextureState(texHandle, accessInfo);
}

} // namespace Play::RDG
