#include "DescriptorManager.h"
#include "PlayProgram.h"
#include "nvvk/check_error.hpp"
#include "VulkanDriver.h"

namespace Play
{
std::unordered_map<DescriptorEnum, size_t> DescriptorSetOffsetMap = {
    {DescriptorEnum::eGlobalDescriptorSet, GLOBAL_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eSceneDescriptorSet, PER_SCENE_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eFrameDescriptorSet, PER_FRAME_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::ePerPassDescriptorSet, PER_PASS_DESCRIPTOR_SET_OFFSET},
    {DescriptorEnum::eDrawObjectDescriptorSet, PER_DRAW_OBJECT_DESCRIPTOR_SET_OFFSET}};

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

    _descBuffer = Buffer::Create("DescBuffer",
                                 VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                                     VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
                                 4096 * 32, (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
}

void DescriptorBufferManagerExt::deinit()
{
    Buffer::Destroy(_descBuffer);
}

void DescriptorBufferManagerExt::updateDescSetBindingOffset(DescriptorSetManager* manager)
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
            vkGetDescriptorSetLayoutBindingOffsetEXT(_device, manager->getDescriptorSetLayouts()[setIdx], binding.binding,
                                                     &_descriptorOffsetInfo[setIdx][binding.binding]);
        }
    }
}

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
            vkDestroyDescriptorPool(vkDriver->_device, poolNode.pool, nullptr);
        }
    }
    vkDestroyDescriptorPool(vkDriver->_device, _globalDescriptorPool, nullptr);
    vkDestroyDescriptorPool(vkDriver->_device, _sceneDescriptorPool, nullptr);
    vkDestroyDescriptorPool(vkDriver->_device, _frameDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vkDriver->_device, _globalDescriptorSet.layout, nullptr);
    vkDestroyDescriptorSetLayout(vkDriver->_device, _sceneDescriptorSet.layout, nullptr);
    vkDestroyDescriptorSetLayout(vkDriver->_device, _frameDescriptorSet.layout, nullptr);
}

VkDescriptorSet DescriptorSetCache::requestDescriptorSet(DescriptorSetManager& setManager, uint32_t setIdx)
{
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
    uint64_t BindingsHash = setManager.getBindingsHash(setIdx);
    uint64_t layoutHash   = setManager.getDescsetLayoutHash(setIdx);
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
            return createDescriptorSet(cacheNode, setManager, setIdx);
        }
    }
    else
    { // damn new layout, create new pool array
        LOGD("descriptorSet layout with hash {} is not founded, it's a never meeted descriptor layout", layoutHash);
        auto cacheNode                 = std::make_shared<DescriptorSetCache::CacheNode>();
        _descriptorPoolMap[layoutHash] = cacheNode;
        return createDescriptorSet(*cacheNode, setManager, setIdx);
    }
    return VK_NULL_HANDLE;
}

VkDescriptorSet DescriptorSetCache::createDescriptorSet(DescriptorSetCache::CacheNode& cacheNode, DescriptorSetManager& setManager, uint32_t setIdx)
{
    // without free native vulkan pool, we create new pool
    [[unlikely]]
    if (cacheNode.pools.empty() || cacheNode.pools.back().availableCount == 0)
    {
        // create new pool;
        auto                              bindings  = setManager.getSetBindingInfo()[setIdx];
        std::vector<VkDescriptorPoolSize> poolSizes = bindings.calculatePoolSizes(CacheNode::PoolNode::maxSetPerPool);
        VkDescriptorPoolCreateInfo        poolCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolCreateInfo.maxSets       = CacheNode::PoolNode::maxSetPerPool;
        poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes    = poolSizes.data();
        CacheNode::PoolNode newPoolNode;
        NVVK_CHECK(vkCreateDescriptorPool(vkDriver->_device, &poolCreateInfo, nullptr, &newPoolNode.pool));
        cacheNode.pools.push_back(newPoolNode);
        CacheNode::CachedSet newSet = createDescriptorSetImplement(cacheNode, setManager, setIdx);
        return newSet.descriptorSet;
    }
    else
    {
        CacheNode::CachedSet newSet = createDescriptorSetImplement(cacheNode, setManager, setIdx);
        return newSet.descriptorSet;
    }
}

