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
    for (const auto& attachment : config.colorAttachments)
    {
        auto& colorAttachment       = m_vkColorAttachments.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
        colorAttachment.clearValue  = attachment.clearValue;
        colorAttachment.imageView   = attachment.imageView;
        colorAttachment.imageLayout = attachment.initialLayout;
        colorAttachment.loadOp      = attachment.loadOp;
        colorAttachment.storeOp     = attachment.storeOp;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    }
    if (config.depthStencilAttachment)
    {
        m_vkDepthAttachment.clearValue  = config.depthStencilAttachment->clearValue;
        m_vkDepthAttachment.imageView   = config.depthStencilAttachment->imageView;
        m_vkDepthAttachment.imageLayout = config.depthStencilAttachment->initialLayout;
        m_vkDepthAttachment.loadOp      = config.depthStencilAttachment->loadOp;
        m_vkDepthAttachment.storeOp     = config.depthStencilAttachment->storeOp;
        m_vkDepthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
    }
    m_vkRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_vkColorAttachments.size());
    m_vkRenderingInfo.pColorAttachments    = m_vkColorAttachments.data();
    if (config.depthStencilAttachment)
    {
        m_vkRenderingInfo.pDepthAttachment = &m_vkDepthAttachment;
    }
    else
    {
        m_vkRenderingInfo.pDepthAttachment = nullptr;
    }
    _isDirty = false;
}

void DynamicRenderPass::begin(VkCommandBuffer cmd, const VkRect2D& renderArea)
{
    nvvk::BarrierContainer batchBarrier;
    for (auto& [texture, state] : m_ownerPass->getTextureStates())
    {
        auto& accessInfo = state.textureStates[0];
        if (!accessInfo.isAttachment) continue;
        batchBarrier.appendOptionalLayoutTransition(*texture->getRHI(), state.barrierInfo);
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
