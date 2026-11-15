#include "RDG.h"
#include <stdexcept>
#include "queue"
#include "RDGBarrierUtils.h"
#include "resourceManagement/vulkan/cache/RenderPassCache.h"
#include "resourceManagement/vulkan/pipeline/RenderPass.h"
#include "core/runtime/VulkanRuntime.h"
#include "core/Profiling.h"
#include "resourceManagement/vulkan/descriptors/DescriptorManager.h"
#include "resourceManagement/vulkan/resources/PlayAllocator.h"
#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/barriers.hpp>
#include "nvvk/debug_util.hpp"
namespace Play::RDG
{

RDGTextureBuilder& RDGTextureBuilder::Import(Texture* texture)
{
    _textureNode->import(texture);
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
    _textureNode->_info._usageFlags |= usageFlags;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::AspectFlags(VkImageAspectFlags aspectFlags)
{
    _textureNode->_info._aspectFlags |= aspectFlags;
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

RDGTextureRef RDGTextureBuilder::finish()
{
    _builder->registerTexture(_textureNode);
    return _textureNode;
}

RDGBufferBuilder& RDGBufferBuilder::Size(VkDeviceSize size)
{
    _bufferNode->_info._size = size;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Import(Buffer* buffer)
{
    _bufferNode->import(buffer);
    _bufferNode->_info._range      = buffer->BufferRange();
    _bufferNode->_info._size       = buffer->BufferSize();
    _bufferNode->_info._usageFlags = buffer->UsageFlags();
    _bufferNode->_info._location   = buffer->BufferProperty() & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                         ? RDGBuffer::BufferDesc::MemoryLocation::eDeviceLocal
                                         : RDGBuffer::BufferDesc::MemoryLocation::eHostVisible;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Range(VkDeviceSize range)
{
    _bufferNode->_info._range = range;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::UsageFlags(VkBufferUsageFlags usageFlags)
{
    _bufferNode->_info._usageFlags |= usageFlags;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Location(bool isDeviceLocal)
{
    _bufferNode->_info._location =
        isDeviceLocal ? RDGBuffer::BufferDesc::MemoryLocation::eDeviceLocal : RDGBuffer::BufferDesc::MemoryLocation::eHostVisible;
    return *this;
}

RDGBufferRef RDGBufferBuilder::finish()
{
    _builder->registerBuffer(_bufferNode);
    return _bufferNode;
}

BlackBoard::~BlackBoard()
{
    for (auto& [name, texture] : _textureMap)
    {
        delete texture;
    }
    for (auto& [name, buffer] : _bufferMap)
    {
        delete buffer;
    }
}

void BlackBoard::registerTexture(RDGTextureRef texture)
{
    const bool inserted = _textureMap.emplace(texture->name(), texture).second;
    assert(inserted);
}

void BlackBoard::registerBuffer(RDGBufferRef buffer)
{
    const bool inserted = _bufferMap.emplace(buffer->name(), buffer).second;
    assert(inserted);
}

RDGTextureRef BlackBoard::getTexture(const std::string& name)
{
    return _textureMap.at(name);
}

RDGBufferRef BlackBoard::getBuffer(const std::string& name)
{
    return _bufferMap.at(name);
}

void ResourcePool::clear()
{
    _textures.clear();
    _buffers.clear();
    _lastLoggedStats = {};
}

void ResourcePool::beginGraph()
{
    _stats = {};
    for (const auto& entry : _textures) assert(!entry.inUse);
}

Texture* ResourcePool::getTexture(const RDGTexture& node)
{
    const auto& info    = node._info;
    const auto  matches = [&](const TextureEntry& entry)
    {
        const auto& desc = entry.desc;
        return !entry.inUse && desc._extent.width == info._extent.width && desc._extent.height == info._extent.height &&
               desc._extent.depth == info._extent.depth && desc._format == info._format && desc._type == info._type &&
               desc._usageFlags == info._usageFlags && desc._aspectFlags == info._aspectFlags && desc._mipmapLevel == info._mipmapLevel &&
               desc._layerCount == info._layerCount && desc._sampleCount == info._sampleCount;
    };
    TextureEntry* selected = nullptr;
    for (auto& entry : _textures)
    {
        if (matches(entry))
        {
            selected = &entry;
            break;
        }
    }
    if (!selected)
    {
        PLAY_PROFILE_SCOPE("RDG Allocate Texture");
        // The existing Texture constructors support single-layer 2D/3D targets.
        assert(info._layerCount == 1);
        assert(info._type == (info._extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D));
        assert(info._extent.depth == 1 || info._sampleCount == VK_SAMPLE_COUNT_1_BIT);
        TextureEntry entry;
        entry.desc = info;
        if (info._extent.depth == 1)
        {
            entry.texture = new Texture(info._extent.width, info._extent.height, info._format, info._usageFlags, VK_IMAGE_LAYOUT_UNDEFINED,
                                        info._mipmapLevel, info._sampleCount);
        }
        else
        {
            entry.texture = new Texture(info._extent.width, info._extent.height, info._extent.depth, info._format, info._usageFlags,
                                        VK_IMAGE_LAYOUT_UNDEFINED, info._mipmapLevel);
        }
        entry.texture->DebugName() = "RDGPoolTexture_" + std::to_string(_textures.size());
        nvvk::DebugUtil::getInstance().setObjectName(entry.texture->image, entry.texture->DebugName().c_str());
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(vkDriver->getDevice(), entry.texture->image, &requirements);
        entry.bytes = requirements.size;
        _textures.push_back(std::move(entry));
        selected = &_textures.back();
        ++_stats.createdCount;
    }
    selected->inUse = true;
    ++_stats.logicalCount;
    _stats.logicalBytes += selected->bytes;
    _stats.activeBytes += selected->bytes;
    _stats.peakActiveBytes = std::max(_stats.peakActiveBytes, _stats.activeBytes);
    return selected->texture.get();
}

void ResourcePool::releaseTexture(Texture* texture)
{
    // Baseline mode retains every allocation until the end of this graph.
    if (!_reuseTextures) return;
    for (auto& entry : _textures)
    {
        if (entry.texture.get() == texture)
        {
            assert(entry.inUse);
            entry.inUse = false;
            _stats.activeBytes -= entry.bytes;
            return;
        }
    }
    assert(false);
}

void ResourcePool::endGraph()
{
    for (auto& entry : _textures)
    {
        assert(!_reuseTextures || !entry.inUse);
        entry.inUse = false;
        _stats.reservedBytes += entry.bytes;
    }
    _stats.physicalCount = static_cast<uint32_t>(_textures.size());
    // Only log allocation / workload changes, not every frame.
    if (_logStats && (_stats.logicalCount != _lastLoggedStats.logicalCount || _stats.logicalBytes != _lastLoggedStats.logicalBytes ||
                      _stats.physicalCount != _lastLoggedStats.physicalCount || _stats.reservedBytes != _lastLoggedStats.reservedBytes ||
                      _stats.peakActiveBytes != _lastLoggedStats.peakActiveBytes || _stats.createdCount != _lastLoggedStats.createdCount))
    {
        constexpr double MiB = 1024.0 * 1024.0;
        LOGI("RDG texture pool [%s]: logical=%u (%.2f MiB), physical=%u (%.2f MiB reserved), peak active=%.2f MiB, created=%u\n",
             _reuseTextures ? "reuse" : "no reuse", _stats.logicalCount, _stats.logicalBytes / MiB, _stats.physicalCount, _stats.reservedBytes / MiB,
             _stats.peakActiveBytes / MiB, _stats.createdCount);
        _lastLoggedStats = _stats;
    }
}

Buffer* ResourcePool::getBuffer(const RDGBuffer& node)
{
    auto&                       buffer   = _buffers[node.name()];
    const auto&                 info     = node._info;
    const VkMemoryPropertyFlags property = info._location == RDGBuffer::BufferDesc::MemoryLocation::eDeviceLocal
                                               ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                               : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!buffer || buffer->BufferSize() != info._size || buffer->UsageFlags() != info._usageFlags || buffer->BufferProperty() != property)
    {
        PLAY_PROFILE_SCOPE("RDG Allocate Buffer");
        buffer = new Buffer(node.name(), info._usageFlags, info._size, property);
    }
    return buffer.get();
}

PipelineStateBuilder RenderContext::createPipelineStateBuilder() const
{
    assert(_prevPassNode);
    const PendingState* pendingState = nullptr;
    const RenderPass*   renderPass   = nullptr;
    switch (_prevPassNode->type())
    {
        case PassNode::Type::Render:
            pendingState = &_pendingGfxState;
            renderPass   = _pendingGfxState._renderPass;
            break;
        case PassNode::Type::Compute:
            pendingState = &_pendingComputeState;
            break;
        case PassNode::Type::RayTracing:
            pendingState = &_pendingRTState;
            break;
        default:
            break;
    }
    assert(pendingState);

    auto*              descriptorCache = vkDriver->getDescriptorSetCache();
    PipelineLayoutDesc layoutDesc;
    layoutDesc.setDescriptorSetLayout(DescriptorEnum::eGlobalDescriptorSet, descriptorCache->getEngineDescriptorSet().layout);
    layoutDesc.setDescriptorSetLayout(DescriptorEnum::eSceneDescriptorSet, descriptorCache->getSceneDescriptorSet().layout);
    layoutDesc.setDescriptorSetLayout(DescriptorEnum::eFrameDescriptorSet, descriptorCache->getFrameDescriptorSet().layout);
    if (pendingState->_passDescriptorSetLayout != VK_NULL_HANDLE)
        layoutDesc.setDescriptorSetLayout(DescriptorEnum::ePerPassDescriptorSet, pendingState->_passDescriptorSetLayout);

    return PipelineStateBuilder(*vkDriver->getPipelineCacheManager(), *descriptorCache, layoutDesc, renderPass);
}

void RenderContext::bindPipeline(const GraphicsPipelineStateInitializer& initializer)
{
    VkPipeline pipeline = vkDriver->getPipelineCacheManager()->getOrCreateGraphicsPipeline(initializer);

    vkCmdBindPipeline(_currCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    _boundPipelineLayout    = initializer.pipelineLayout;
    _boundPipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    bindDescriptorSets(_boundPipelineBindPoint, *initializer.pipelineLayout, _pendingGfxState, initializer.materialDescriptorSet.set);
}

void RenderContext::bindPipeline(const ComputePipelineStateInitializer& initializer)
{
    VkPipeline pipeline = vkDriver->getPipelineCacheManager()->getOrCreateComputePipeline(initializer);

    vkCmdBindPipeline(_currCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    _boundPipelineLayout    = initializer.pipelineLayout;
    _boundPipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    bindDescriptorSets(_boundPipelineBindPoint, *initializer.pipelineLayout, _pendingComputeState, initializer.materialDescriptorSet.set);
}

void RenderContext::bindDescriptorSets(VkPipelineBindPoint bindPoint, const PipelineLayout& layout, const PendingState& pendingState,
                                       VkDescriptorSet materialSet)
{
    const std::array<VkDescriptorSet, 3> persistentSets = {pendingState._globalDescriptorSet, pendingState._sceneDescriptorSet,
                                                           pendingState._frameDescriptorSet};
    vkCmdBindDescriptorSets(_currCmdBuffer, bindPoint, layout.vkHandle, static_cast<uint32_t>(DescriptorEnum::eGlobalDescriptorSet),
                            static_cast<uint32_t>(persistentSets.size()), persistentSets.data(), 0, nullptr);
    if (pendingState._passDescriptorSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(_currCmdBuffer, bindPoint, layout.vkHandle, static_cast<uint32_t>(DescriptorEnum::ePerPassDescriptorSet), 1,
                                &pendingState._passDescriptorSet, 0, nullptr);
    if (materialSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(_currCmdBuffer, bindPoint, layout.vkHandle, static_cast<uint32_t>(DescriptorEnum::eDrawObjectDescriptorSet), 1,
                                &materialSet, 0, nullptr);
}

RDGBuilder::RDGBuilder(ResourcePool& resources) : _resources(resources) {}

RDGBuilder::~RDGBuilder() = default;

RenderPassBuilder RDGBuilder::createRenderPass(std::string name)
{
    auto  node = std::make_unique<RenderPassNode>(std::move(name));
    auto* pass = node.get();
    _passes.push_back(std::move(node));
    return RenderPassBuilder(this, pass);
}

ComputePassBuilder RDGBuilder::createComputePass(std::string name)
{
    auto  node = std::make_unique<ComputePassNode>(std::move(name));
    auto* pass = node.get();
    _passes.push_back(std::move(node));
    return ComputePassBuilder(this, pass);
}

RTPassBuilder RDGBuilder::createRTPass(std::string name)
{
    auto  node = std::make_unique<RTPassNode>(std::move(name));
    auto* pass = node.get();
    _passes.push_back(std::move(node));
    return RTPassBuilder(this, pass);
}

void RDGBuilder::prepareDescriptorSets(RenderContext& context, PassNode* pass)
{
    auto& programDescManager = pass->_descBindings;

    for (auto& state : pass->_textureStates)
    {
        if (state.use.isAttachment) continue;
        assert(state.texture->getRHI());
        TextureAccessInfo accessInfo = state.use;
        if (accessInfo.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && state.texture->_rhi->descriptor.sampler == VK_NULL_HANDLE)
        {
            PlayResourceManager::Instance().acquireSampler(state.texture->_rhi->descriptor.sampler);
        }
        programDescManager.setDescInfo(accessInfo.binding, state.texture->_rhi->descriptor.imageView, accessInfo.layout,
                                       state.texture->_rhi->descriptor.sampler);
    }

    for (auto& state : pass->_bufferStates)
    {
        assert(state.buffer->getRHI());
        BufferAccessInfo bufferInfo = state.bufferState;
        programDescManager.setDescInfo(bufferInfo.binding, *state.buffer->_rhi, bufferInfo.offset, bufferInfo.size);
    }

    const auto                  passSet           = vkDriver->getDescriptorSetCache()->getOrCreateDescriptorSet(programDescManager);
    const VkDescriptorSetLayout currPassSetLayout = passSet.layout;
    const VkDescriptorSet       currPassSet       = passSet.set;
    switch (pass->type())
    {
        case PassNode::Type::Render:
        {
            context._pendingGfxState._passDescriptorSet       = currPassSet;
            context._pendingGfxState._passDescriptorSetLayout = currPassSetLayout;
            break;
        }
        case PassNode::Type::Compute:
        {
            context._pendingComputeState._passDescriptorSet       = currPassSet;
            context._pendingComputeState._passDescriptorSetLayout = currPassSetLayout;
            break;
        }

        case PassNode::Type::RayTracing:
        {
            context._pendingRTState._passDescriptorSet       = currPassSet;
            context._pendingRTState._passDescriptorSetLayout = currPassSetLayout;
            break;
        }
        default:
            break;
    }
}

namespace
{
constexpr VkAccessFlags2 WriteAccess = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                                       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

void transitionTexture(nvvk::BarrierContainer& barriers, Texture& texture, VkImageLayout layout, VkImageAspectFlags aspect,
                       VkPipelineStageFlags2 stages, VkAccessFlags2 access, bool discardContents = false)
{
    auto& previous = texture.graphicsAccess;
    // Match the semaphore wait stage when transitioning an acquired image.
    const auto srcStages    = texture.ownsImage() ? previous.stages : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto oldLayout    = discardContents ? VK_IMAGE_LAYOUT_UNDEFINED : texture.Layout();
    const bool changeLayout = oldLayout != layout;
    if (changeLayout || (previous.stages && ((previous.access | access) & WriteAccess)))
    {
        barriers.imageBarriers.push_back({
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask        = srcStages,
            .srcAccessMask       = previous.access,
            .dstStageMask        = stages,
            .dstAccessMask       = access,
            .oldLayout           = oldLayout,
            .newLayout           = layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = texture.image,
            .subresourceRange    = {aspect, 0, texture.MipLevel(), 0, texture.LayerCount()},
        });
    }

    texture.Layout() = layout;
    if (changeLayout || (access & WriteAccess))
    {
        // A layout transition is also a write. Keep it in scope for subsequent
        // readers even if the pass requesting the new layout is read only.
        previous = {stages, access | (changeLayout && stages ? VK_ACCESS_2_MEMORY_WRITE_BIT : 0)};
    }
    else
    {
        previous.stages |= stages;
        previous.access |= access;
    }
}
} // namespace

void RDGBuilder::prepareResourceBarrier(RenderContext& context, PassNode* pass, nvvk::BarrierContainer& barriers)
{
    barriers.clear();
    for (const auto& state : pass->_textureStates)
    {
        auto&       node            = *state.texture;
        const auto& use             = state.use;
        bool        discardContents = false;
        if (!node.getRHI())
        {
            assert(!node._external);
            node._rhi = _resources.getTexture(node);
            // Discard old logical contents, but retain the physical access scope
            // so the new owner cannot overlap earlier GPU readers / writers.
            discardContents = use.isAttachment ? use.loadOp != VK_ATTACHMENT_LOAD_OP_LOAD : use.accessMask == VK_ACCESS_2_SHADER_WRITE_BIT;
        }
        transitionTexture(barriers, *node.getRHI(), use.layout, node._info._aspectFlags, use.stageMask, use.accessMask, discardContents);
    }

    for (const auto& state : pass->_bufferStates)
    {
        auto& node = *state.buffer;
        if (!node.getRHI())
        {
            node._rhi = _resources.getBuffer(node);
        }
        auto&       buffer   = *node.getRHI();
        auto&       previous = buffer.graphicsAccess;
        const auto& use      = state.bufferState;
        if (previous.stages && ((previous.access | use.accessMask) & WriteAccess))
        {
            barriers.bufferBarriers.push_back({
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask        = previous.stages,
                .srcAccessMask       = previous.access,
                .dstStageMask        = use.stageMask,
                .dstAccessMask       = use.accessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer              = buffer.buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE,
            });
        }
        if (use.accessMask & WriteAccess) previous = {};
        previous.stages |= use.stageMask;
        previous.access |= use.accessMask;
    }
    barriers.cmdPipelineBarrier(context._currCmdBuffer, 0);
}

void RDGBuilder::prepareRenderPass(PassNode* pass)
{
    assert(pass->type() == PassNode::Type::Render);
    RenderPassNode* renderPassNode = dynamic_cast<RenderPassNode*>(pass);
    renderPassNode->initRenderPass();
    _renderContext._pendingGfxState._renderPass = renderPassNode->_renderPass.get();
    renderPassNode->_renderPass->begin(_renderContext._currCmdBuffer,
                                       {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}});
}

void RDGBuilder::endRenderPass(PassNode* pass, nvvk::BarrierContainer& barriers)
{
    assert(pass->type() == PassNode::Type::Render);
    // deal with dynamic rendering
    const auto cmd = _renderContext._currCmdBuffer;
    _renderContext._pendingGfxState._renderPass->end(cmd);

    barriers.clear();
    for (const auto& state : pass->_textureStates)
    {
        const auto& use = state.use;
        if (use.isAttachment && use.attachFinalLayout != VK_IMAGE_LAYOUT_UNDEFINED && use.attachFinalLayout != use.layout)
        {
            const bool present = use.attachFinalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            transitionTexture(barriers, *state.texture->getRHI(), use.attachFinalLayout, state.texture->_info._aspectFlags,
                              present ? VK_PIPELINE_STAGE_2_NONE : use.stageMask, present ? VK_ACCESS_2_NONE : use.accessMask);
        }
    }
    barriers.cmdPipelineBarrier(cmd, 0);
}

RDGTextureBuilder RDGBuilder::createTexture(std::string name)
{
    RDGTextureRef     node = new RDGTexture(std::move(name));
    RDGTextureBuilder builder(this, node);
    return builder;
}

RDGBufferBuilder RDGBuilder::createBuffer(std::string name)
{
    RDGBufferRef     node = new RDGBuffer(std::move(name));
    RDGBufferBuilder builder(this, node);
    return builder;
}

void RDGBuilder::registerTexture(RDGTextureRef texture)
{
    _blackBoard.registerTexture(texture);
}

void RDGBuilder::registerBuffer(RDGBufferRef buffer)
{
    _blackBoard.registerBuffer(buffer);
}

RDGTextureRef RDGBuilder::getTexture(const std::string& name)
{
    return _blackBoard.getTexture(name);
}

RDGBufferRef RDGBuilder::getBuffer(const std::string& name)
{
    return _blackBoard.getBuffer(name);
}

void RDGBuilder::compile()
{
    PLAY_PROFILE_SCOPE("RDG Compile");

    _textureReleases.clear();
    _textureReleases.resize(_passes.size());
    // Lifetimes cover every declared access, including reads far from the producer.
    for (uint32_t passIndex = 0; passIndex < _passes.size(); ++passIndex)
    {
        for (const auto& state : _passes[passIndex]->_textureStates)
        {
            auto& texture = *state.texture;
            if (texture._external) continue;
            if (texture._firstUsePass == ~0U) texture._firstUsePass = passIndex;
            texture._lastUsePass = passIndex;
        }
    }
    for (uint32_t passIndex = 0; passIndex < _passes.size(); ++passIndex)
    {
        for (const auto& state : _passes[passIndex]->_textureStates)
        {
            auto& texture = *state.texture;
            if (texture._external || texture._firstUsePass != passIndex) continue;
            auto& releases = _textureReleases[texture._lastUsePass];
            if (std::find(releases.begin(), releases.end(), &texture) == releases.end()) releases.push_back(&texture);
        }
    }

    // Passes are declared in execution order. The old DAG walked every edge
    // to validate that same order; resource accesses now drive barriers directly.
    for (auto& pass : _passes)
    {
        for (const auto& state : pass->_textureStates)
        {
            const auto& use = state.use;
            if (!use.isAttachment)
                pass->_descBindings.addBinding(use.binding, 1, use.descriptorType, inferShaderStageFromPipelineStage(use.stageMask));
        }
        for (const auto& state : pass->_bufferStates)
        {
            const auto& use = state.bufferState;
            pass->_descBindings.addBinding(use.binding, 1, use.descriptorType, inferShaderStageFromPipelineStage(use.stageMask));
        }
        pass->_descBindings.finalizeBindings();
    }
}

void RDGBuilder::execute()
{
    PLAY_PROFILE_SCOPE("RDG Execute");

    _resources.beginGraph();
    nvvk::BarrierContainer barriers;
    for (uint32_t passIndex = 0; passIndex < _passes.size(); ++passIndex)
    {
        executePass(_passes[passIndex].get(), barriers);
        for (auto* texture : _textureReleases[passIndex])
        {
            _resources.releaseTexture(texture->getRHI());
            texture->_rhi = nullptr;
        }
    }
    _resources.endGraph();
    vkDriver->submitFrameCommands(_renderContext._currCmdBuffer);
}

void RDGBuilder::executePass(PassNode* pass, nvvk::BarrierContainer& barriers)
{
    PLAY_PROFILE_SCOPE(pass->name().c_str());

    auto renderContext = prepareRenderContext(pass);
    PLAY_PROFILE_COMMAND_LABEL(renderContext->_currCmdBuffer, pass->name().c_str());
    prepareResourceBarrier(*renderContext, pass, barriers);
    prepareDescriptorSets(*renderContext, pass);

    if (pass->type() == PassNode::Type::Render)
    {
        prepareRenderPass(pass);
    }
    pass->execute(*renderContext);
    if (pass->type() == PassNode::Type::Render)
    {
        endRenderPass(pass, barriers);
    }
}

RenderContext* RDGBuilder::prepareRenderContext(PassNode* pass)
{
    switch (pass->type())
    {
        case PassNode::Type::Render:
        {
            auto& globalDescriptorSet = _renderContext._pendingGfxState._globalDescriptorSet;
            auto& sceneDescriptorSet  = _renderContext._pendingGfxState._sceneDescriptorSet;
            auto& frameDescriptorSet  = _renderContext._pendingGfxState._frameDescriptorSet;

            globalDescriptorSet = vkDriver->getDescriptorSetCache()->getEngineDescriptorSet().set;
            sceneDescriptorSet  = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;
            frameDescriptorSet  = vkDriver->getDescriptorSetCache()->getFrameDescriptorSet().set;
            break;
        }
        case PassNode::Type::Compute:
        {
            auto& globalDescriptorSet = _renderContext._pendingComputeState._globalDescriptorSet;
            auto& sceneDescriptorSet  = _renderContext._pendingComputeState._sceneDescriptorSet;
            auto& frameDescriptorSet  = _renderContext._pendingComputeState._frameDescriptorSet;

            globalDescriptorSet = vkDriver->getDescriptorSetCache()->getEngineDescriptorSet().set;
            sceneDescriptorSet  = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;
            frameDescriptorSet  = vkDriver->getDescriptorSetCache()->getFrameDescriptorSet().set;
            break;
        }
        case PassNode::Type::RayTracing:
        {
            auto& globalDescriptorSet = _renderContext._pendingRTState._globalDescriptorSet;
            auto& sceneDescriptorSet  = _renderContext._pendingRTState._sceneDescriptorSet;
            auto& frameDescriptorSet  = _renderContext._pendingRTState._frameDescriptorSet;

            globalDescriptorSet = vkDriver->getDescriptorSetCache()->getEngineDescriptorSet().set;
            sceneDescriptorSet  = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;
            frameDescriptorSet  = vkDriver->getDescriptorSetCache()->getFrameDescriptorSet().set;
            break;
        }
        default:
            break;
    }

    if (!_renderContext._prevPassNode)
    {
        _renderContext._frameData     = &vkDriver->getCurrentFrameData();
        _renderContext._currCmdBuffer = _renderContext._frameData->graphicsCmdPool.allocCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        NVVK_CHECK(vkBeginCommandBuffer(_renderContext._currCmdBuffer, &beginInfo));
    }

    _renderContext._prevPassNode = pass;
    return &_renderContext;
}
} // namespace Play::RDG
