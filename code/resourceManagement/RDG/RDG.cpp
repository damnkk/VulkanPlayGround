#include "RDG.h"
#include <stdexcept>
#include "queue"
#include "utils.hpp"
#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/barriers.hpp>
#include "nvvk/debug_util.hpp"
namespace Play::RDG
{
void RDGTextureCache::regist(Texture* texture) {}

RDGTexture* RDGTextureCache::request(Texture* texture)
{
    return nullptr;
}

RDGTextureBuilder& RDGTextureBuilder::Import(Texture* texture)
{
    _textureNode->setRHI(texture);
    _textureNode->_info._aspectFlags = texture->AspectFlags();
    _textureNode->_info._format      = texture->Format();
    _textureNode->_info._extent      = texture->Extent();
    _textureNode->_info._type        = texture->Type();
    _textureNode->_info._usageFlags  = texture->UsageFlags();
    _textureNode->_info._sampleCount = texture->SampleCount();
    _textureNode->_info._mipmapLevel = texture->MipLevel();
    _textureNode->_info._layerCount  = texture->LayerCount();
    _textureNode->_info._debugName   = texture->DebugName();
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
    return _textureNode;
}

RDGBufferBuilder& RDGBufferBuilder::Size(VkDeviceSize size)
{
    _bufferNode->_info._size = size;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::Range(VkDeviceSize range)
{
    _bufferNode->_info._range = range;
    return *this;
}

RDGBufferBuilder& RDGBufferBuilder::UsageFlags(VkBufferUsageFlags usageFlags)
{
    _bufferNode->_info._usageFlags = usageFlags;
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
    return _bufferNode;
}

void BlackBoard::registTexture(RDGTextureRef texture)
{
    _textureMap[texture->name()] = texture;
}

void BlackBoard::registBuffer(RDGBufferRef buffer)
{
    _bufferMap[buffer->name()] = buffer;
}

void BlackBoard::registPass(PassNode* pass)
{
    _passMap[pass->name()] = pass;
}

RDGTextureRef BlackBoard::getTexture(std::string name)
{
    return _textureMap[name];
}

RDGBufferRef BlackBoard::getBuffer(std::string name)
{
    return _bufferMap[name];
}

PassNode* BlackBoard::getPass(std::string name)
{
    return _passMap[name];
}

RDGBuilder::RDGBuilder(PlayElement* element) : _element(element)
{
    _dag           = std::make_unique<Dag>();
    _renderContext = std::make_shared<RenderContext>(element);
}

RDGBuilder::~RDGBuilder() {}

RenderPassBuilder RDGBuilder::createRenderPass(std::string name)
{
    RenderPassNodeRef nodeRef = _dag->addNode<RenderPassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(nodeRef);
    return RenderPassBuilder(this, nodeRef);
}

ComputePassBuilder RDGBuilder::createComputePass(std::string name)
{
    ComputePassNodeRef nodeRef = _dag->addNode<ComputePassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(nodeRef);
    return ComputePassBuilder(this, nodeRef);
}

RTPassBuilder RDGBuilder::createRTPass(std::string name)
{
    RTPassNodeRef nodeRef = _dag->addNode<RTPassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(nodeRef);
    return RTPassBuilder(this, nodeRef);
}

PresentPassBuilder RDGBuilder::createPresentPass()
{
    PresentPassNode* nodeRef = _dag->addNode<PresentPassNode>("PresentPass");
    _passes.push_back(nodeRef);
    _blackBoard.registPass(nodeRef);
    return PresentPassBuilder(this, nodeRef);
}

// InputPassNodeRef RDGBuilder::createInputPass(std::string name)
// {
//     InputPassNodeRef nodeRef = _dag->addNode<InputPassNode>(std::move(name));
//     return nodeRef;
// }

void RDGBuilder::beforePassExecute() {}

void RDGBuilder::prepareDescriptorSets(RenderContext& context, PassNode* pass)
{
    DescriptorSetCache* descCache          = _element->getDescriptorSetCache();
    auto&               programDescManager = pass->getProgram()->getDescriptorSetManager();
    auto                bindingInfo        = pass->getProgram()->getDescriptorSetManager().getSetBindingInfo();

    for (auto& [texture, state] : pass->_textureStates)
    {
        if (state.textureStates.front().isAttachment) continue;
        assert(texture->getRHI());
        TextureAccessInfo accessInfo = state.textureStates[0];
        if (accessInfo.isAttachment) continue;
        programDescManager.setDescInfo(uint32_t(DescriptorEnum::ePerPassDescriptorSet), accessInfo.binding, *texture->_rhi);
    }

    for (auto& [buffer, state] : pass->_bufferStates)
    {
        assert(buffer->getRHI());
        BufferAccessInfo bufferInfo = state.bufferState;
        programDescManager.setDescInfo(uint32_t(DescriptorEnum::ePerPassDescriptorSet), bufferInfo.binding, *buffer->_rhi);
    }

    VkDescriptorSet currPassSet = descCache->requestDescriptorSet(programDescManager, (uint32_t) DescriptorEnum::ePerPassDescriptorSet);
    switch (pass->type())
    {
        case PassNode::Type::Render:
            context._pendingGfxState->_passDescriptorSet = currPassSet;
            break;
        case PassNode::Type::Compute:

            context._pendingComputeState->_passDescriptorSet = currPassSet;
            break;
        case PassNode::Type::RayTracing:
            context._pendingRTState->_passDescriptorSet = currPassSet;
            break;
        default:
            break;
    }
}

void RDGBuilder::prepareResourceBarrier(RenderContext& context, PassNode* pass)
{
    nvvk::BarrierContainer barrierContainer;
    for (auto& [texture, state] : pass->_textureStates)
    {
        RDGTexture::TextureDesc& info       = texture->_info;
        TextureAccessInfo        accessInfo = state.textureStates[0];
        if (!texture->getRHI())
        {
            texture->setRHI(Texture::Create(info._extent.width, info._extent.height, info._extent.depth, info._format, info._usageFlags,
                                            accessInfo.layout, info._mipmapLevel));
        }
        if (!Play::isImageBarrierValid(state.barrierInfo)) continue;
        state.barrierInfo.image = texture->getRHI()->image;
        barrierContainer.appendOptionalLayoutTransition(*texture->getRHI(), state.barrierInfo);
    }

    for (auto& [buffer, state] : pass->_bufferStates)
    {
        RDGBuffer::BufferDesc& info = buffer->_info;
        if (!buffer->getRHI())
        {
            buffer->setRHI(Buffer::Create(info._debugName, info._size, info._usageFlags,
                                          info._location == RDGBuffer::BufferDesc::MemoryLocation::eDeviceLocal
                                              ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                              : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        }
        if (!Play::isBufferBarrierValid(state.barrierInfo)) continue;
        state.barrierInfo.buffer = buffer->getRHI()->buffer;
        barrierContainer.bufferBarriers.push_back(state.barrierInfo);
    }
    barrierContainer.cmdPipelineBarrier(_renderContext->_currCmdBuffer, 0);
}

void RDGBuilder::prepareRenderPass(PassNode* pass)
{
    assert(pass->type() == PassNode::Type::Render);
    _renderContext->_pendingGfxState;
}

RDGTextureBuilder RDGBuilder::createTexture(std::string name)
{
    RDGTextureRef     node = new RDGTexture(name);
    RDGTextureBuilder builder(this, node);
    return builder;
}

RDGBufferBuilder RDGBuilder::createBuffer(std::string name)
{
    RDGBufferRef     node = new RDGBuffer(name);
    RDGBufferBuilder builder(this, node);
    return builder;
}

RDGTextureRef RDGBuilder::getTexture(std::string name)
{
    return _blackBoard.getTexture(name);
}

RDGBufferRef RDGBuilder::getBuffer(std::string name)
{
    return _blackBoard.getBuffer(name);
}

void RDGBuilder::compile()
{
    // dependency update
    for (auto passNode : _passes)
    {
        for (auto& [texture, state] : passNode->_textureStates)
        {
            auto& producerInfo = texture->_producerInfo;
            if (producerInfo.accessMask != VK_ACCESS_2_NONE)
            {
                TextureAccessInfo* lastAccessInfo = nullptr;

                if (producerInfo.lastReadOnlyAccesser == nullptr)
                {
                    // this is rdg connection, but not equivalent to a barrier relationship,
                    Edge* edge     = _dag->createEdge(producerInfo.lastProducer, passNode);
                    lastAccessInfo = &producerInfo.lastProducer->_textureStates[texture].textureStates.front();
                }
                else
                {
                    lastAccessInfo = &producerInfo.lastReadOnlyAccesser->_textureStates[texture].textureStates.front();
                }

                TextureAccessInfo& currAccessInfo = state.textureStates.front();
                if (*lastAccessInfo == currAccessInfo)
                {
                    VkImageMemoryBarrier2& imageBarrier = state.barrierInfo;
                    imageBarrier.srcAccessMask          = VK_ACCESS_2_NONE;
                    imageBarrier.dstAccessMask          = VK_ACCESS_2_NONE;
                    imageBarrier.srcStageMask           = VK_PIPELINE_STAGE_2_NONE;
                    imageBarrier.dstStageMask           = VK_PIPELINE_STAGE_2_NONE;
                    imageBarrier.oldLayout              = VK_IMAGE_LAYOUT_UNDEFINED;
                    imageBarrier.newLayout              = VK_IMAGE_LAYOUT_UNDEFINED;
                    imageBarrier.srcQueueFamilyIndex    = ~0U;
                    imageBarrier.dstQueueFamilyIndex    = ~0U;
                    imageBarrier.subresourceRange = {texture->_info._aspectFlags, 0, texture->_info._mipmapLevel, 0, texture->_info._layerCount};
                }
                else
                {
                    VkImageMemoryBarrier2& imageBarrier = state.barrierInfo;
                    imageBarrier.srcAccessMask          = lastAccessInfo->accessMask;
                    imageBarrier.dstAccessMask          = currAccessInfo.accessMask;
                    imageBarrier.srcStageMask           = lastAccessInfo->stageMask;
                    imageBarrier.dstStageMask           = currAccessInfo.stageMask;
                    imageBarrier.oldLayout              = lastAccessInfo->layout;
                    imageBarrier.newLayout              = currAccessInfo.layout;
                    imageBarrier.srcQueueFamilyIndex    = lastAccessInfo->queueFamilyIndex;
                    imageBarrier.dstQueueFamilyIndex    = currAccessInfo.queueFamilyIndex;
                    imageBarrier.subresourceRange = {texture->_info._aspectFlags, 0, texture->_info._mipmapLevel, 0, texture->_info._layerCount};
                }
            }

            if (state.textureStates.front().accessMask &
                (VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
            {
                producerInfo.lastProducer         = passNode;
                producerInfo.lastReadOnlyAccesser = nullptr;
                producerInfo.accessMask           = state.textureStates.front().accessMask;
            }
            else
            {
                producerInfo.lastReadOnlyAccesser = passNode;
                producerInfo.accessMask           = state.textureStates.front().accessMask;
            }
        }
        for (auto& [buffer, state] : passNode->_bufferStates)
        {
            auto& producerInfo = buffer->_producerInfo;
            if (producerInfo.accessMask != VK_ACCESS_2_NONE)
            {
                BufferAccessInfo* lastAccessInfo = nullptr;
                [[likely]]
                if (producerInfo.lastReadOnlyAccesser == nullptr)

                // this is rdg connection, but not equivalent to a barrier relationship,
                {
                    Edge* edge     = _dag->createEdge(producerInfo.lastProducer, passNode);
                    lastAccessInfo = &producerInfo.lastProducer->_bufferStates[buffer].bufferState;
                }
                else
                {
                    lastAccessInfo = &producerInfo.lastReadOnlyAccesser->_bufferStates[buffer].bufferState;
                }

                BufferAccessInfo& currAccessInfo = state.bufferState;
                if (*lastAccessInfo == currAccessInfo)
                {
                    VkBufferMemoryBarrier2& bufferBarrier = state.barrierInfo;
                    bufferBarrier.srcAccessMask           = VK_ACCESS_2_NONE;
                    bufferBarrier.dstAccessMask           = VK_ACCESS_2_NONE;
                    bufferBarrier.srcStageMask            = VK_PIPELINE_STAGE_2_NONE;
                    bufferBarrier.dstStageMask            = VK_PIPELINE_STAGE_2_NONE;
                    bufferBarrier.srcQueueFamilyIndex     = ~0U;
                    bufferBarrier.dstQueueFamilyIndex     = ~0U;
                    bufferBarrier.offset                  = 0;
                    bufferBarrier.size                    = 0;
                    continue;
                }
                else
                {
                    VkBufferMemoryBarrier2& bufferBarrier = state.barrierInfo;
                    bufferBarrier.srcAccessMask           = lastAccessInfo->accessMask;
                    bufferBarrier.dstAccessMask           = currAccessInfo.accessMask;
                    bufferBarrier.srcStageMask            = lastAccessInfo->stageMask;
                    bufferBarrier.dstStageMask            = currAccessInfo.stageMask;
                    bufferBarrier.srcQueueFamilyIndex     = lastAccessInfo->queueFamilyIndex;
                    bufferBarrier.dstQueueFamilyIndex     = currAccessInfo.queueFamilyIndex;
                    bufferBarrier.offset                  = state.bufferState.offset;
                    bufferBarrier.size                    = state.bufferState.size;
                }
            }

            if (state.bufferState.accessMask & (VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT))
            {
                producerInfo.lastProducer         = passNode;
                producerInfo.lastReadOnlyAccesser = nullptr;
                producerInfo.accessMask           = state.bufferState.accessMask;
            }
            else
            {
                producerInfo.lastReadOnlyAccesser = passNode;
                producerInfo.accessMask           = state.bufferState.accessMask;
            }
        }
    }
    // culling
    std::queue<PassNode*>              processQueue;
    std::unordered_map<PassNode*, int> inDegreeMap;
    for (auto passNode : _passes)
    {
        if (passNode->getIncomingEdges().empty())
        {
            processQueue.push(passNode);
        }
        inDegreeMap[passNode] = passNode->getIncomingEdges().size();
    }

    while (!processQueue.empty())
    {
        PassNode* currNode = processQueue.front();
        processQueue.pop();
        currNode->setCull(false);
        for (auto edge : currNode->getOutgoingEdges())
        {
            PassNode* toNode = static_cast<PassNode*>(edge->getTo());
            assert(inDegreeMap.find(toNode) != inDegreeMap.end());
            inDegreeMap[toNode]--;
            if (inDegreeMap[toNode] == 0)
            {
                processQueue.push(toNode);
            }
        }
    }
}

void RDGBuilder::execute()
{
    for (auto& pass : _passes)
    {
        if (pass->isCull() || !pass) continue;
        executePass(pass);
    }
}

void RDGBuilder::executePass(PassNode* pass)
{
    auto renderContext = prepareRenderContext(pass);
    prepareResourceBarrier(*renderContext, pass);
    prepareDescriptorSets(*renderContext, pass);

    if (pass->type() == PassNode::Type::Render)
    {
        prepareRenderPass(pass);
    }

    // pass->execute(*renderContext);
}

bool isAsyncCompute(PassNode* pass)
{
    if (pass->type() == PassNode::Type::Compute)
    {
        auto* computePass = static_cast<ComputePassNode*>(pass);
        return computePass->getAsyncState();
    }
    return false;
}

void RDGBuilder::afterPassExecute()
{
    // after the last pass in the frame, we need to submit the command buffer if exists.
    NVVK_CHECK(vkEndCommandBuffer(_renderContext->_currCmdBuffer));
    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = _renderContext->_currCmdBuffer;
    cmdInfo.deviceMask    = 0;
    VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitInfo.semaphore = _renderContext->_frameData->semaphore;
    waitInfo.value     = _renderContext->_frameData->timelineValue;
    switch (_renderContext->_prevPassNode->type())
    {
        case PassNode::Type::Render:
        {
            waitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        case PassNode::Type::Compute:
        {
            waitInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            break;
        }
        case PassNode::Type::RayTracing:
        {
            waitInfo.stageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            break;
        }
        default:
        {
            waitInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            break;
        }
    }
    VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signalInfo.semaphore = _renderContext->_frameData->semaphore;
    signalInfo.value     = ++_renderContext->_frameData->timelineValue;
    switch (_renderContext->_prevPassNode->type())
    {
        case PassNode::Type::Render:
        {
            signalInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        case PassNode::Type::Compute:
        {
            signalInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            break;
        }
        case PassNode::Type::RayTracing:
        {
            signalInfo.stageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            break;
        }
        default:
        {
            signalInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            break;
        }
    }
    VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submitInfo.commandBufferInfoCount   = 1;
    submitInfo.pCommandBufferInfos      = &cmdInfo;
    submitInfo.waitSemaphoreInfoCount   = 1;
    submitInfo.pWaitSemaphoreInfos      = &waitInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos    = &signalInfo;
    _submitInfos.push_back({submitInfo, isAsyncCompute(_renderContext->_prevPassNode) ? 1 : 0});
    for (auto& [submit, queueIndex] : _submitInfos)
    {
        vkQueueSubmit2(_element->getApp()->getQueue(queueIndex).queue, 1, &submit, nullptr);
    }
    // we add the last signaled semaphore into here, so that the next frame would wait on it.
    _element->getApp()->addWaitSemaphore(signalInfo);
}

// reference passNode is AsyncCompute or not to determine the submit package. This func would
// make passNode's execute func more cleaner, without caring about the submit details.
RenderContext* RDGBuilder::prepareRenderContext(PassNode* pass)
{
    switch (pass->type())
    {
        case PassNode::Type::Render:
        {
            auto& globalDescriptorSet = _renderContext->_pendingComputeState->_globalDescriptorSet;
            auto& sceneDescriptorSet  = _renderContext->_pendingComputeState->_sceneDescriptorSet;
            auto& frameDescriptorSet  = _renderContext->_pendingComputeState->_frameDescriptorSet;

            globalDescriptorSet = globalDescriptorSet == this->_element->getDescriptorSetCache()->getEngineDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getEngineDescriptorSet()
                                      : globalDescriptorSet;
            sceneDescriptorSet  = sceneDescriptorSet == this->_element->getDescriptorSetCache()->getSceneDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getSceneDescriptorSet()
                                      : sceneDescriptorSet;
            frameDescriptorSet  = frameDescriptorSet == this->_element->getDescriptorSetCache()->getFrameDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getFrameDescriptorSet()
                                      : frameDescriptorSet;

            break;
        }
        case PassNode::Type::Compute:
        {
            auto& globalDescriptorSet = _renderContext->_pendingComputeState->_globalDescriptorSet;
            auto& sceneDescriptorSet  = _renderContext->_pendingComputeState->_sceneDescriptorSet;
            auto& frameDescriptorSet  = _renderContext->_pendingComputeState->_frameDescriptorSet;

            globalDescriptorSet = globalDescriptorSet == this->_element->getDescriptorSetCache()->getEngineDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getEngineDescriptorSet()
                                      : globalDescriptorSet;
            sceneDescriptorSet  = sceneDescriptorSet == this->_element->getDescriptorSetCache()->getSceneDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getSceneDescriptorSet()
                                      : sceneDescriptorSet;
            frameDescriptorSet  = frameDescriptorSet == this->_element->getDescriptorSetCache()->getFrameDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getFrameDescriptorSet()
                                      : frameDescriptorSet;
            break;
        }
        case PassNode::Type::RayTracing:
        {
            auto& globalDescriptorSet = _renderContext->_pendingComputeState->_globalDescriptorSet;
            auto& sceneDescriptorSet  = _renderContext->_pendingComputeState->_sceneDescriptorSet;
            auto& frameDescriptorSet  = _renderContext->_pendingComputeState->_frameDescriptorSet;

            globalDescriptorSet = globalDescriptorSet == this->_element->getDescriptorSetCache()->getEngineDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getEngineDescriptorSet()
                                      : globalDescriptorSet;
            sceneDescriptorSet  = sceneDescriptorSet == this->_element->getDescriptorSetCache()->getSceneDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getSceneDescriptorSet()
                                      : sceneDescriptorSet;
            frameDescriptorSet  = frameDescriptorSet == this->_element->getDescriptorSetCache()->getFrameDescriptorSet()
                                      ? this->_element->getDescriptorSetCache()->getFrameDescriptorSet()
                                      : frameDescriptorSet;
            break;
        }
        default:
            break;
    }

    _renderContext->_frameInFlightIndex = _element->getApp()->getFrameCycleIndex();
    _renderContext->_frameData          = &_element->getFrameData(_renderContext->_frameInFlightIndex);

    // if the first pass in the frame, we directly allocate a command buffer from the pool,and
    // begin it.
    [[unlikely]]
    if (!(_renderContext->_prevPassNode))
    {
        VkCommandPool cmdPool = isAsyncCompute(pass) ? _renderContext->_frameData->computeCmdPool : _renderContext->_frameData->graphicsCmdPool;
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool        = cmdPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        NVVK_CHECK(vkAllocateCommandBuffers(_element->getApp()->getDevice(), &allocInfo, &_renderContext->_currCmdBuffer));
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(_renderContext->_currCmdBuffer, &beginInfo);
    }
    else
    {
        // if not, we compare the current pass and previous pass, if they are different in
        // asyncCompute state,
        //  we pack the submit info, and allocate a new command buffer for current pass.
        // if they are the same, we just continue using the current command buffer.
        bool isCurrPassAsync = isAsyncCompute(pass);
        bool isPrevPassAsync = isAsyncCompute(_renderContext->_prevPassNode);
        if (isCurrPassAsync ^ isPrevPassAsync)
        {
            NVVK_CHECK(vkEndCommandBuffer(_renderContext->_currCmdBuffer));
            VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
            cmdInfo.commandBuffer = _renderContext->_currCmdBuffer;
            cmdInfo.deviceMask    = 0;

            VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            waitInfo.semaphore = _renderContext->_frameData->semaphore;
            waitInfo.value     = _renderContext->_frameData->timelineValue;
            switch (_renderContext->_prevPassNode->type())
            {
                case PassNode::Type::Render:
                {
                    waitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;
                }
                case PassNode::Type::Compute:
                {
                    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                    break;
                }
                case PassNode::Type::RayTracing:
                {
                    waitInfo.stageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                    break;
                }
                default:
                {
                    waitInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                    break;
                }
            }

            VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            signalInfo.semaphore = _renderContext->_frameData->semaphore;
            signalInfo.value     = ++_renderContext->_frameData->timelineValue;
            switch (pass->type())
            {
                case PassNode::Type::Render:
                {
                    signalInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    break;
                }
                case PassNode::Type::Compute:
                {
                    signalInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                    break;
                }
                case PassNode::Type::RayTracing:
                {
                    signalInfo.stageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                    break;
                }
                default:
                {
                    signalInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                    break;
                }
            }
            VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
            submitInfo.commandBufferInfoCount   = 1;
            submitInfo.pCommandBufferInfos      = &cmdInfo;
            submitInfo.waitSemaphoreInfoCount   = 1;
            submitInfo.pWaitSemaphoreInfos      = &waitInfo;
            submitInfo.signalSemaphoreInfoCount = 1;
            submitInfo.pSignalSemaphoreInfos    = &signalInfo;

            _submitInfos.push_back({submitInfo, isPrevPassAsync ? 1 : 0});
            VkCommandPool cmdPool = isCurrPassAsync ? _renderContext->_frameData->computeCmdPool : _renderContext->_frameData->graphicsCmdPool;
            VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool        = cmdPool;
            allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            NVVK_CHECK(vkAllocateCommandBuffers(_element->getApp()->getDevice(), &allocInfo, &_renderContext->_currCmdBuffer));
            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(_renderContext->_currCmdBuffer, &beginInfo);
        }
    }

    _renderContext->_prevPassNode = pass;
    return _renderContext.get();
}
} // namespace Play::RDG