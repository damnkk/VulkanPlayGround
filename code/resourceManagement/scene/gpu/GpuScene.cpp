#include "GpuScene.h"
#include "core/Profiling.h"
#include "core/runtime/VulkanRuntime.h"
#include "nvvk/check_error.hpp"

namespace Play
{
static_assert(sizeof(GpuSceneIndirectDrawCommand) == sizeof(VkDrawIndirectCommand));
namespace
{
uint32_t growCapacity(uint32_t required)
{
    uint32_t capacity = 1;
    while (capacity < required) capacity *= 2;
    return capacity;
}

VkDeviceSize reserveRegion(VkDeviceSize& size, uint32_t capacity, VkDeviceSize stride)
{
    size                      = (size + 15) & ~VkDeviceSize(15);
    const VkDeviceSize offset = size;
    size += VkDeviceSize(capacity) * stride;
    return offset;
}

void memoryBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                   VkAccessFlags2 dstAccess)
{
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask  = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask  = dstStage;
    barrier.dstAccessMask = dstAccess;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers    = &barrier;
    vkCmdPipelineBarrier2(cmd, &dependency);
}
} // namespace

void GpuScene::DirtyRange::include(uint32_t offset, uint32_t count)
{
    if (count == 0) return;
    first = std::min(first, offset);
    end   = std::max(end, offset + count);
}

uint32_t GpuScene::registerModel(const ModelRef& model)
{
    if (!model || !model->getAssetBuffer() || !model->getAssetBuffer()->isValid()) return InvalidID;
    for (uint32_t modelID = 0; modelID < _models.size(); ++modelID)
    {
        if (_models[modelID] == model) return modelID;
    }
    const auto& textures = model->getTextures();
    if (_textures.size() + textures.size() > TextureCapacity)
    {
        LOGE("GpuScene texture array is full (%zu + %zu > %u)\n", _textures.size(), textures.size(), TextureCapacity);
        return InvalidID;
    }

    GpuSceneModelData data{};
    data.buffers               = model->getBufferInfo();
    data.buffers.textureOffset = static_cast<uint32_t>(_textures.size());
    data.drawableAddress       = model->getDrawableAddress();
    data.drawableCount         = static_cast<uint32_t>(model->getDrawableInfos().size());
    data.textureCount          = static_cast<uint32_t>(textures.size());
    // Preserve local texture indices, including missing-texture holes. ModelTextureInfo
    // already marks missing textures with -1, so shaders never access these holes.
    _textures.insert(_textures.end(), textures.begin(), textures.end());
    const uint32_t modelID = static_cast<uint32_t>(_models.size());
    _models.push_back(model);
    _modelData.push_back(data);
    for (auto& frame : _frames) frame.modelsDirty.include(modelID, 1);
    return modelID;
}

const GpuSceneModelData& GpuScene::getModelData(uint32_t modelID) const
{
    return _modelData.at(modelID);
}

GpuSceneRange GpuScene::allocateRange(std::vector<GpuSceneRange>& freeRanges, uint32_t count, uint32_t size)
{
    if (count == 0) return {};
    for (auto iter = freeRanges.begin(); iter != freeRanges.end(); ++iter)
    {
        if (iter->capacity < count) continue;
        const GpuSceneRange range{iter->offset, count};
        iter->offset += count;
        iter->capacity -= count;
        if (iter->capacity == 0) freeRanges.erase(iter);
        return range;
    }
    return {size, count};
}

void GpuScene::releaseRange(std::vector<GpuSceneRange>& freeRanges, GpuSceneRange range)
{
    if (range.capacity == 0) return;
    freeRanges.push_back(range);
    std::sort(freeRanges.begin(), freeRanges.end(), [](const auto& a, const auto& b) { return a.offset < b.offset; });
    for (size_t i = 1; i < freeRanges.size();)
    {
        auto& previous = freeRanges[i - 1];
        if (previous.offset + previous.capacity == freeRanges[i].offset)
        {
            previous.capacity += freeRanges[i].capacity;
            freeRanges.erase(freeRanges.begin() + i);
        }
        else
            ++i;
    }
}

GpuSceneRange GpuScene::allocateInstances(uint32_t capacity)
{
    if (capacity == 0) return {};
    const auto range = allocateRange(_freeInstanceRanges, growCapacity(capacity), static_cast<uint32_t>(_instanceData.size()));
    _instanceData.resize(std::max(_instanceData.size(), size_t(range.offset) + range.capacity));
    return range;
}

