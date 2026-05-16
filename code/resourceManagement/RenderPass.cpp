#include "RenderPass.h"
#include <volk.h>
#include <nvvk/barriers.hpp>
#include "RDG/RDGPasses.hpp"

namespace Play
{

LegacyRenderPass::~LegacyRenderPass() {}

void LegacyRenderPass::init(const RenderPassConfig& config) {}

void LegacyRenderPass::begin(VkCommandBuffer cmd, const VkRect2D& renderArea) {}

void LegacyRenderPass::end(VkCommandBuffer cmd) {}

void LegacyRenderPass::createRenderPass() {}

void LegacyRenderPass::createFramebuffer() {}

void LegacyRenderPass::destroy() {}

void DynamicRenderPass::init(const RenderPassConfig& config)
{
    if (!_isDirty) return;
    m_config = config;
    for (const auto& attachment : config.colorAttachments)
    {
        auto& colorAttachment       = m_vkColorAttachments.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
        colorAttachment.clearValue  = attachment.clearValue;
        colorAttachment.imageView   = attachment.imageView;
        colorAttachment.imageLayout = attachment.initialLayout;
        colorAttachment.loadOp      = attachment.loadOp;
        colorAttachment.storeOp     = attachment.storeOp;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        m_vkColorAttachmentFormats.push_back(attachment.format);
    }
    if (config.depthAttachment)
    {
        m_vkDepthAttachment.clearValue     = config.depthAttachment->clearValue;
        m_vkDepthAttachment.imageView      = config.depthAttachment->imageView;
        m_vkDepthAttachment.imageLayout    = config.depthAttachment->initialLayout;
        m_vkDepthAttachment.loadOp         = config.depthAttachment->loadOp;
        m_vkDepthAttachment.storeOp        = config.depthAttachment->storeOp;
        m_vkDepthAttachment.resolveMode    = VK_RESOLVE_MODE_NONE;
        m_vkDepthAttachmentFormat          = config.depthAttachment->format;
        m_vkRenderingInfo.pDepthAttachment = &m_vkDepthAttachment;
    }
    if (config.stencilAttachment)
    {
        m_vkStencilAttachment.clearValue     = config.stencilAttachment->clearValue;
        m_vkStencilAttachment.imageView      = config.stencilAttachment->imageView;
        m_vkStencilAttachment.imageLayout    = config.stencilAttachment->initialLayout;
        m_vkStencilAttachment.loadOp         = config.stencilAttachment->loadOp;
        m_vkStencilAttachment.storeOp        = config.stencilAttachment->storeOp;
        m_vkStencilAttachment.resolveMode    = VK_RESOLVE_MODE_NONE;
        m_vkStencilAttachmentFormat          = config.stencilAttachment->format;
        m_vkRenderingInfo.pStencilAttachment = &m_vkStencilAttachment;
    }
    m_vkRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_vkColorAttachments.size());
    m_vkRenderingInfo.pColorAttachments    = m_vkColorAttachments.data();

    m_vkRenderingInfo.flags = config.needMultiThreadRecording ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT : 0;
    _isDirty                = false;
}

void DynamicRenderPass::begin(VkCommandBuffer cmd, const VkRect2D& renderArea)
{
    nvvk::BarrierContainer batchBarrier;
    for (auto& state : m_ownerPass->getTextureStates())
    {
        auto& accessInfo = state.textureStates[0];
        if (!accessInfo.isAttachment) continue;
        Texture* texture = state.texture->getRHI();
        if (!texture) continue;

        state.barrierInfo.oldLayout = texture->Layout();
        batchBarrier.appendOptionalLayoutTransition(*texture, state.barrierInfo);

        if (accessInfo.attachSlotIdx != ~0U && accessInfo.attachSlotIdx < m_vkColorAttachments.size())
        {
            m_vkColorAttachments[accessInfo.attachSlotIdx].imageView   = texture->descriptor.imageView;
            m_vkColorAttachments[accessInfo.attachSlotIdx].imageLayout = accessInfo.layout;
        }
        else if (texture->isDepth())
        {
            m_vkDepthAttachment.imageView   = texture->descriptor.imageView;
            m_vkDepthAttachment.imageLayout = accessInfo.layout;
        }
        else
        {
            m_vkStencilAttachment.imageView   = texture->descriptor.imageView;
            m_vkStencilAttachment.imageLayout = accessInfo.layout;
        }
    }
    batchBarrier.cmdPipelineBarrier(cmd, 0);
    m_vkRenderingInfo.renderArea = renderArea;
    m_vkRenderingInfo.layerCount = 1;

    vkCmdBeginRendering(cmd, &m_vkRenderingInfo);
}

void DynamicRenderPass::end(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
}

void DynamicRenderPass::transitionLayouts(VkCommandBuffer cmd, bool isBegin) {}
} // namespace Play
