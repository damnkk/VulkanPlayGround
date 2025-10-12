#include "DescriptorManager.h"
#include "PlayProgram.h"

namespace Play
{
std::unordered_map<DescriptorEnum, size_t> DescriptorSetOffsetMap = {
    {DescriptorEnum::eGlobalDescriptorSet, GLOBAL_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eSceneDescriptorSet, PER_SCENE_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eFrameDescriptorSet, PER_FRAME_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::ePerPassDescriptorSet, PER_PASS_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eDrawObjectDescriptorSet, PER_DRAW_OBJECT_DESCRIPTOR_SET_OFFSET}};

DescriptorManager::~DescriptorManager() {}

void DescriptorManager::init(VkPhysicalDevice physicalDevice, VkDevice device)
{
    _device         = device;
    _physicalDevice = physicalDevice;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};

    VkPhysicalDeviceProperties2 deviceProperties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    deviceProperties2.pNext = &descriptorBufferProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    _descriptorBufferProperties = descriptorBufferProperties;

    _descBuffer = Buffer::Create(
        "DescBuffer",
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_2_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
        4096 * 32, (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
}

void DescriptorManager::deinit()
{
    Buffer::Destroy(_descBuffer);
}

void DescriptorManager::updateDescSetBindingOffset(DescriptorSetManager* manager)
{
    // a series of descriptor sets' binding info
    const auto& setBindingInfo = manager->getSetBindingInfo();
    // for each set
    for (int setIdx = 0; setIdx < setBindingInfo.size(); ++setIdx)
    {
        const auto& setBinding = setBindingInfo[setIdx];
        // for each binding in the set
        for (auto& binding : setBinding.getBindings())
        {
            vkGetDescriptorSetLayoutBindingOffsetEXT(
                _device, manager->getDescriptorSetLayouts()[setIdx], binding.binding,
                &_descriptorOffsetInfo[setIdx][binding.binding]);
        }
    }
}

size_t DescriptorManager::getDescriptorSize(VkDescriptorType descriptorType)
{
    if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        return _descriptorBufferProperties.uniformBufferDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    {
        return _descriptorBufferProperties.storageBufferDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    {
        return _descriptorBufferProperties.combinedImageSamplerDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
    {
        return _descriptorBufferProperties.samplerDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
    {
        return _descriptorBufferProperties.sampledImageDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
    {
        return _descriptorBufferProperties.storageImageDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
    {
        return _descriptorBufferProperties.uniformTexelBufferDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
        return _descriptorBufferProperties.storageTexelBufferDescriptorSize;
    }
    else if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
        return _descriptorBufferProperties.accelerationStructureDescriptorSize;
    }
    else
    {
        throw std::runtime_error("Unsupported descriptor type");
    }
}

void DescriptorManager::updateDescriptor(uint32_t setIdx, uint32_t bindingIdx,
                                         VkDescriptorType descriptorType, uint32_t descriptorCount,
                                         Buffer* buffers)
{
    assert(_descriptorOffsetInfo[setIdx].find(bindingIdx) !=
           _descriptorOffsetInfo[setIdx].end()); // ensure the binding exists
    uint8_t* descBufferPtr = _descBuffer->mapping +
                             DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setIdx)] +
                             _descriptorOffsetInfo[setIdx][bindingIdx];
    size_t descSize = getDescriptorSize(descriptorType);
    for (int i = 0; i < descriptorCount; ++i)
    {
        VkDescriptorAddressInfoEXT addrInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        addrInfo.address = buffers[i].address;
        addrInfo.format  = VK_FORMAT_UNDEFINED;
        addrInfo.range   = buffers[i].BufferRange();
        VkDescriptorGetInfoEXT bufferDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};
        bufferDescrptorInfo.type                = descriptorType;
        bufferDescrptorInfo.data.pUniformBuffer = &addrInfo;
        vkGetDescriptorEXT(_device, &bufferDescrptorInfo,
                           _descriptorBufferProperties.uniformBufferDescriptorSize,
                           descBufferPtr + i * descSize);
    }
}

void DescriptorManager::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx,
                                         VkDescriptorType descriptorType, uint32_t descriptorCount,
                                         nvvk::Image* images)
{
    assert(_descriptorOffsetInfo[setEnum].find(bindingIdx) !=
           _descriptorOffsetInfo[setEnum].end()); // ensure the binding exists
    uint8_t* descBufferPtr = _descBuffer->mapping +
                             DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setEnum)] +
                             _descriptorOffsetInfo[setEnum][bindingIdx];
    size_t descSize = getDescriptorSize(descriptorType);
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        VkDescriptorImageInfo  imgInfo = images[i].descriptor;
        VkDescriptorGetInfoEXT imageDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        imageDescrptorInfo.type = descriptorType;
        if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            imageDescrptorInfo.data.pCombinedImageSampler = &imgInfo;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            imageDescrptorInfo.data.pStorageImage = &imgInfo;
        }
        else if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
        {
            imageDescrptorInfo.data.pSampledImage = &imgInfo;
        }
        else
        {
            LOGE("Unsupported descriptor type for image\n");
        }
        vkGetDescriptorEXT(_device, &imageDescrptorInfo,
                           _descriptorBufferProperties.combinedImageSamplerDescriptorSize,
                           descBufferPtr + i * descSize);
    }
}

void DescriptorManager::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx,
                                         VkDescriptorType descriptorType, uint32_t descriptorCount,
                                         const VkBufferView* bufferViews)
{
    LOGW("Not implemented yet\n");
}

void DescriptorManager::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx,
                                         VkDescriptorType descriptorType, uint32_t descriptorCount,
                                         nvvk::AccelerationStructure* accels)
{
    assert(_descriptorOffsetInfo[setEnum].find(bindingIdx) !=
           _descriptorOffsetInfo[setEnum].end()); // ensure the binding exists
    uint8_t* descBufferPtr = _descBuffer->mapping +
                             DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setEnum)] +
                             _descriptorOffsetInfo[setEnum][bindingIdx];
    size_t descSize = getDescriptorSize(descriptorType);
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        VkDescriptorGetInfoEXT accelDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        accelDescrptorInfo.type                       = descriptorType;
        accelDescrptorInfo.data.accelerationStructure = accels->address;
        vkGetDescriptorEXT(_device, &accelDescrptorInfo,
                           _descriptorBufferProperties.accelerationStructureDescriptorSize,
                           descBufferPtr + i * descSize);
    }
}

void DescriptorManager::cmdBindDescriptorBuffers(VkCommandBuffer cmdBuf, PlayProgram* program)
{
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2] = {
        {VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT}};
    descriptorBufferBindingInfo[0].address = _descBuffer->address;
    descriptorBufferBindingInfo[0].usage   = VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    vkCmdBindDescriptorBuffersEXT(cmdBuf, 2, descriptorBufferBindingInfo);
    VkPipelineLayout    layout    = program->getDescriptorManager().getPipelineLayout();
    VkPipelineBindPoint bindPoint = program->getPipelineBindPoint();
    // a series of descriptor sets' binding info
    const auto& setBindingInfo = program->getDescriptorManager().getSetBindingInfo();
    // for each set
    uint32_t bufferInfoIdx = 0;
    for (int setIdx = 0; setIdx < setBindingInfo.size(); ++setIdx)
    {
        vkCmdSetDescriptorBufferOffsetsEXT(
            cmdBuf, bindPoint, layout, setIdx, 1, &bufferInfoIdx,
            &DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setIdx)]);
    }
}

} // namespace Play