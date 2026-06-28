#include "DescriptorManager.h"
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

DescriptorSetBindings::DescriptorSetBindings() {}
DescriptorSetBindings::DescriptorSetBindings(DescriptorEnum setSlot) : _setSlot(setSlot) {}
DescriptorSetBindings::~DescriptorSetBindings() {}

void DescriptorSetBindings::reset(DescriptorEnum setSlot)
{
    if (_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vkDriver->getDevice(), _layout, nullptr);
        _layout = VK_NULL_HANDLE;
    }

    nvvk::DescriptorBindings::clear();
    _bindingInfos.clear();
    _descInfos.clear();
    _setBindingHash = 0;
    if (setSlot != DescriptorEnum::eCount)
    {
        _setSlot = setSlot;
    }
    _setLayoutDirty      = true;
    _descInfoDirty      |= 1 << 0;
    _descriptorSetDirty  = true;
    _cachedDescriptorSet = VK_NULL_HANDLE;
}

void DescriptorSetBindings::markLayoutDirty()
{
    _setLayoutDirty      = true;
    _descInfoDirty      |= 1 << 0;
    _descriptorSetDirty  = true;
    _cachedDescriptorSet = VK_NULL_HANDLE;
}

void DescriptorSetBindings::markDescriptorInfoDirty()
{
    _descInfoDirty      |= 1 << 0;
    _descriptorSetDirty = true;
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

            bool layoutChanged = false;
            VkShaderStageFlags mergedStageFlags = _bindingInfos[i].shaderStageFlags | bindingInfo.shaderStageFlags;
            if (_bindingInfos[i].shaderStageFlags != mergedStageFlags)
            {
                _bindingInfos[i].shaderStageFlags = mergedStageFlags;
                layoutChanged = true;
            }
            if (_bindingInfos[i].descriptorCount < bindingInfo.descriptorCount)
            {
                _bindingInfos[i].descriptorCount = bindingInfo.descriptorCount;
                layoutChanged = true;
            }
            if (layoutChanged)
            {
                markLayoutDirty();
            }
            return *this;
        }
    }

    _bindingInfos.push_back(bindingInfo);
    markLayoutDirty();
    return *this;
}

DescriptorSetBindings& DescriptorSetBindings::addBinding(uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                                         VkShaderStageFlags shaderStageFlags)
{
    BindInfo bindingInfo = {bindingIdx, descriptorCount, descriptorType, shaderStageFlags};
    return addBinding(bindingInfo);
}

VkDescriptorSetLayout DescriptorSetBindings::finalizeLayout()
{
    if (!_setLayoutDirty) return _layout;
    if (_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vkDriver->getDevice(), _layout, nullptr);
        _layout = VK_NULL_HANDLE;
    }

    nvvk::DescriptorBindings::clear();
    uint32_t descriptorCount = 0;
    for (const auto& binding : _bindingInfos)
    {
        descriptorCount += binding.descriptorCount;
        nvvk::DescriptorBindings::addBinding(binding.bindingIdx, binding.descriptorType, binding.descriptorCount, binding.shaderStageFlags);
    }
    _descInfos.resize(descriptorCount);
    createDescriptorSetLayout(vkDriver->getDevice(), 0, &_layout);
    _setLayoutDirty      = false;
    _descriptorSetDirty  = true;
    _cachedDescriptorSet = VK_NULL_HANDLE;
    return _layout;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer.buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    markDescriptorInfoDirty();
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
    markDescriptorInfoDirty();
    imageInfo.image = image.descriptor;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    markDescriptorInfoDirty();
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
    markDescriptorInfoDirty();
    destBufferInfo = bufferInfo;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler)
{
    auto& imageInfo = _descInfos[descriptorOffset(bindingIdx)].image;
    if (imageInfo.imageLayout == imageLayout && imageInfo.imageView == imageView && imageInfo.sampler == sampler)
    {
        return;
    }
    markDescriptorInfoDirty();
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
    markDescriptorInfoDirty();
    destImageInfo = imageInfo;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkAccelerationStructureKHR accel)
{
    auto& accelInfo = _descInfos[descriptorOffset(bindingIdx)].accel;
    if (accelInfo == accel)
    {
        return;
    }
    markDescriptorInfoDirty();
    accelInfo = accel;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Buffer* buffers, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx) + i].buffer;
        if (bufferInfo.buffer == buffers[i].buffer)
        {
            continue;
        }
        markDescriptorInfoDirty();
        bufferInfo.buffer = buffers[i].buffer;
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
        markDescriptorInfoDirty();
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
        markDescriptorInfoDirty();
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
        markDescriptorInfoDirty();
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
        markDescriptorInfoDirty();
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