DescriptorSetCache::CacheNode::CachedSet DescriptorSetCache::createDescriptorSetImplement(CacheNode& cacheNode, DescriptorSetManager& setManager,
                                                                                          uint32_t setIdx)
{
    auto                        newSet = cacheNode.createCachedSet();
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = cacheNode.pools.back().pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &setManager.getDescriptorSetLayouts()[setIdx];

    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->_device, &allocInfo, &newSet.descriptorSet));
    uint64_t BindingsHash                    = setManager.getBindingsHash(setIdx);
    cacheNode.descriptorSetMap[BindingsHash] = newSet;
    // currently new descriptor set allocated success, decrease the available count
    cacheNode.pools.back().availableCount--;
    // now prepare the write descriptor sets info
    auto bindings = setManager.getSetBindingInfo()[setIdx];

    auto                                                 nativeBindings = bindings.getBindings();
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
        auto descriptorInfo      = setManager.getDescriptorInfo(setIdx);
        switch (binding.descriptorType)
        {
            // if the resource is image type, we give image info
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
                uint32_t                            offset     = setManager.descriptorOffset(setIdx, binding.binding);
                std::vector<VkDescriptorImageInfo>& imageInfos = imageInfosArray.emplace_back(binding.descriptorCount);

                for (int i = offset; i < binding.descriptorCount; ++i)
                {
                    imageInfos[i]         = descriptorInfo[i].image;
                    imageInfos[i].sampler = VK_NULL_HANDLE;
                }
                writeSet.pImageInfo = imageInfos.data();
                break;
            }
            // if the resource is buffer type, we give buffer info
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                uint32_t                            offset = setManager.descriptorOffset(setIdx, binding.binding);
                std::vector<VkDescriptorBufferInfo> bufferInfos(binding.descriptorCount);
                for (int i = offset; i < binding.descriptorCount; ++i)
                {
                    bufferInfos[i] = descriptorInfo[i].buffer;
                }

                writeSet.pBufferInfo = bufferInfos.data();
                break;
            }
            // case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            // case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            // {
            //     uint32_t offset =
            //         setManager.descriptorOffset(setIdx, binding.binding);
            //     std::vector<VkBufferView> bufferViews(binding.descriptorCount);
            //     for (int i = offset; i < binding.descriptorCount; ++i)
            //     {
            //         bufferViews[i] = descriptorInfo[i].bufferView;
            //     }
            //     writeSet.pTexelBufferView = bufferViews.data();
            //     break;
            // }

            // if the resource is acceleration structure type, we give accel info
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            {
                uint32_t                                offset = setManager.descriptorOffset(setIdx, binding.binding);
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
    vkUpdateDescriptorSets(vkDriver->_device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
    return newSet;
}

void DescriptorSetCache::initGlobalDescriptorSets(nvvk::DescriptorBindings& setBindings)
{
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(1);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = 1;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->_device, &poolInfo, nullptr, &_globalDescriptorPool));
    setBindings.createDescriptorSetLayout(vkDriver->getDevice(), 0, &_globalDescriptorSet.layout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _globalDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_globalDescriptorSet.layout;
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->_device, &allocInfo, &_globalDescriptorSet.set));
}
void DescriptorSetCache::initFrameDescriptorSets(nvvk::DescriptorBindings& setBindings)
{
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(1);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = 1;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->_device, &poolInfo, nullptr, &_frameDescriptorPool));
    setBindings.createDescriptorSetLayout(vkDriver->getDevice(), 0, &_frameDescriptorSet.layout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _frameDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_frameDescriptorSet.layout;
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->_device, &allocInfo, &_frameDescriptorSet.set));
}
void DescriptorSetCache::initSceneDescriptorSets(nvvk::DescriptorBindings& setBindings)
{
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    std::vector<VkDescriptorPoolSize> poolSizes = setBindings.calculatePoolSizes(1);
    poolInfo.poolSizeCount                      = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                         = poolSizes.data();
    poolInfo.maxSets                            = 1;
    NVVK_CHECK(vkCreateDescriptorPool(vkDriver->_device, &poolInfo, nullptr, &_sceneDescriptorPool));
    setBindings.createDescriptorSetLayout(vkDriver->getDevice(), 0, &_sceneDescriptorSet.layout);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _sceneDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_sceneDescriptorSet.layout;
    NVVK_CHECK(vkAllocateDescriptorSets(vkDriver->_device, &allocInfo, &_sceneDescriptorSet.set));
}

