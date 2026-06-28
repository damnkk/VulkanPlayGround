#include "PlayAllocator.h"
#include "Resource.h"
#include "core/runtime/VulkanRuntime.h"
#include "utils.hpp"
#include "nvvk/check_error.hpp"
#include "stb_image.h"
namespace Play
{

PlayResourceManager& PlayResourceManager::Instance()
{
    static PlayResourceManager manager;
    return manager;
}

nvvk::ResourceAllocatorExport* PlayResourceManager::GetAsAllocator()
{
    return &Instance();
}

nvvk::StagingUploader* PlayResourceManager::GetAsStagingUploader()
{
    return &Instance();
}

nvvk::SamplerPool* PlayResourceManager::GetAsSamplerPool()
{
    return &Instance();
}

void PlayResourceManager::initialize()
{
    VmaAllocatorCreateInfo allocatorInfo = {
        .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = vkDriver->getPhysicalDevice(),
        .device         = vkDriver->getDevice(),
        .instance       = vkDriver->getInstance(),
    };
    ::nvvk::ResourceAllocator::init(allocatorInfo);
    ::nvvk::StagingUploader::init(this, true);
    ::nvvk::SamplerPool::init(allocatorInfo.device);
}

void PlayResourceManager::deInit()
{
    if (_tempCmdPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(vkDriver->getDevice(), _tempCmdPool, nullptr);
        _tempCmdPool = VK_NULL_HANDLE;
    }
    nvvk::SamplerPool::deinit();
    nvvk::StagingUploader::deinit();
    nvvk::ResourceAllocatorExport::deinit();
}

VkCommandBuffer PlayResourceManager::getTempCommandBuffer()
{
    return vkDriver->createTempCmdBuffer();
}

void PlayResourceManager::submitAndWaitTempCmdBuffer(VkCommandBuffer cmd)
{
    vkDriver->submitAndWaitTempCmdBuffer(cmd);
}
} // namespace Play
