#include "DescriptorManager.h"
#include "core/Hash.h"
#include "nvvk/check_error.hpp"
#include "core/runtime/VulkanRuntime.h"

namespace Play
{
std::unordered_map<DescriptorEnum, size_t> DescriptorSetOffsetMap = {
    {DescriptorEnum::eGlobalDescriptorSet, GLOBAL_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eSceneDescriptorSet, PER_SCENE_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eFrameDescriptorSet, PER_FRAME_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::ePerPassDescriptorSet, PER_PASS_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eDrawObjectDescriptorSet, PER_DRAW_OBJECT_DESCRIPTOR_SET_OFFSET}};

void DescriptorSetBindings::reset()
{
    nvvk::DescriptorBindings::clear();
    _bindingInfos.clear();
    _descInfos.clear();
    _bindingsDirty = true;
}

DescriptorSetBindings& DescriptorSetBindings::addBinding(const BindInfo& bindingInfo)
{
    for (int i = 0; i < _bindingInfos.size(); i++)
    {
        if (_bindingInfos[i].bindingIdx == bindingInfo.bindingIdx)
        {
            if (_bindingInfos[i].descriptorType != bindingInfo.descriptorType)
            {
                LOGE("Descriptor type mismatch");
                return *this;
            }

            bool               layoutChanged    = false;
            VkShaderStageFlags mergedStageFlags = _bindingInfos[i].shaderStageFlags | bindingInfo.shaderStageFlags;
            if (_bindingInfos[i].shaderStageFlags != mergedStageFlags)
            {
                _bindingInfos[i].shaderStageFlags = mergedStageFlags;
                layoutChanged                     = true;
            }
            if (_bindingInfos[i].descriptorCount < bindingInfo.descriptorCount)
            {
                _bindingInfos[i].descriptorCount = bindingInfo.descriptorCount;
                layoutChanged                    = true;
            }
            if (layoutChanged)
            {
                _bindingsDirty = true;
            }
            return *this;
        }
    }

    _bindingInfos.push_back(bindingInfo);
    _bindingsDirty = true;
    return *this;
}

DescriptorSetBindings& DescriptorSetBindings::addBinding(uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                                         VkShaderStageFlags shaderStageFlags)
{
    BindInfo bindingInfo = {bindingIdx, descriptorCount, descriptorType, shaderStageFlags};
    return addBinding(bindingInfo);
}

void DescriptorSetBindings::finalizeBindings()
{
    if (!_bindingsDirty) return;
    std::sort(_bindingInfos.begin(), _bindingInfos.end(), [](const BindInfo& a, const BindInfo& b) { return a.bindingIdx < b.bindingIdx; });
    nvvk::DescriptorBindings::clear();
    uint32_t descriptorCount = 0;
    for (const auto& binding : _bindingInfos)
    {
        descriptorCount += binding.descriptorCount;
        nvvk::DescriptorBindings::addBinding(binding.bindingIdx, binding.descriptorType, binding.descriptorCount, binding.shaderStageFlags);
    }
    // A changed layout changes descriptor offsets; callers populate resources after finalizing it.
    _descInfos.assign(descriptorCount, DescriptorInfo{});
    _bindingsDirty = false;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer.buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    bufferInfo.buffer = buffer.buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Image& image)
{
    auto& imageInfo = _descInfos[descriptorOffset(bindingIdx)];
    if (imageInfo.image.imageLayout == image.descriptor.imageLayout && imageInfo.image.imageView == image.descriptor.imageView &&
        imageInfo.image.sampler == image.descriptor.sampler)
    {
        return;
    }
    imageInfo.image = image.descriptor;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo& bufferInfo)
{
    auto& destBufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (destBufferInfo.buffer == bufferInfo.buffer && destBufferInfo.offset == bufferInfo.offset && destBufferInfo.range == bufferInfo.range)
    {
        return;
    }
    destBufferInfo = bufferInfo;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler)
{
    auto& imageInfo = _descInfos[descriptorOffset(bindingIdx)].image;
    if (imageInfo.imageLayout == imageLayout && imageInfo.imageView == imageView && imageInfo.sampler == sampler)
    {
        return;
    }
    imageInfo.imageLayout = imageLayout;
    imageInfo.imageView   = imageView;
    imageInfo.sampler     = sampler;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo& imageInfo)
{
    auto& destImageInfo = _descInfos[descriptorOffset(bindingIdx)].image;
    if (destImageInfo.imageLayout == imageInfo.imageLayout && destImageInfo.imageView == imageInfo.imageView &&
        destImageInfo.sampler == imageInfo.sampler)
    {
        return;
    }
    destImageInfo = imageInfo;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkAccelerationStructureKHR accel)
{
    auto& accelInfo = _descInfos[descriptorOffset(bindingIdx)].accel;
    if (accelInfo == accel)
    {
        return;
    }
    accelInfo = accel;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Buffer* buffers, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx) + i].buffer;
        if (bufferInfo.buffer == buffers[i].buffer && bufferInfo.offset == 0 && bufferInfo.range == VK_WHOLE_SIZE)
        {
            continue;
        }
        bufferInfo.buffer = buffers[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range  = VK_WHOLE_SIZE;
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Image* images, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& imageInfo = _descInfos[descriptorOffset(bindingIdx) + i].image;
        if (imageInfo.imageLayout == images[i].descriptor.imageLayout && imageInfo.imageView == images[i].descriptor.imageView &&
            imageInfo.sampler == images[i].descriptor.sampler)
        {
            continue;
        }
        imageInfo = images[i].descriptor;
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo* bufferInfos, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& destBufferInfo = _descInfos[descriptorOffset(bindingIdx) + i].buffer;
        if (destBufferInfo.buffer == bufferInfos[i].buffer && destBufferInfo.offset == bufferInfos[i].offset &&
            destBufferInfo.range == bufferInfos[i].range)
        {
            continue;
        }
        destBufferInfo = bufferInfos[i];
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo* imageInfos, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& imageInfo = _descInfos[descriptorOffset(bindingIdx) + i].image;
        if (imageInfo.imageLayout == imageInfos[i].imageLayout && imageInfo.imageView == imageInfos[i].imageView &&
            imageInfo.sampler == imageInfos[i].sampler)
        {
            continue;
        }
        imageInfo = imageInfos[i];
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkAccelerationStructureKHR* accels, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& accelInfo = _descInfos[descriptorOffset(bindingIdx) + i].accel;
        if (accelInfo == accels[i])
        {
            continue;
        }
        accelInfo = accels[i];
    }
}

