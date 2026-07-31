#include "AssetGpuUploader.h"

#include "core/JobSystem.h"
#include "core/Profiling.h"
#include "core/runtime/VulkanRuntime.h"
#include "nvvk/mipmaps.hpp"
#include "resourceManagement/vulkan/resources/PlayAllocator.h"
#include "resourceManagement/vulkan/resources/VulkanResourceUtils.h"

namespace Play
{

namespace
{

uint32_t resolveMipLevels(uint32_t requestedMipLevels, VkExtent2D extent)
{
    const uint32_t maxMipLevels = nvvk::mipLevels(extent);
    if (requestedMipLevels == 0 || requestedMipLevels > maxMipLevels)
    {
        return maxMipLevels;
    }
    return requestedMipLevels;
}

} // namespace

struct AssetGpuUploader::State
{
    nvvk::StagingUploader              staging;
    VkCommandPool                      commandPool       = VK_NULL_HANDLE;
    VkSemaphore                        timelineSemaphore = VK_NULL_HANDLE;
    uint64_t                           nextTimelineValue = 0;
    uint64_t                           nextTicketValue   = 0;
    uint64_t                           generation        = 1;
    bool                               initialized       = false;
    std::mutex                         uploadMutex;
    std::mutex                         completionMutex;
    std::vector<AssetUploadCompletion> completed;
    uint32_t                           nextCompletion = 0;