GpuSceneRange GpuScene::allocateDraws(uint32_t count)
{
    const auto range = allocateRange(_freeDrawRanges, count, static_cast<uint32_t>(_drawData.size()));
    _drawData.resize(std::max(_drawData.size(), size_t(range.offset) + range.capacity));
    _drawCommands.resize(_drawData.size());
    return range;
}

void GpuScene::releaseInstances(GpuSceneRange range)
{
    releaseRange(_freeInstanceRanges, range);
}

void GpuScene::releaseDraws(GpuSceneRange range)
{
    if (range.capacity == 0) return;
    for (uint32_t i = range.offset; i < range.offset + range.capacity; ++i) _drawCommands[i] = {};
    _drawSnapshotDirty = true;
    releaseRange(_freeDrawRanges, range);
}

void GpuScene::updateInstances(GpuSceneRange range, uint32_t first, uint32_t count, const GpuSceneInstanceData* data)
{
    assert(first <= range.capacity && count <= range.capacity - first);
    if (count == 0) return;
    std::copy_n(data, count, _instanceData.begin() + range.offset + first);
    for (auto& frame : _frames) frame.instancesDirty.include(range.offset + first, count);
}

void GpuScene::updateDraws(GpuSceneRange range, uint32_t first, uint32_t count, const GpuSceneDrawData* data)
{
    assert(first <= range.capacity && count <= range.capacity - first);
    if (count == 0) return;
    std::copy_n(data, count, _drawData.begin() + range.offset + first);
    for (auto& frame : _frames) frame.drawsDirty.include(range.offset + first, count);
}

void GpuScene::updateDrawCommands(const std::vector<DrawCommand>& commands)
{
    for (const auto& command : commands)
    {
        auto& cached = _drawCommands.at(command.drawID);
        if (cached.drawID != command.drawID || cached.materialInstanceRef != command.materialInstanceRef) _drawSnapshotDirty = true;
        cached = command;
    }
}

GpuScene::FrameResources& GpuScene::currentFrame()
{
    return _frames.at(vkDriver->getFrameCycleIndex());
}

const GpuScene::FrameResources& GpuScene::currentFrame() const
{
    return _frames.at(vkDriver->getFrameCycleIndex());
}