uint64_t DescriptorSetBindings::getBindingsHash()
{
    if (_descInfoDirty)
    {
        _setBindingHash = memoryHash(_descInfos);

        _descInfoDirty = 0;
    }
    return _setBindingHash;
}

uint64_t DescriptorSetBindings::getDescsetLayoutHash()
{
    return memoryHash(_bindingInfos);
}

VkDescriptorSet DescriptorSetBindings::getOrAcquireDescriptorSet(DescriptorEnum setSlot)
{
    if (setSlot != DescriptorEnum::eCount)
    {
        _setSlot = setSlot;
    }
    if (_setSlot == DescriptorEnum::eCount)
    {
        LOGE("Descriptor set slot is not specified");
        return VK_NULL_HANDLE;
    }

    finalizeLayout();
    if (!_descriptorSetDirty && _cachedDescriptorSet != VK_NULL_HANDLE)
    {
        return _cachedDescriptorSet;
    }

    _cachedDescriptorSet = vkDriver->getDescriptorSetCache()->requestDescriptorSet(this, static_cast<uint32_t>(_setSlot));
    _descriptorSetDirty  = false;
    return _cachedDescriptorSet;
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
        for (auto& poolNode : cacheNode->pools)
        {
            vkDestroyDescriptorPool(vkDriver->getDevice(), poolNode.pool, nullptr);
        }
    }
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

VkDescriptorSet DescriptorSetCache::requestDescriptorSet(DescriptorSetBindings* setManager, uint32_t setIdx)
{
    if (setManager && setIdx >= static_cast<uint32_t>(DescriptorEnum::ePerPassDescriptorSet))
    {
        setManager->finalizeLayout();
    }
    // if the set is global set(engine/perScene/perFrame), return the cached set directly
    if (setIdx < static_cast<uint32_t>(DescriptorEnum::ePerPassDescriptorSet))
    {
        switch (setIdx)
        {
            case static_cast<uint32_t>(DescriptorEnum::eGlobalDescriptorSet):
                return _globalDescriptorSet.set;
            case static_cast<uint32_t>(DescriptorEnum::eSceneDescriptorSet):
                return _sceneDescriptorSet.set;
            case static_cast<uint32_t>(DescriptorEnum::eFrameDescriptorSet):
                return _frameDescriptorSet.set;
        }
    }
    // if (setIdx >= static_cast<uint32_t>(DescriptorEnum::ePerPassDescriptorSet))
    // {
    uint64_t BindingsHash = setManager->getBindingsHash();
    uint64_t layoutHash   = setManager->getDescsetLayoutHash();
    auto     res          = _descriptorPoolMap.find(layoutHash);
    // for perpass/perobject sets, we cache them , one layout one poolArray
    if (res != _descriptorPoolMap.end())
    {
        auto& cacheNode = *(res->second);
        auto  setRes    = cacheNode.descriptorSetMap.find(BindingsHash);
        // we find the descriptor with the same descriptor info
        if (setRes != cacheNode.descriptorSetMap.end())
        {
            return setRes->second.descriptorSet;
        }
        else
        { // if same layout but different binding info, create new set from pool
            LOGD("descriptorSet layout with hash {} got a new descriptor info, new descriptor set allocated", layoutHash);
            return createDescriptorSet(cacheNode, setManager);
        }
    }
    else
    { // damn new layout, create new pool array
        LOGD("descriptorSet layout with hash {} is not founded, it's a never meeted descriptor layout", layoutHash);
        auto cacheNode                 = std::make_shared<DescriptorSetCache::CacheNode>();
        _descriptorPoolMap[layoutHash] = cacheNode;
        return createDescriptorSet(*cacheNode, setManager);
    }
    return VK_NULL_HANDLE;
}

