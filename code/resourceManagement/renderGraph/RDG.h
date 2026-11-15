#ifndef RDG_H
#define RDG_H
#include "string"
#include "unordered_set"
#include "functional"
#include "array"
#include "list"
#include "core/runtime/RenderSession.h"
#include "core/runtime/VulkanRuntime.h"
#include "RDGResources.h"
#include "RDGPasses.hpp"
#include "resourceManagement/vulkan/pipeline/PipelineCacheManager.h"
#include "resourceManagement/vulkan/pipeline/PipelineStateBuilder.h"
namespace nvvk
{
class BarrierContainer;
}

namespace Play
{
class RenderPass;
class GraphicsPipelineStateInitializer;
class ComputePipelineStateInitializer;
} // namespace Play
namespace Play::RDG
{

class RDGBuilder;
class RDGTextureBuilder
{
public:
    RDGTextureBuilder(RDGBuilder* builder, RDGTextureRef node) : _builder(builder), _textureNode(node) {}
    RDGTextureBuilder& Import(Texture* texture);
    RDGTextureBuilder& Format(VkFormat format);
    RDGTextureBuilder& Type(VkImageType type);
    RDGTextureBuilder& Extent(VkExtent3D extent);
    RDGTextureBuilder& UsageFlags(VkImageUsageFlags usageFlags);
    RDGTextureBuilder& AspectFlags(VkImageAspectFlags aspectFlags);
    RDGTextureBuilder& SampleCount(VkSampleCountFlagBits sampleCount);
    RDGTextureBuilder& MipmapLevel(uint32_t mipmapLevel);
    RDGTextureBuilder& LayerCount(uint32_t layerCount);
    RDGTextureRef      finish();

private:
    RDGBuilder*   _builder     = nullptr;
    RDGTextureRef _textureNode = nullptr;
};
class RDGBufferBuilder
{
public:
    RDGBufferBuilder(RDGBuilder* builder, RDGBufferRef node) : _builder(builder), _bufferNode(node) {}
    RDGBufferBuilder& Size(VkDeviceSize size);
    RDGBufferBuilder& Import(Buffer* buffer);
    RDGBufferBuilder& Range(VkDeviceSize range);
    RDGBufferBuilder& UsageFlags(VkBufferUsageFlags usageFlags);
    RDGBufferBuilder& Location(bool isDeviceLocal);
    RDGBufferRef      finish();

private:
    RDGBuilder*  _builder    = nullptr;
    RDGBufferRef _bufferNode = nullptr;
};

class BlackBoard
{
public:
    BlackBoard() = default;
    ~BlackBoard();
    void          registerTexture(RDGTextureRef texture);
    void          registerBuffer(RDGBufferRef buffer);
    RDGTextureRef getTexture(const std::string& name);
    RDGBufferRef  getBuffer(const std::string& name);

private:
    std::unordered_map<std::string, RDGTextureRef> _textureMap;
    std::unordered_map<std::string, RDGBufferRef>  _bufferMap;
};

// Physical textures survive graphs; logical names do not identify pool entries.
class ResourcePool
{
public:
    ResourcePool(bool reuseTextures = true, bool logStats = false) : _reuseTextures(reuseTextures), _logStats(logStats) {}
    void clear();

private:
    friend class RDGBuilder;
    void     beginGraph();
    void     releaseTexture(Texture* texture);
    void     endGraph();
    Texture* getTexture(const RDGTexture& node);
    Buffer*  getBuffer(const RDGBuffer& node);