void GpuScene::ensureFrameCapacity(FrameResources& frame, uint32_t commandCount, uint32_t bucketCount)
{
    if (frame.buffer && frame.modelCapacity >= _modelData.size() && frame.drawCapacity >= _drawData.size() &&
        frame.instanceCapacity >= _instanceData.size() && frame.commandCapacity >= commandCount && frame.bucketCapacity >= bucketCount)
        return;

    frame.modelCapacity       = growCapacity(static_cast<uint32_t>(_modelData.size()));
    frame.drawCapacity        = growCapacity(static_cast<uint32_t>(_drawData.size()));
    frame.instanceCapacity    = growCapacity(static_cast<uint32_t>(_instanceData.size()));
    frame.commandCapacity     = std::max(frame.commandCapacity, growCapacity(commandCount));
    frame.bucketCapacity      = std::max(frame.bucketCapacity, growCapacity(bucketCount));
    VkDeviceSize size         = sizeof(GpuSceneData);
    frame.modelOffset         = reserveRegion(size, frame.modelCapacity, sizeof(GpuSceneModelData));
    frame.drawOffset          = reserveRegion(size, frame.drawCapacity, sizeof(GpuSceneDrawData));
    frame.instanceOffset      = reserveRegion(size, frame.instanceCapacity, sizeof(GpuSceneInstanceData));
    frame.candidateOffset     = reserveRegion(size, frame.commandCapacity, sizeof(GpuSceneDrawCandidate));
    frame.bucketOffset        = reserveRegion(size, frame.bucketCapacity, sizeof(GpuSceneDrawBucket));
    frame.indirectOffset      = reserveRegion(size, frame.commandCapacity, sizeof(VkDrawIndirectCommand));
    frame.countOffset         = reserveRegion(size, frame.bucketCapacity, sizeof(uint32_t));
    frame.visibleDrawIDOffset = reserveRegion(size, frame.commandCapacity, sizeof(uint32_t));

    frame.buffer = RefPtr<Buffer>(new Buffer("GpuScene frame arena",
                                             VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
                                                 VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
                                             size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    frame.modelsDirty.include(0, static_cast<uint32_t>(_modelData.size()));
    frame.drawsDirty.include(0, static_cast<uint32_t>(_drawData.size()));
    frame.instancesDirty.include(0, static_cast<uint32_t>(_instanceData.size()));
    frame.root          = {};
    frame.rootDirty     = true;
    frame.snapshotDirty = true;
}

void GpuScene::rebuildDrawSnapshot()
{
    _buckets.clear();
    _gpuBuckets.clear();
    _candidates.clear();
    std::vector<std::vector<uint32_t>>              bucketDraws;
    std::unordered_map<MaterialInstance*, uint32_t> bucketIDs;
    const uint32_t                                  maxDrawCount = vkDriver->_physicalDeviceProperties2.properties.limits.maxDrawIndirectCount;
    for (const auto& command : _drawCommands)
    {
        if (command.drawID == InvalidID) continue;
        auto iter = bucketIDs.find(command.materialInstanceRef);
        if (iter == bucketIDs.end() || bucketDraws[iter->second].size() == maxDrawCount)
        {
            const uint32_t bucketID                = static_cast<uint32_t>(bucketDraws.size());
            bucketIDs[command.materialInstanceRef] = bucketID;
            bucketDraws.emplace_back();
            _buckets.push_back({command.materialInstanceRef, {}});
            iter = bucketIDs.find(command.materialInstanceRef);
        }
        bucketDraws[iter->second].push_back(command.drawID);
    }

    for (uint32_t bucketID = 0; bucketID < bucketDraws.size(); ++bucketID)
    {
        const auto&        drawIDs = bucketDraws[bucketID];
        GpuSceneDrawBucket bucket{static_cast<uint32_t>(_candidates.size()), static_cast<uint32_t>(drawIDs.size()), bucketID, 0};
        _buckets[bucketID].gpu = bucket;
        _gpuBuckets.push_back(bucket);
        for (const uint32_t drawID : drawIDs)
        {
            _candidates.push_back({drawID, bucketID});
        }
    }
    _drawSnapshotDirty = false;
    for (auto& frame : _frames) frame.snapshotDirty = true;
}

void GpuScene::prepareFrame()
{
    PLAY_PROFILE_SCOPE("GpuScene::prepareFrame");
    const uint32_t frameCount = vkDriver->getFrameCycleSize();
    if (_frames.size() != frameCount)
    {
        // The runtime waits for the device before changing the frame-cycle size.
        _frames.clear();
        _frames.resize(frameCount);
    }
    if (_drawSnapshotDirty) rebuildDrawSnapshot();

    auto& frame = currentFrame();
    ensureFrameCapacity(frame, static_cast<uint32_t>(_candidates.size()), static_cast<uint32_t>(_buckets.size()));

    const VkDeviceAddress base = frame.buffer->address;
    const GpuSceneData    root = {base + frame.modelOffset,
                                  base + frame.drawOffset,
                                  base + frame.instanceOffset,
                                  base + frame.candidateOffset,
                                  base + frame.bucketOffset,
                                  base + frame.indirectOffset,
                                  base + frame.countOffset,
                                  base + frame.visibleDrawIDOffset,
                                  static_cast<uint32_t>(_modelData.size()),
                                  static_cast<uint32_t>(_drawData.size()),
                                  static_cast<uint32_t>(_instanceData.size()),
                                  static_cast<uint32_t>(_buckets.size()),
                                  static_cast<uint32_t>(_candidates.size()),
                                  0,
                                  0,
                                  0};
    if (memcmp(&root, &frame.root, sizeof(root)) != 0)
    {
        frame.root      = root;
        frame.rootDirty = true;
    }
    if (frame.snapshotDirty) frame.buckets = _buckets;
}

void GpuScene::updateTextureDescriptors(VkDescriptorSet descriptorSet)
{
    // Renderer construction can publish sky textures before the first frame.
    if (_frames.empty()) return;
    auto& frame = currentFrame();
    for (uint32_t index = frame.uploadedTextureCount; index < _textures.size(); ++index)
    {
        if (!_textures[index]) continue;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptorSet;
        write.dstBinding      = TextureBinding;
        write.dstArrayElement = index;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo      = &_textures[index]->descriptor;
        vkUpdateDescriptorSets(vkDriver->getDevice(), 1, &write, 0, nullptr);
    }
    frame.uploadedTextureCount = static_cast<uint32_t>(_textures.size());
}

void GpuScene::cmdUpload(VkCommandBuffer cmd)
{
    auto& frame    = currentFrame();
    auto& uploader = PlayResourceManager::Instance();
    uploader.releaseStaging();
    // RDG signals this value when it submits the command buffer being recorded.
    const auto& submission = vkDriver->getCurrentFrameData();
    const auto  completion = nvvk::SemaphoreState::makeFixed(submission.semaphore, submission.timelineValue + 1);
    bool        uploaded   = false;
    auto        append     = [&](VkDeviceSize offset, VkDeviceSize size, const void* data)
    {
        if (size == 0) return;
        NVVK_CHECK(uploader.appendBuffer(*frame.buffer, offset, size, data, completion));
        uploaded = true;
    };
    if (frame.rootDirty)
    {
        append(0, sizeof(frame.root), &frame.root);
        frame.rootDirty = false;
    }
    auto appendDirty = [&](VkDeviceSize offset, VkDeviceSize stride, const void* data, DirtyRange& dirty)
    {
        if (dirty.first == InvalidID) return;
        append(offset + stride * dirty.first, stride * (dirty.end - dirty.first), static_cast<const char*>(data) + stride * dirty.first);
        dirty = {};
    };
    appendDirty(frame.modelOffset, sizeof(GpuSceneModelData), _modelData.data(), frame.modelsDirty);
    appendDirty(frame.drawOffset, sizeof(GpuSceneDrawData), _drawData.data(), frame.drawsDirty);
    appendDirty(frame.instanceOffset, sizeof(GpuSceneInstanceData), _instanceData.data(), frame.instancesDirty);
    if (frame.snapshotDirty)
    {
        append(frame.candidateOffset, _candidates.size() * sizeof(GpuSceneDrawCandidate), _candidates.data());
        append(frame.bucketOffset, _gpuBuckets.size() * sizeof(GpuSceneDrawBucket), _gpuBuckets.data());
        frame.snapshotDirty = false;
    }
    // Append and record together: no pending scene uploads remain in the shared uploader.
    if (uploaded)
    {
        uploader.cmdUploadAppended(cmd);
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                      VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
    }
    // Visibility is per-frame output. Persistent input tables are never rebuilt here.
    cmdResetDrawCounts(cmd);
}

VkDeviceAddress GpuScene::getRootAddress() const
{
    return currentFrame().buffer->address;
}

const GpuSceneData& GpuScene::getRootData() const
{
    return currentFrame().root;
}

Buffer* GpuScene::getFrameBuffer() const
{
    return currentFrame().buffer.get();
}

const std::vector<GpuScene::DrawBucket>& GpuScene::getDrawBuckets() const
{
    return currentFrame().buckets;
}

VkDeviceSize GpuScene::getIndirectOffset(uint32_t bucketID) const
{
    const auto& frame = currentFrame();
    return frame.indirectOffset + VkDeviceSize(frame.buckets.at(bucketID).gpu.firstCommand) * sizeof(VkDrawIndirectCommand);
}

VkDeviceSize GpuScene::getCountOffset(uint32_t bucketID) const
{
    const auto& frame = currentFrame();
    return frame.countOffset + VkDeviceSize(frame.buckets.at(bucketID).gpu.countIndex) * sizeof(uint32_t);
}

void GpuScene::cmdDrawBucket(VkCommandBuffer cmd, uint32_t bucketID) const
{
    const auto& frame  = currentFrame();
    const auto& bucket = frame.buckets.at(bucketID).gpu;
    if (bucket.capacity == 0) return;
    vkCmdDrawIndirectCount(cmd, frame.buffer->buffer, getIndirectOffset(bucketID), frame.buffer->buffer, getCountOffset(bucketID), bucket.capacity,
                           sizeof(VkDrawIndirectCommand));
}

void GpuScene::cmdResetDrawCounts(VkCommandBuffer cmd) const
{
    const auto& frame = currentFrame();
    if (frame.buckets.empty()) return;
    vkCmdFillBuffer(cmd, frame.buffer->buffer, frame.countOffset, frame.buckets.size() * sizeof(uint32_t), 0);
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
}

void GpuScene::cmdBarrierAfterCulling(VkCommandBuffer cmd) const
{
    memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT);
}

} // namespace Play
