#include "RDG.h"
#include <stdexcept>
#include "queue"
#include "utils.hpp"
#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include "nvvk/debug_util.hpp"
namespace Play::RDG
{
void RDGTextureCache::regist(Texture* texture) {}

RDGTexture* RDGTextureCache::request(Texture* texture)
{
    return nullptr;
}

RDGTextureBuilder& RDGTextureBuilder::Import(Texture* texture, VkAccessFlags2 accessMask,
                                             VkImageLayout layout, VkPipelineStageFlags2 stageMask,
                                             uint32_t queueFamilyIndex)
{
    this->_textureNode->setRHI(texture);
    this->_textureNode->_info._format      = texture->Format();
    this->_textureNode->_info._type        = texture->Type();
    this->_textureNode->_info._extent      = texture->Extent();
    this->_textureNode->_info._usageFlags  = texture->UsageFlags();
    this->_textureNode->_info._aspectFlags = texture->AspectFlags();
    this->_textureNode->_info._sampleCount = texture->SampleCount();
    this->_textureNode->_info._mipmapLevel = texture->MipLevel();
    this->_textureNode->_info._layerCount  = texture->LayerCount();
    InputPassNodeRef inputNode = this->_builder->createInputPass(texture->DebugName() + "_import");
    TextureEdge*     edge =
        this->_builder->getDag()->createEdge<TextureEdge>(inputNode, this->_textureNode);
    edge->accessMask       = accessMask;
    edge->layout           = layout;
    edge->stageMask        = stageMask;
    edge->queueFamilyIndex = queueFamilyIndex;
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
    _textureNode->_info._usageFlags = usageFlags;
    return *this;
}

RDGTextureBuilder& RDGTextureBuilder::AspectFlags(VkImageAspectFlags aspectFlags)
{
    _textureNode->_info._aspectFlags = aspectFlags;
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

TextureNodeRef RDGTextureBuilder::finish()
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
    _bufferNode->_info._location = isDeviceLocal
                                       ? BufferNode::BufferDesc::MemoryLocation::eDeviceLocal
                                       : BufferNode::BufferDesc::MemoryLocation::eHostVisible;
    return *this;
}

BufferNodeRef RDGBufferBuilder::finish()
{
    return _bufferNode;
}

void BlackBoard::registTexture(std::string name, TextureNodeRef texture) {}

void BlackBoard::registBuffer(std::string name, BufferNodeRef buffer) {}

void BlackBoard::registPass(std::string name, PassNode* pass) {}

TextureNodeRef BlackBoard::getTexture(std::string name)
{
    return _textureMap[name];
}

BufferNodeRef BlackBoard::getBuffer(std::string name)
{
    return _bufferMap[name];
}

PassNode* BlackBoard::getPass(std::string name)
{
    return _passMap[name];
}

RDGBuilder::RDGBuilder(PlayElement* element) {}

RDGBuilder::~RDGBuilder() {}

RenderPassBuilder RDGBuilder::createRenderPass(std::string name)
{
    RenderPassNodeRef nodeRef = _dag->addNode<RenderPassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(name, nodeRef);
    return RenderPassBuilder(this, nodeRef);
}

ComputePassBuilder RDGBuilder::createComputePass(std::string name)
{
    ComputePassNodeRef nodeRef = _dag->addNode<ComputePassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(name, nodeRef);
    return ComputePassBuilder(this, nodeRef);
}

RTPassBuilder RDGBuilder::createRTPass(std::string name)
{
    RTPassNodeRef nodeRef = _dag->addNode<RTPassNode>(std::move(name));
    _passes.push_back(nodeRef);
    _blackBoard.registPass(name, nodeRef);
    return RTPassBuilder(this, nodeRef);
}

InputPassNodeRef RDGBuilder::createInputPass(std::string name)
{
    InputPassNodeRef nodeRef = _dag->addNode<InputPassNode>(std::move(name));
    return nodeRef;
}

RDGTextureBuilder RDGBuilder::createTexture(std::string name)
{
    TextureNodeRef    node = _dag->addNode<TextureNode>(name);
    RDGTextureBuilder builder(this, node);
    _blackBoard.registTexture(name, node);
    return builder;
}

RDGBufferBuilder RDGBuilder::createBuffer(std::string name)
{
    BufferNodeRef    node = _dag->addNode<BufferNode>(name);
    RDGBufferBuilder builder(this, node);
    _blackBoard.registBuffer(name, node);
    return builder;
}

TextureNodeRef RDGBuilder::getTexture(std::string name)
{
    return _blackBoard.getTexture(name);
}

BufferNodeRef RDGBuilder::getBuffer(std::string name)
{
    return _blackBoard.getBuffer(name);
}

void RDGBuilder::execute(RenderPassNode* pass) {}

void RDGBuilder::execute(ComputePassNode* pass) {}

void RDGBuilder::execute(RTPassNode* pass) {}

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
    NVVK_CHECK(vkEndCommandBuffer(_renderContext._currCmdBuffer));
    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = _renderContext._currCmdBuffer;
    cmdInfo.deviceMask    = 0;
    VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitInfo.semaphore = _renderContext._frameData->semaphore;
    waitInfo.value     = _renderContext._frameData->timelineValue;
    switch (_renderContext._prevPassNode->type())
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
    signalInfo.semaphore = _renderContext._frameData->semaphore;
    signalInfo.value     = ++_renderContext._frameData->timelineValue;
    switch (_renderContext._prevPassNode->type())
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
    _submitInfos.push_back({submitInfo, isAsyncCompute(_renderContext._prevPassNode) ? 1 : 0});
    for (auto& [submit, queueIndex] : _submitInfos)
    {
        vkQueueSubmit2(_element->getApp()->getQueue(queueIndex).queue, 1, &submit, nullptr);
    }
    // we add the last signaled semaphore into here, so that the next frame would wait on it.
    _element->getApp()->addWaitSemaphore(signalInfo);
}

