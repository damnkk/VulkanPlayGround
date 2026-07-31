#ifndef ASSET_GPU_UPLOADER_H
#define ASSET_GPU_UPLOADER_H

#include "resourceManagement/vulkan/resources/Resource.h"
#include "nvvk/semaphore.hpp"
#include <VPGLoader/Texture.hpp>

namespace Play
{

struct AssetUploadTicket
{
    uint64_t value = ~0ull;

    bool isValid() const
    {
        return value != ~0ull;
    }
};

class AssetGpuUploadContext;

// Asset-specific upload builders run on the uploader worker. They keep their CPU source
// data and GPU output alive until an AssetUploadCompletion is consumed on the main thread.
class AssetGpuUploadJob
{
public:
    virtual ~AssetGpuUploadJob() = default;

    virtual bool build(AssetGpuUploadContext& context, std::string& message) = 0;
};

struct AssetUploadCompletion
{
    AssetUploadTicket                   ticket;
    std::shared_ptr<AssetGpuUploadJob> job;
    bool                                success = false;
    std::string                         message;
};

// Only AssetGpuUploadJob::build may use this context. It provides generic GPU resource
// creation and staging operations; it has no knowledge of model or scene ownership.
class AssetGpuUploadContext
{
public:
    RefPtr<Buffer> createDeviceBuffer(const std::string& name, VkBufferUsageFlags2 usage, VkDeviceSize size);
    RefPtr<Texture> createTexture2D(const std::string& name, const vpgloader::Texture& source, bool isSrgb, uint32_t mipLevels);

    VkResult uploadBuffer(Buffer& buffer, VkDeviceSize offset, VkDeviceSize size, const void* data);
    VkResult uploadImage(Texture& texture, const vpgloader::Texture& source, VkImageLayout finalLayout);
    void     generateMipmaps(const RefPtr<Texture>& texture, VkImageLayout finalLayout);

    bool hasPendingWork() const;

private:
    struct PendingMipGeneration
    {
        RefPtr<Texture> texture;
        VkExtent2D      extent      = {};
        uint32_t        mipLevels   = 1;
        VkImageLayout   finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };

    AssetGpuUploadContext(nvvk::StagingUploader& staging, const nvvk::SemaphoreState& completionState);

    void recordPendingWork(VkCommandBuffer commandBuffer);

    nvvk::StagingUploader&             _staging;
    nvvk::SemaphoreState                _completionState;
    std::vector<PendingMipGeneration>  _pendingMipGenerations;

    friend class AssetGpuUploader;
};

// A serialized background uploader for arbitrary assets. It owns its StagingUploader and
// command pool, submits directly to the graphics queue through VulkanRuntime::submitGraphics,
// and waits on its own timeline semaphore without blocking the main thread.
class AssetGpuUploader
{
public:
    AssetGpuUploader();
    ~AssetGpuUploader();

    AssetGpuUploader(const AssetGpuUploader&)            = delete;
    AssetGpuUploader& operator=(const AssetGpuUploader&) = delete;

    bool initialize();
    void deinitialize();

    AssetUploadTicket enqueue(const std::shared_ptr<AssetGpuUploadJob>& job);
    bool              popCompleted(AssetUploadCompletion& completion);

private:
    struct State;

    static void executeUpload(const std::shared_ptr<State>& state, uint64_t generation, AssetUploadTicket ticket,
                              const std::shared_ptr<AssetGpuUploadJob>& job);

    std::shared_ptr<State> _state;
};

} // namespace Play

#endif // ASSET_GPU_UPLOADER_H