    void pushCompletion(AssetUploadCompletion&& completion)
    {
        std::lock_guard<std::mutex> lock(completionMutex);
        completed.push_back(std::move(completion));
    }
};

AssetGpuUploadContext::AssetGpuUploadContext(nvvk::StagingUploader& staging, const nvvk::SemaphoreState& completionState)
    : _staging(staging), _completionState(completionState)
{
}

RefPtr<Buffer> AssetGpuUploadContext::createDeviceBuffer(const std::string& name, VkBufferUsageFlags2 usage, VkDeviceSize size)
{
    if (size == 0)
    {
        return nullptr;
    }

    RefPtr<Buffer> buffer = RefPtr<Buffer>(new Buffer(name, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    return buffer && buffer->isValid() ? buffer : nullptr;
}

RefPtr<Texture> AssetGpuUploadContext::createTexture2D(const std::string& name, const vpgloader::Texture& source, bool isSrgb, uint32_t mipLevels)
{
    if (source.info().depth != 1)
    {
        return nullptr;
    }

    const vpgloader::TextureInfo&     sourceInfo = source.info();
    const vpgloader::TextureMipLevel& baseMip    = sourceInfo.mipLevels.front();
    const VkFormat                    format     = toVkFormat(sourceInfo.format, isSrgb);
    const VkExtent2D                  extent     = {baseMip.width, baseMip.height};
    if (format == VK_FORMAT_UNDEFINED)
    {
        return nullptr;
    }

    mipLevels = resolveMipLevels(mipLevels, extent);

    const VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {extent.width, extent.height, 1},
        .mipLevels     = mipLevels,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {inferImageAspectFlags(format, true), 0, mipLevels, 0, 1},
    };

    RefPtr<Texture> texture = RefPtr<Texture>(new Texture(name));
    if (PlayResourceManager::Instance().createImage(*texture, imageInfo, viewInfo) != VK_SUCCESS)
    {
        return nullptr;
    }

    if (PlayResourceManager::Instance().acquireSampler(texture->descriptor.sampler) != VK_SUCCESS)
    {
        return nullptr;
    }

    texture->Type()        = imageInfo.imageType;
    texture->Format()      = format;
    texture->Extent()      = imageInfo.extent;
    texture->SampleCount() = imageInfo.samples;
    texture->UsageFlags()  = imageInfo.usage;
    texture->AspectFlags() = inferImageAspectFlags(format, false);
    texture->MipLevel()    = mipLevels;
    texture->LayerCount()  = imageInfo.arrayLayers;
    texture->Layout()      = VK_IMAGE_LAYOUT_UNDEFINED;
    return texture;
}

VkResult AssetGpuUploadContext::uploadBuffer(Buffer& buffer, VkDeviceSize offset, VkDeviceSize size, const void* data)
{
    if (size == 0)
    {
        return VK_SUCCESS;
    }
    if (!data || !buffer.isValid())
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return _staging.appendBuffer(buffer, offset, size, data, _completionState);
}

VkResult AssetGpuUploadContext::uploadImage(Texture& texture, const vpgloader::Texture& source, VkImageLayout finalLayout)
{
    if (!texture.isValid() || source.info().depth != 1)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const vpgloader::TextureMipLevel& baseMip = source.info().mipLevels.front();
    return _staging.appendImage(texture, baseMip.byteSize, source.data() + baseMip.byteOffset, finalLayout, _completionState);
}

void AssetGpuUploadContext::generateMipmaps(const RefPtr<Texture>& texture, VkImageLayout finalLayout)
{
    if (!texture || texture->MipLevel() <= 1)
    {
        return;
    }

    PendingMipGeneration generation;
    generation.texture     = texture;
    generation.extent      = {texture->Extent().width, texture->Extent().height};
    generation.mipLevels   = texture->MipLevel();
    generation.finalLayout = finalLayout;
    _pendingMipGenerations.push_back(std::move(generation));
}

bool AssetGpuUploadContext::hasPendingWork() const
{
    return !_staging.isAppendedEmpty() || !_pendingMipGenerations.empty();
}

void AssetGpuUploadContext::recordPendingWork(VkCommandBuffer commandBuffer)
{
    if (!_staging.isAppendedEmpty())
    {
        _staging.cmdUploadAppended(commandBuffer);
    }

    for (const PendingMipGeneration& generation : _pendingMipGenerations)
    {
        if (!generation.texture)
        {
            continue;
        }

        nvvk::cmdGenerateMipmaps(commandBuffer, generation.texture->image, generation.extent, generation.mipLevels, 1, generation.finalLayout);
    }
}

AssetGpuUploader::AssetGpuUploader() : _state(std::make_shared<State>()) {}

AssetGpuUploader::~AssetGpuUploader()
{
    deinitialize();
}

bool AssetGpuUploader::initialize()
{
    PLAY_PROFILE_SCOPE("AssetGpuUploader::initialize");

    if (!_state || !vkDriver || vkDriver->getDevice() == VK_NULL_HANDLE)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(_state->uploadMutex);
    if (_state->initialized)
    {
        return true;
    }

    const VkCommandPoolCreateInfo commandPoolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = vkDriver->getGfxQueue().familyIndex,
    };
    if (vkCreateCommandPool(vkDriver->getDevice(), &commandPoolInfo, nullptr, &_state->commandPool) != VK_SUCCESS)
    {
        _state->commandPool = VK_NULL_HANDLE;
        return false;
    }

    if (nvvk::createTimelineSemaphore(vkDriver->getDevice(), 0, _state->timelineSemaphore) != VK_SUCCESS)
    {
        vkDestroyCommandPool(vkDriver->getDevice(), _state->commandPool, nullptr);
        _state->commandPool = VK_NULL_HANDLE;
        return false;
    }

    _state->staging.init(PlayResourceManager::GetAsAllocator(), true);
    _state->nextTimelineValue = 0;
    _state->initialized       = true;
    return true;
}

void AssetGpuUploader::deinitialize()
{
    PLAY_PROFILE_SCOPE("AssetGpuUploader::deinitialize");

    if (!_state)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(_state->uploadMutex);
    if (!_state->initialized)
    {
        return;
    }

    _state->initialized = false;
    ++_state->generation;
    if (_state->generation == 0)
    {
        _state->generation = 1;
    }

    _state->staging.deinit();

    if (_state->timelineSemaphore != VK_NULL_HANDLE && vkDriver)
    {
        vkDestroySemaphore(vkDriver->getDevice(), _state->timelineSemaphore, nullptr);
        _state->timelineSemaphore = VK_NULL_HANDLE;
    }
    if (_state->commandPool != VK_NULL_HANDLE && vkDriver)
    {
        vkDestroyCommandPool(vkDriver->getDevice(), _state->commandPool, nullptr);
        _state->commandPool = VK_NULL_HANDLE;
    }
}

AssetUploadTicket AssetGpuUploader::enqueue(const std::shared_ptr<AssetGpuUploadJob>& job)
{
    PLAY_PROFILE_SCOPE("AssetGpuUploader::enqueue");

    if (!_state || !job)
    {
        return {};
    }

    AssetUploadTicket ticket;
    uint64_t          generation = 0;
    {
        std::lock_guard<std::mutex> lock(_state->uploadMutex);
        if (!_state->initialized)
        {
            return {};
        }

        ticket.value = _state->nextTicketValue++;
        if (!ticket.isValid())
        {
            ticket.value = _state->nextTicketValue++;
        }
        generation = _state->generation;
    }

    const std::shared_ptr<State> state = _state;
    JobSystem::detach([state, generation, ticket, job]() { executeUpload(state, generation, ticket, job); });
    return ticket;
}

bool AssetGpuUploader::popCompleted(AssetUploadCompletion& completion)
{
    PLAY_PROFILE_SCOPE("AssetGpuUploader::popCompleted");

    if (!_state)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(_state->completionMutex);
    if (_state->nextCompletion >= _state->completed.size())
    {
        _state->completed.clear();
        _state->nextCompletion = 0;
        return false;
    }

    completion = std::move(_state->completed[_state->nextCompletion++]);
    return true;
}

void AssetGpuUploader::executeUpload(const std::shared_ptr<State>& state, uint64_t generation, AssetUploadTicket ticket,
                                     const std::shared_ptr<AssetGpuUploadJob>& job)
{
    AssetUploadCompletion completion;
    completion.ticket = ticket;
    completion.job    = job;

    std::lock_guard<std::mutex> lock(state->uploadMutex);
    if (!state->initialized || generation != state->generation)
    {
        completion.message = "Asset GPU uploader is not available.";
        state->pushCompletion(std::move(completion));
        return;
    }

    const uint64_t signalValue = ++state->nextTimelineValue;
    const nvvk::SemaphoreState completionState = nvvk::SemaphoreState::makeFixed(state->timelineSemaphore, signalValue);
    AssetGpuUploadContext      context(state->staging, completionState);

    if (!job->build(context, completion.message))
    {
        state->staging.cancelAppended();
        if (completion.message.empty())
        {
            completion.message = "Asset upload job setup failed.";
        }
        state->pushCompletion(std::move(completion));
        return;
    }

    if (!context.hasPendingWork())
    {
        completion.success = true;
        state->pushCompletion(std::move(completion));
        return;
    }

    const VkCommandBufferAllocateInfo allocateInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = state->commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkResult        result        = vkAllocateCommandBuffers(vkDriver->getDevice(), &allocateInfo, &commandBuffer);
    if (result == VK_SUCCESS)
    {
        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    }
    if (result == VK_SUCCESS)
    {
        context.recordPendingWork(commandBuffer);
        result = vkEndCommandBuffer(commandBuffer);
    }

    if (result == VK_SUCCESS)
    {
        const VkCommandBufferSubmitInfo commandBufferInfo{
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = commandBuffer,
        };
        const VkSemaphoreSubmitInfo signalInfo{
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = state->timelineSemaphore,
            .value     = signalValue,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        };
        const VkSubmitInfo2 submitInfo{
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &commandBufferInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalInfo,
        };
        result = vkDriver->submitGraphics(submitInfo);
    }

    if (result == VK_SUCCESS)
    {
        const VkSemaphoreWaitInfo waitInfo{
            .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores    = &state->timelineSemaphore,
            .pValues        = &signalValue,
        };
        result = vkWaitSemaphores(vkDriver->getDevice(), &waitInfo, UINT64_MAX);
    }

    if (result == VK_SUCCESS)
    {
        state->staging.releaseStaging(false);
        completion.success = true;
    }
    else
    {
        state->staging.releaseStaging(true);
        completion.message = "Asset GPU upload submission failed.";
    }

    if (commandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(vkDriver->getDevice(), state->commandPool, 1, &commandBuffer);
    }

    state->pushCompletion(std::move(completion));
}

} // namespace Play