int DescriptorSetBindings::descriptorOffset(uint32_t bindingIdx)
{
    int  offset     = 0;
    bool gotBinding = false;
    // bindInfo with same setIdx and bindingIdx would be merge to one bindingInfo
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& bindInfo)
                  {
                      if (bindInfo.bindingIdx == bindingIdx) gotBinding = true;
                      if (bindInfo.bindingIdx < bindingIdx)
                      {
                          offset += bindInfo.descriptorCount;
                      }
                  });
    assert(gotBinding);
    return gotBinding ? offset : -1; // 或者其他适当的错误值
}

std::vector<uint64_t> DescriptorSetBindings::getDescriptorKey() const
{
    std::vector<uint64_t> key;
    uint32_t              offset = 0;
    for (const auto& binding : _bindingInfos)
    {
        for (uint32_t i = 0; i < binding.descriptorCount; ++i)
        {
            const auto& info = _descInfos[offset++];
            switch (binding.descriptorType)
            {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    key.push_back((uint64_t) info.buffer.buffer);
                    key.push_back(info.buffer.offset);
                    key.push_back(info.buffer.range);
                    break;
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    key.push_back((uint64_t) info.accel);
                    break;
                default:
                    // Ignore fields that are not part of this descriptor type.
                    key.push_back(binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ? 0 : (uint64_t) info.image.imageView);
                    key.push_back(binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ? 0 : (uint64_t) info.image.imageLayout);
                    key.push_back(binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                                          binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                      ? (uint64_t) info.image.sampler
                                      : 0);
                    break;
            }
        }
    }
    return key;
}

DescriptorBufferManagerExt::~DescriptorBufferManagerExt() {}

