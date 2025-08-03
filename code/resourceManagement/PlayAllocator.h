#include "nvvk/memallocator_vma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

namespace Play{
    class PlayAllocator: public nvvk::ResourceAllocatorVma
    {
    public:
        PlayAllocator() = default;
        ~PlayAllocator(){}

        PlayAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize stagingBlockSize = NVVK_DEFAULT_STAGING_BLOCKSIZE)
        :            nvvk::ResourceAllocatorVma(instance, device, physicalDevice, stagingBlockSize)
        {
        }
        nvvk::MemHandle AllocateMemory(const nvvk::MemAllocateInfo& allocateInfo) 
        {
            return nvvk::ResourceAllocatorVma::AllocateMemory(allocateInfo);
        }
        void CreateBufferEx(const VkBufferCreateInfo& info_, VkBuffer* buffer)
        {
            nvvk::ResourceAllocatorVma::CreateBufferEx(info_, buffer);
        }

        void createImageEx(const VkImageCreateInfo& info_, VkImage* image)
        {
            nvvk::ResourceAllocatorVma::CreateImageEx(info_, image);
        }

        nvvk::MemAllocator::MemInfo getMemoryInfo(nvvk::MemHandle handle) const
        {
            return this->m_memAlloc->getMemoryInfo(handle);
        }

    };

}// namespace Play