    struct TextureEntry
    {
        RDGTexture::TextureDesc desc;
        RefPtr<Texture>         texture;
        VkDeviceSize            bytes = 0;
        bool                    inUse = false;
    };
    struct TextureStats
    {
        uint32_t     logicalCount    = 0;
        uint32_t     createdCount    = 0;
        uint32_t     physicalCount   = 0;
        VkDeviceSize logicalBytes    = 0;
        VkDeviceSize reservedBytes   = 0;
        VkDeviceSize activeBytes     = 0;
        VkDeviceSize peakActiveBytes = 0;
    };
    std::vector<TextureEntry>                       _textures;
    std::unordered_map<std::string, RefPtr<Buffer>> _buffers;
    bool                                            _reuseTextures;
    bool                                            _logStats;
    TextureStats                                    _stats;
    TextureStats                                    _lastLoggedStats;
};

struct RenderContext;
struct PendingState
{
private:
    friend struct RenderContext;

public:
    VkDescriptorSet       _globalDescriptorSet     = VK_NULL_HANDLE;
    VkDescriptorSet       _frameDescriptorSet      = VK_NULL_HANDLE;
    VkDescriptorSet       _sceneDescriptorSet      = VK_NULL_HANDLE;
    VkDescriptorSet       _passDescriptorSet       = VK_NULL_HANDLE;
    VkDescriptorSetLayout _passDescriptorSetLayout = VK_NULL_HANDLE;
};

struct PendingGfxState : public PendingState
{
    PendingGfxState(RenderContext* renderContext) : _renderContext(renderContext) {}
    const RenderContext* _renderContext = nullptr;
    RenderPass*          _renderPass    = nullptr;
};

struct PendingComputeState : public PendingState
{
    PendingComputeState(RenderContext* renderContext) : _renderContext(renderContext) {}
    const RenderContext* _renderContext = nullptr;
};

struct PendingRTState : public PendingState
{
    PendingRTState(RenderContext* renderContext) : _renderContext(renderContext) {};
    const RenderContext* _renderContext = nullptr;
};

struct RenderContext
{
    PipelineStateBuilder createPipelineStateBuilder() const;
    void                 bindPipeline(const GraphicsPipelineStateInitializer& initializer);
    void                 bindPipeline(const ComputePipelineStateInitializer& initializer);

    template <typename T>
    void bindPushConstant(const T& pushConstant)
    {
        if (!_boundPipelineLayout || !_boundPipelineLayout->hasPushConstant)
        {
            LOGE("Push constant range is not registered");
            return;
        }

        const VkPushConstantRange& range = _boundPipelineLayout->pushConstantRange;
        if (range.size != static_cast<uint32_t>(sizeof(T)))
        {
            LOGE("Push constant type size mismatch");
            return;
        }

        vkCmdPushConstants(_currCmdBuffer, _boundPipelineLayout->vkHandle, range.stageFlags, range.offset, range.size, &pushConstant);
    }

    PlayFrameData*      _frameData              = nullptr;
    PassNode*           _prevPassNode           = nullptr;
    VkCommandBuffer     _currCmdBuffer          = VK_NULL_HANDLE;
    PipelineLayout*     _boundPipelineLayout    = nullptr;
    VkPipelineBindPoint _boundPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    PendingComputeState _pendingComputeState{this};
    PendingGfxState     _pendingGfxState{this};
    PendingRTState      _pendingRTState{this};

private:
    void bindDescriptorSets(VkPipelineBindPoint bindPoint, const PipelineLayout& layout, const PendingState& pendingState,
                            VkDescriptorSet materialSet);
};

class RDGBuilder
{
public:
    explicit RDGBuilder(ResourcePool& resources);
    ~RDGBuilder();
    RenderPassBuilder  createRenderPass(std::string name);
    ComputePassBuilder createComputePass(std::string name);
    RTPassBuilder      createRTPass(std::string name);

    RDGTextureBuilder createTexture(std::string name);
    RDGBufferBuilder  createBuffer(std::string name);

    void registerTexture(RDGTextureRef texture);
    void registerBuffer(RDGBufferRef buffer);

    RDGTextureRef getTexture(const std::string& name);
    RDGBufferRef  getBuffer(const std::string& name);
    void          compile();
    void          execute();

protected:
    friend class RDGTextureBuilder;
    friend class RDGBufferBuilder;
    void           executePass(PassNode* pass, nvvk::BarrierContainer& barriers);
    RenderContext* prepareRenderContext(PassNode* pass);
    void           prepareDescriptorSets(RenderContext& context, PassNode* pass);
    void           prepareResourceBarrier(RenderContext& context, PassNode* pass, nvvk::BarrierContainer& barriers);
    void           prepareRenderPass(PassNode* pass);
    void           endRenderPass(PassNode* pass, nvvk::BarrierContainer& barriers);
    friend class RenderPassBuilder;
    friend class ComputePassBuilder;
    friend class RTPassBuilder;
    friend class PresentPassBuilder;

private:
    ResourcePool&                           _resources;
    BlackBoard                              _blackBoard;
    std::vector<std::unique_ptr<PassNode>>  _passes;
    RenderContext                           _renderContext;
    std::vector<std::vector<RDGTextureRef>> _textureReleases;
};
} // namespace Play::RDG

#endif // RDG_H