void DescriptorBufferManagerExt::init(VkPhysicalDevice physicalDevice, VkDevice device)
{
    _device         = device;
    _physicalDevice = physicalDevice;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};

    VkPhysicalDeviceProperties2 deviceProperties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    deviceProperties2.pNext = &descriptorBufferProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    _descriptorBufferProperties = descriptorBufferProperties;

    _descBuffer = RefPtr<Buffer>(new Buffer("DescBuffer",
                                            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                                                VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
                                            4096 * 32, (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
}

void DescriptorBufferManagerExt::deinit() {}

// void DescriptorBufferManagerExt::updateDescSetBindingOffset(DescriptorSetManager* manager)
// {
//     // a series of descriptor sets' binding info
//     const auto& setBindingInfo = manager->getSetBindingInfo();
//     // for each set
//     for (int setIdx = 0; setIdx < setBindingInfo.size(); ++setIdx)
//     {
//         const auto& setBinding = setBindingInfo[setIdx];
//         // for each binding in the set
//         for (auto& binding : setBinding.getBindings())
//         {
//             vkGetDescriptorSetLayoutBindingOffsetEXT(_device, manager->getDescriptorSetLayouts()[setIdx], binding.binding,
//                                                      &_descriptorOffsetInfo[setIdx][binding.binding]);
//         }
//     }
// }

// void DescriptorBufferManagerExt::updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
//                                                   Buffer* buffers)
// {
//     assert(_descriptorOffsetInfo[setIdx].find(bindingIdx) != _descriptorOffsetInfo[setIdx].end()); // ensure the binding exists
//     uint8_t* descBufferPtr =
//         _descBuffer->mapping + DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setIdx)] + _descriptorOffsetInfo[setIdx][bindingIdx];
//     size_t descSize = getDescriptorSize(descriptorType);
//     for (int i = 0; i < descriptorCount; ++i)
//     {
//         VkDescriptorAddressInfoEXT addrInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
//         addrInfo.address = buffers[i].address;
//         addrInfo.format  = VK_FORMAT_UNDEFINED;
//         addrInfo.range   = buffers[i].BufferRange();
//         VkDescriptorGetInfoEXT bufferDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};
//         bufferDescrptorInfo.type                = descriptorType;
//         bufferDescrptorInfo.data.pUniformBuffer = &addrInfo;
//         vkGetDescriptorEXT(_device, &bufferDescrptorInfo, _descriptorBufferProperties.uniformBufferDescriptorSize, descBufferPtr + i * descSize);
//     }
// }

// void DescriptorBufferManagerExt::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
//                                                   nvvk::Image* images)
// {
//     assert(_descriptorOffsetInfo[setEnum].find(bindingIdx) == _descriptorOffsetInfo[setEnum].end()); // ensure the binding exists
//     uint8_t* descBufferPtr =
//         _descBuffer->mapping + DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setEnum)] + _descriptorOffsetInfo[setEnum][bindingIdx];
//     size_t descSize = getDescriptorSize(descriptorType);
//     for (uint32_t i = 0; i < descriptorCount; ++i)
//     {
//         VkDescriptorImageInfo  imgInfo = images[i].descriptor;
//         VkDescriptorGetInfoEXT imageDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
//         imageDescrptorInfo.type = descriptorType;
//         if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
//         {
//             imageDescrptorInfo.data.pCombinedImageSampler = &imgInfo;
//         }
//         else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
//         {
//             imageDescrptorInfo.data.pStorageImage = &imgInfo;
//         }
//         else if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
//         {
//             imageDescrptorInfo.data.pSampledImage = &imgInfo;
//         }
//         else
//         {
//             LOGE("Unsupported descriptor type for image\n");
//         }
//         vkGetDescriptorEXT(_device, &imageDescrptorInfo, _descriptorBufferProperties.combinedImageSamplerDescriptorSize,
//                            descBufferPtr + i * descSize);
//     }
// }

// void DescriptorBufferManagerExt::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
//                                                   const VkBufferView* bufferViews)
// {
//     LOGW("Not implemented yet\n");
// }

// void DescriptorBufferManagerExt::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
//                                                   nvvk::AccelerationStructure* accels)
// {
//     assert(_descriptorOffsetInfo[setEnum].find(bindingIdx) != _descriptorOffsetInfo[setEnum].end()); // ensure the binding exists
//     uint8_t* descBufferPtr =
//         _descBuffer->mapping + DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setEnum)] + _descriptorOffsetInfo[setEnum][bindingIdx];
//     size_t descSize = getDescriptorSize(descriptorType);
//     for (uint32_t i = 0; i < descriptorCount; ++i)
//     {
//         VkDescriptorGetInfoEXT accelDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
//         accelDescrptorInfo.type                       = descriptorType;
//         accelDescrptorInfo.data.accelerationStructure = accels->address;
//         vkGetDescriptorEXT(_device, &accelDescrptorInfo, _descriptorBufferProperties.accelerationStructureDescriptorSize,
//                            descBufferPtr + i * descSize);
//     }
// }

size_t DescriptorBufferManagerExt::getDescriptorSize(VkDescriptorType descriptorType)
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

DescriptorSetCache::~DescriptorSetCache()
{
    for (auto& [layoutHash, cacheNode] : _descriptorPoolMap)
    {
        for (auto& poolNode : cacheNode.pools)
        {
            vkDestroyDescriptorPool(vkDriver->getDevice(), poolNode.pool, nullptr);
        }
    }
    for (auto& [hash, entries] : _descriptorLayoutMap)
        for (auto& entry : entries) vkDestroyDescriptorSetLayout(vkDriver->getDevice(), entry.layout, nullptr);
    vkDestroyDescriptorPool(vkDriver->getDevice(), _globalDescriptorPool, nullptr);
    vkDestroyDescriptorPool(vkDriver->getDevice(), _sceneDescriptorPool, nullptr);
    vkDestroyDescriptorPool(vkDriver->getDevice(), _frameDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vkDriver->getDevice(), _globalDescriptorSet.layout, nullptr);
    _globalDescriptorSet.layout = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(vkDriver->getDevice(), _sceneDescriptorSet.layout, nullptr);
    _sceneDescriptorSet.layout = VK_NULL_HANDLE;
    vkDestroyDescriptorSetLayout(vkDriver->getDevice(), _frameDescriptorSet.layout, nullptr);
    _frameDescriptorSet.layout = VK_NULL_HANDLE;
}

VkDescriptorSetLayout DescriptorSetCache::getOrCreateDescriptorSetLayout(DescriptorSetBindings& bindings)
{
    auto& entries = _descriptorLayoutMap[memoryHash(bindings._bindingInfos)];
    for (const auto& entry : entries)
        if (entry.bindings == bindings._bindingInfos) return entry.layout;
    LayoutEntry entry{bindings._bindingInfos};
    NVVK_CHECK(bindings.createDescriptorSetLayout(vkDriver->getDevice(), 0, &entry.layout));
    entries.push_back(entry);
    return entry.layout;
}

CommonDescriptorSet DescriptorSetCache::getOrCreateDescriptorSet(DescriptorSetBindings& bindings)
{
    if (bindings._bindingInfos.empty()) return {};
    bindings.finalizeBindings();
    const auto layout    = getOrCreateDescriptorSetLayout(bindings);
    auto       key       = bindings.getDescriptorKey();
    auto&      cacheNode = _descriptorPoolMap[layout];
    auto&      entries   = cacheNode.descriptorSetMap[memoryHash(key)];
    for (const auto& cached : entries)
        if (cached.descriptorKey == key) return {cached.descriptorSet, layout};

    const VkDescriptorSet set = createDescriptorSet(cacheNode, layout, bindings);
    entries.push_back({std::move(key), set});
    return {set, layout};
}

VkDescriptorSet DescriptorSetCache::createDescriptorSet(CacheNode& cacheNode, VkDescriptorSetLayout layout, DescriptorSetBindings& bindings)
{
    // without free native vulkan pool, we create new pool
    [[unlikely]]
    if (cacheNode.pools.empty() || cacheNode.pools.back().availableCount == 0)
    {
        // create new pool;
        std::vector<VkDescriptorPoolSize> poolSizes = bindings.calculatePoolSizes(CacheNode::PoolNode::maxSetPerPool);
        VkDescriptorPoolCreateInfo        poolCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCreateInfo.maxSets       = CacheNode::PoolNode::maxSetPerPool;
        poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes    = poolSizes.data();
        CacheNode::PoolNode newPoolNode;
        NVVK_CHECK(vkCreateDescriptorPool(vkDriver->getDevice(), &poolCreateInfo, nullptr, &newPoolNode.pool));
        cacheNode.pools.push_back(newPoolNode);
    }
    VkDescriptorSet             set = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = cacheNode.pools.back().pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;

    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->getDevice(), &allocInfo, &set));
    cacheNode.pools.back().availableCount--;

    auto                                                      nativeBindings = bindings.getBindings();
    std::vector<VkWriteDescriptorSet>                         writeSets;
    std::vector<std::vector<VkDescriptorImageInfo>>           imageInfosArray;
    std::vector<std::vector<VkDescriptorBufferInfo>>          bufferInfosArray;
    std::vector<std::vector<VkAccelerationStructureKHR>>      accelInfosArray;
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelWrites;
    accelWrites.reserve(nativeBindings.size());
    for (auto& binding : nativeBindings)
    {
        // binding info is general, easy to fill
        VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.dstSet            = set;
        writeSet.dstBinding        = binding.binding;
        writeSet.descriptorCount   = binding.descriptorCount;
        writeSet.descriptorType    = binding.descriptorType;
        writeSet.dstArrayElement   = 0;
        const auto& descriptorInfo = bindings.getDescriptorInfos();
        switch (binding.descriptorType)
        {
            // if the resource is image type, we give image info
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
                uint32_t                            offset     = bindings.descriptorOffset(binding.binding);
                std::vector<VkDescriptorImageInfo>& imageInfos = imageInfosArray.emplace_back(binding.descriptorCount);

                for (int i = 0; i < binding.descriptorCount; ++i)
                {
                    imageInfos[i] = descriptorInfo[i + offset].image;
                    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    {
                        imageInfos[i].sampler = VK_NULL_HANDLE;
                    }
                }
                writeSet.pImageInfo = imageInfos.data();
                break;
            }
            // if the resource is buffer type, we give buffer info
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                uint32_t                             offset      = bindings.descriptorOffset(binding.binding);
                std::vector<VkDescriptorBufferInfo>& bufferInfos = bufferInfosArray.emplace_back(binding.descriptorCount);
                for (int i = 0; i < binding.descriptorCount; ++i)
                {
                    bufferInfos[i] = descriptorInfo[i + offset].buffer;
                }

                writeSet.pBufferInfo = bufferInfos.data();
                break;
            }

            // if the resource is acceleration structure type, we give accel info
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            {
                uint32_t offset     = bindings.descriptorOffset(binding.binding);
                auto&    accelInfos = accelInfosArray.emplace_back(binding.descriptorCount);
                for (uint32_t i = 0; i < binding.descriptorCount; ++i)
                {
                    accelInfos[i] = descriptorInfo[offset + i].accel;
                }
                auto& accelInfo                      = accelWrites.emplace_back();
                accelInfo.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                accelInfo.accelerationStructureCount = binding.descriptorCount;
                accelInfo.pAccelerationStructures    = accelInfos.data();
                writeSet.pNext                       = &accelInfo;
                break;
            }
            default:
                LOGE("Unsupported descriptor type in descriptor set cache\n");
                break;
        }
        writeSets.push_back(writeSet);
    }
    // Update the descriptor set with the new binding information
    vkUpdateDescriptorSets(vkDriver->getDevice(), static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
    return set;
}