void DescriptorBufferManagerExt::updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                  Buffer* buffers)
{
    assert(_descriptorOffsetInfo[setIdx].find(bindingIdx) != _descriptorOffsetInfo[setIdx].end()); // ensure the binding exists
    uint8_t* descBufferPtr =
        _descBuffer->mapping + DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setIdx)] + _descriptorOffsetInfo[setIdx][bindingIdx];
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
        vkGetDescriptorEXT(_device, &bufferDescrptorInfo, _descriptorBufferProperties.uniformBufferDescriptorSize, descBufferPtr + i * descSize);
    }
}

void DescriptorBufferManagerExt::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                  nvvk::Image* images)
{
    assert(_descriptorOffsetInfo[setEnum].find(bindingIdx) == _descriptorOffsetInfo[setEnum].end()); // ensure the binding exists
    uint8_t* descBufferPtr =
        _descBuffer->mapping + DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setEnum)] + _descriptorOffsetInfo[setEnum][bindingIdx];
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
        vkGetDescriptorEXT(_device, &imageDescrptorInfo, _descriptorBufferProperties.combinedImageSamplerDescriptorSize,
                           descBufferPtr + i * descSize);
    }
}

void DescriptorBufferManagerExt::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                  const VkBufferView* bufferViews)
{
    LOGW("Not implemented yet\n");
}

void DescriptorBufferManagerExt::updateDescriptor(uint32_t setEnum, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                  nvvk::AccelerationStructure* accels)
{
    assert(_descriptorOffsetInfo[setEnum].find(bindingIdx) != _descriptorOffsetInfo[setEnum].end()); // ensure the binding exists
    uint8_t* descBufferPtr =
        _descBuffer->mapping + DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setEnum)] + _descriptorOffsetInfo[setEnum][bindingIdx];
    size_t descSize = getDescriptorSize(descriptorType);
    for (uint32_t i = 0; i < descriptorCount; ++i)
    {
        VkDescriptorGetInfoEXT accelDescrptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        accelDescrptorInfo.type                       = descriptorType;
        accelDescrptorInfo.data.accelerationStructure = accels->address;
        vkGetDescriptorEXT(_device, &accelDescrptorInfo, _descriptorBufferProperties.accelerationStructureDescriptorSize,
                           descBufferPtr + i * descSize);
    }
}

void DescriptorBufferManagerExt::cmdBindDescriptorBuffers(VkCommandBuffer cmdBuf, PlayProgram* program)
{
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo[2] = {{VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT}};
    descriptorBufferBindingInfo[0].address                          = _descBuffer->address;
    descriptorBufferBindingInfo[0].usage                            = VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    vkCmdBindDescriptorBuffersEXT(cmdBuf, 2, descriptorBufferBindingInfo);
    VkPipelineLayout    layout    = program->getDescriptorSetManager().getPipelineLayout();
    VkPipelineBindPoint bindPoint = program->getPipelineBindPoint();
    // a series of descriptor sets' binding info
    const auto& setBindingInfo = program->getDescriptorSetManager().getSetBindingInfo();
    // for each set
    uint32_t bufferInfoIdx = 0;
    for (int setIdx = 0; setIdx < setBindingInfo.size(); ++setIdx)
    {
        vkCmdSetDescriptorBufferOffsetsEXT(cmdBuf, bindPoint, layout, setIdx, 1, &bufferInfoIdx,
                                           &DescriptorSetOffsetMap[static_cast<DescriptorEnum>(setIdx)]);
    }
}

} // namespace Play