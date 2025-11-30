#ifndef PLAY_RENDERPASS_H
#define PLAY_RENDERPASS_H

#include <vulkan/vulkan_core.h>
#include <vector>
#include <optional>

namespace Play
{
namespace RDG
{
class RenderPassNode;
};

// Configuration for a single attachment
struct RenderPassAttachment
{
    VkImage               image{VK_NULL_HANDLE};     // Used for Barrier
    VkImageView           imageView{VK_NULL_HANDLE}; // Used for Rendering
    VkFormat              format{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    uint32_t              slotIndex{0};

    VkAttachmentLoadOp  loadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE};
    VkAttachmentStoreOp storeOp{VK_ATTACHMENT_STORE_OP_DONT_CARE};
    VkAttachmentLoadOp  stencilLoadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE};
    VkAttachmentStoreOp stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE};

    VkClearValue clearValue{};

    // Layout Management
    VkImageLayout initialLayout{VK_IMAGE_LAYOUT_UNDEFINED}; // State before Begin (Used for Barrier src in Dynamic mode)
    VkImageLayout finalLayout{VK_IMAGE_LAYOUT_UNDEFINED};   // State after End (Used for Barrier dst in Dynamic mode)
};

// Overall RenderPass configuration
struct RenderPassConfig
{
    std::vector<RenderPassAttachment>   colorAttachments;
    std::optional<RenderPassAttachment> depthStencilAttachment;
    // Future extensions for multiview, layers, etc.
};

class RenderPass
{
public:
    RenderPass()          = default;
    virtual ~RenderPass() = default;

    // Initialization interface: pass in config, internally check if dirty and rebuild resources
    virtual void init(const RenderPassConfig& config) = 0;

    // Begin rendering
    virtual void begin(VkCommandBuffer cmd, const VkRect2D& renderArea) = 0;

    // End rendering
    virtual void end(VkCommandBuffer cmd) = 0;

    // Get native object interface (Mainly for Legacy mode)
    virtual VkRenderPass getNativeHandle() const
    {
        return VK_NULL_HANDLE;
    }
    virtual VkFramebuffer getFramebuffer() const
    {
        return VK_NULL_HANDLE;
    }

    bool isDirty() const
    {
        return _isDirty;
    }

protected:
    RenderPassConfig m_config;
    bool             _isDirty = true;
};

// Legacy RenderPass implementation (VkRenderPass + VkFramebuffer)
class LegacyRenderPass : public RenderPass
{
public:
    LegacyRenderPass(RDG::RenderPassNode* ownerPass) : m_ownerPass(ownerPass) {}
    virtual ~LegacyRenderPass();

    void init(const RenderPassConfig& config) override;
    void begin(VkCommandBuffer cmd, const VkRect2D& renderArea) override;
    void end(VkCommandBuffer cmd) override;

    VkRenderPass getNativeHandle() const override
    {
        return m_renderPass;
    }
    VkFramebuffer getFramebuffer() const override
    {
        return m_framebuffer;
    }

private:
    void createRenderPass();
    void createFramebuffer();
    void destroy();

    VkRenderPass         m_renderPass{VK_NULL_HANDLE};
    VkFramebuffer        m_framebuffer{VK_NULL_HANDLE};
    RDG::RenderPassNode* m_ownerPass{nullptr};
};

// Dynamic Rendering implementation (VK_KHR_dynamic_rendering)
// Internally holds VkRenderingInfo and handles Layout Barrier
class DynamicRenderPass : public RenderPass
{
public:
    DynamicRenderPass(RDG::RenderPassNode* ownerPass) : m_ownerPass(ownerPass) {}
    virtual ~DynamicRenderPass() = default;
    void init(const RenderPassConfig& config) override;
    void begin(VkCommandBuffer cmd, const VkRect2D& renderArea) override;
    void end(VkCommandBuffer cmd) override;

private:
    // Internal helper: handle Layout Transition
    // isBegin = true: initialLayout -> layout
    // isBegin = false: layout -> finalLayout
    void transitionLayouts(VkCommandBuffer cmd, bool isBegin);

    // Cache VkRenderingInfo related structures to avoid per-frame construction
    std::vector<VkRenderingAttachmentInfo> m_vkColorAttachments;
    VkRenderingAttachmentInfo              m_vkDepthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingInfo                        m_vkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    RDG::RenderPassNode*                   m_ownerPass{nullptr};
};

} // namespace Play

#endif // PLAY_RENDERPASS_H