void DescriptorSetCache::initGlobalDescriptorSets(nvvk::DescriptorBindings& setBindings)
{
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(1);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = 1;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->getDevice(), &poolInfo, nullptr, &_globalDescriptorPool));
    setBindings.createDescriptorSetLayout(vkDriver->getDevice(), 0, &_globalDescriptorSet.layout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _globalDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_globalDescriptorSet.layout;
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->getDevice(), &allocInfo, &_globalDescriptorSet.set));
}
void DescriptorSetCache::initFrameDescriptorSets(nvvk::DescriptorBindings& setBindings)
{
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(1);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = 1;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->getDevice(), &poolInfo, nullptr, &_frameDescriptorPool));
    setBindings.createDescriptorSetLayout(vkDriver->getDevice(), 0, &_frameDescriptorSet.layout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _frameDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_frameDescriptorSet.layout;
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->getDevice(), &allocInfo, &_frameDescriptorSet.set));
}
void DescriptorSetCache::initSceneDescriptorSets(nvvk::DescriptorBindings& setBindings)
{
    // Frame-count changes happen after the runtime's device-idle swapchain rebuild.
    if (_sceneDescriptorPool) vkDestroyDescriptorPool(vkDriver->getDevice(), _sceneDescriptorPool, nullptr);
    const uint32_t                    frameCount = vkDriver->getFrameCycleSize();
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(frameCount);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = frameCount;
    poolInfo.flags                              = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->getDevice(), &poolInfo, nullptr, &_sceneDescriptorPool));
    if (_sceneDescriptorSet.layout == VK_NULL_HANDLE)
        setBindings.createDescriptorSetLayout(vkDriver->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                              &_sceneDescriptorSet.layout);
    std::vector<VkDescriptorSetLayout> layouts(frameCount, _sceneDescriptorSet.layout);
    _sceneDescriptorSets.resize(frameCount);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _sceneDescriptorPool;
    allocInfo.descriptorSetCount = frameCount;
    allocInfo.pSetLayouts        = layouts.data();
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->getDevice(), &allocInfo, _sceneDescriptorSets.data()));
}

CommonDescriptorSet DescriptorSetCache::getSceneDescriptorSet()
{
    return {_sceneDescriptorSets.at(vkDriver->getFrameCycleIndex()), _sceneDescriptorSet.layout};
}

} // namespace Play