// reference passNode is AsyncCompute or not to determine the submit package. This func would
// make passNode's execute func more cleaner, without caring about the submit details.
RDGBuilder::RenderContext RDGBuilder::prepareRenderContext(PassNode* pass)
{
    RDGBuilder::RenderContext context;
    context._element            = _element;
    context._frameInFlightIndex = _element->getApp()->getFrameCycleIndex();
    context._frameData          = &_element->getFrameData(context._frameInFlightIndex);
    // if the first pass in the frame, we directly allocate a command buffer from the pool,and begin
    // it.
    [[unlikely]]
    if (!context._prevPassNode)
    {
        VkCommandPool cmdPool = isAsyncCompute(pass) ? context._frameData->computeCmdPool
                                                     : context._frameData->graphicsCmdPool;
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool        = cmdPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        NVVK_CHECK(vkAllocateCommandBuffers(_element->getApp()->getDevice(), &allocInfo,
                                            &context._currCmdBuffer));
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(context._currCmdBuffer, &beginInfo);
    }
    else
    {
        // if not, we compare the current pass and previous pass, if they are different in
        // asyncCompute state,
        //  we pack the submit info, and allocate a new command buffer for current pass.
        // if they are the same, we just continue using the current command buffer.
        bool isCurrPassAsync = isAsyncCompute(pass);
        bool isPrevPassAsync = isAsyncCompute(context._prevPassNode);
        if (isCurrPassAsync ^ isPrevPassAsync)
        {
            NVVK_CHECK(vkEndCommandBuffer(context._currCmdBuffer));
            VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
            cmdInfo.commandBuffer = context._currCmdBuffer;
            cmdInfo.deviceMask    = 0;

            VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            waitInfo.semaphore = context._frameData->semaphore;
            waitInfo.value     = context._frameData->timelineValue;
            switch (context._prevPassNode->type())
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
            signalInfo.semaphore = context._frameData->semaphore;
            signalInfo.value     = ++context._frameData->timelineValue;
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
            VkCommandPool cmdPool = isCurrPassAsync ? context._frameData->computeCmdPool
                                                    : context._frameData->graphicsCmdPool;
            VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool        = cmdPool;
            allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            NVVK_CHECK(vkAllocateCommandBuffers(_element->getApp()->getDevice(), &allocInfo,
                                                &context._currCmdBuffer));
            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(context._currCmdBuffer, &beginInfo);
        }
    }

    context._prevPassNode = pass;
    return context;
}
} // namespace Play::RDG