VkDescriptorSet DescriptorSetCache::createDescriptorSet(DescriptorSetCache::CacheNode& cacheNode, DescriptorSetBindings* setManager)
{
    // without free native vulkan pool, we create new pool
    [[unlikely]]
    if (cacheNode.pools.empty() || cacheNode.pools.back().availableCount == 0)
    {
        // create new pool;
        std::vector<VkDescriptorPoolSize> poolSizes = setManager->calculatePoolSizes(CacheNode::PoolNode::maxSetPerPool);
        VkDescriptorPoolCreateInfo        poolCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCreateInfo.maxSets       = CacheNode::PoolNode::maxSetPerPool;
        poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes    = poolSizes.data();
        CacheNode::PoolNode newPoolNode;
        NVVK_CHECK(vkCreateDescriptorPool(vkDriver->getDevice(), &poolCreateInfo, nullptr, &newPoolNode.pool));
        cacheNode.pools.push_back(newPoolNode);
        CacheNode::CachedSet newSet = createDescriptorSetImplement(cacheNode, setManager);
        return newSet.descriptorSet;
    }
    else
    {
        CacheNode::CachedSet newSet = createDescriptorSetImplement(cacheNode, setManager);
        return newSet.descriptorSet;
    }
}

DescriptorSetCache::CacheNode::CachedSet DescriptorSetCache::createDescriptorSetImplement(CacheNode& cacheNode, DescriptorSetBindings* setManager)
{
    auto                        newSet = cacheNode.createCachedSet();
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = cacheNode.pools.back().pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &setManager->getSetLayout();

    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->getDevice(), &allocInfo, &newSet.descriptorSet));
    uint64_t BindingsHash                    = setManager->getBindingsHash();
    cacheNode.descriptorSetMap[BindingsHash] = newSet;
    // currently new descriptor set allocated success, decrease the available count
    cacheNode.pools.back().availableCount--;

    auto                                                 nativeBindings = setManager->getBindings();
    std::vector<VkWriteDescriptorSet>                    writeSets;
    std::vector<std::vector<VkDescriptorImageInfo>>      imageInfosArray;
    std::vector<std::vector<VkDescriptorBufferInfo>>     bufferInfosArray;
    std::vector<std::vector<VkAccelerationStructureKHR>> accelInfosArray;
    for (auto& binding : nativeBindings)
    {
        // binding info is general, easy to fill
        VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.dstSet          = newSet.descriptorSet;
        writeSet.dstBinding      = binding.binding;
        writeSet.descriptorCount = binding.descriptorCount;
        writeSet.descriptorType  = binding.descriptorType;
        writeSet.dstArrayElement = 0;
        auto descriptorInfo      = setManager->getDescriptorInfos();
        switch (binding.descriptorType)
        {
            // if the resource is image type, we give image info
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
                uint32_t                            offset     = setManager->descriptorOffset(binding.binding);
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
                uint32_t                             offset      = setManager->descriptorOffset(binding.binding);
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
                uint32_t                                offset = setManager->descriptorOffset(binding.binding);
                std::vector<VkAccelerationStructureKHR> accelInfos(binding.descriptorCount);
                for (int i = offset; i < binding.descriptorCount; ++i)
                {
                    accelInfos[i] = descriptorInfo[i].accel;
                }
                VkWriteDescriptorSetAccelerationStructureKHR accelInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
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
    return newSet;
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
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(1);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = 1;
    poolInfo.flags                              = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->getDevice(), &poolInfo, nullptr, &_sceneDescriptorPool));
    setBindings.createDescriptorSetLayout(vkDriver->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                          &_sceneDescriptorSet.layout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _sceneDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_sceneDescriptorSet.layout;
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->getDevice(), &allocInfo, &_sceneDescriptorSet.set));
}

} // namespace Play