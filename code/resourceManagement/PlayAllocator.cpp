#include "PlayAllocator.h"
#include "Resource.h"
#include "VulkanDriver.h"
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
        .physicalDevice = vkDriver->_physicalDevice,
        .device         = vkDriver->_device,
        .instance       = vkDriver->_instance,
    };
    ::nvvk::ResourceAllocator::init(allocatorInfo);
    ::nvvk::StagingUploader::init(this);
    ::nvvk::SamplerPool::init(allocatorInfo.device);
    this->m_enableLayoutBarriers = true;
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
    return vkDriver->_app->createTempCmdBuffer();
}

void PlayResourceManager::submitAndWaitTempCmdBuffer(VkCommandBuffer cmd)
{
    vkDriver->_app->submitAndWaitTempCmdBuffer(cmd);
}
} // namespace Play
