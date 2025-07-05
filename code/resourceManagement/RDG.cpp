#include "RDG.h"
#include <stdexcept>
#include "utils.hpp"
#include "nvh/nvprint.hpp"
namespace Play
{
bool RDG::RDGShaderParameters::addResource(RDGResourceHandle             resource,
                                           RDGResourceState::AccessType  accessType,
                                           RDGResourceState::AccessStage accessStage)
{
    NV_ASSERT(resource.isValid());
    if (accessType >= RDGResourceState::AccessType::eCount) {
        throw std::runtime_error("RDGShaderParameters: Invalid access type");
    }
    _resources[static_cast<size_t>(accessType)].push_back({accessType,accessStage, resource});
}
// RDGTextureDescriptionPool implementations
void RDG::RDGTextureDescriptionPool::init(uint32_t poolSize)
{
    this->_objs.resize(poolSize);
    this->_freeIndices.resize(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) {
        this->_freeIndices[i] = i;
    }
    this->_availableIndex = 0;
}

void RDG::RDGTextureDescriptionPool::deinit()
{
    for (auto obj : _objs)
    {
        if (obj && obj->_texture)
        {
            Play::PlayApp::FreeTexture(obj->_texture->_pData);
        }
        delete (obj);
    }
}

RDG::RDGResourceHandle RDG::RDGTextureDescriptionPool::alloc()
{
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGTextureDescriptionPool: No available texture description in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGTextureDescription();
    return {static_cast<int32_t>(index), RDGResourceHandle::ResourceType::eUndefeined};
}

void RDG::RDGTextureDescriptionPool::destroy(RDGResourceHandle handle)
{
    if(!handle.isValid()) return ;
    if(handle.getHandle()>= this->_objs.size()){
        throw std::runtime_error("RDGTextureDescriptionPool: Invalid texture description handle");
    }
    this->_freeIndices[--this->_availableIndex] = handle.getHandle();
    this->_objs[handle.getHandle()] = nullptr; // Clear the pointer
}

// RDGBufferDescriptionPool implementations
void RDG::RDGBufferDescriptionPool::init(uint32_t poolSize)
{
    this->_objs.resize(poolSize);
    this->_freeIndices.resize(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) {
        this->_freeIndices[i] = i;
    }
    this->_availableIndex = 0;
}

void RDG::RDGBufferDescriptionPool::deinit()
{
    for (auto obj : _objs)
    {
        if (obj && obj->_buffer)
        {
            Play::PlayApp::FreeBuffer(obj->_buffer->_pData);
        }
        delete (obj);
    }
}

Play::RDG::RDGResourceHandle RDG::RDGBufferDescriptionPool::alloc()
{
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGBufferDescriptionPool: No available buffer description in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGBufferDescription();
    return {static_cast<int32_t>(index), RDGResourceHandle::ResourceType::eUndefeined};
}

void RDG::RDGBufferDescriptionPool::destroy(RDGResourceHandle handle)
{
    if(!handle.isValid()) return ;
    if(handle.getHandle()>= this->_objs.size()){
        throw std::runtime_error("RDGBufferDescriptionPool: Invalid buffer description handle");
    }
    this->_freeIndices[--this->_availableIndex] = handle.getHandle();
    this->_objs[handle.getHandle()] = nullptr; // Clear the pointer
}

// RenderDependencyGraph implementations
RDG::RenderDependencyGraph::RenderDependencyGraph()
{

}
RDG::RenderDependencyGraph::~RenderDependencyGraph()
{

}

void                         RDG::RenderDependencyGraph::execute() {}
void                         RDG::RenderDependencyGraph::compile()
{
    for(auto& pass :this->_rdgPasses){
        
        for(auto& readOnlyResource : pass->_shaderParameters->_resources[static_cast<size_t>(RDGResourceState::AccessType::eReadOnly)]) {
            if (!readOnlyResource._resourceHandle.isValid()) return;
            if (readOnlyResource._resourceHandle.isTexture()) {
                auto textureDescription = this->getTextureDescription(readOnlyResource._resourceHandle);
                pass->_dependencies.push_back(textureDescription->_lastProducer);
            } else {
                auto bufferDescription = this->getBufferDescription(readOnlyResource._resourceHandle);
                pass->_dependencies.push_back(bufferDescription->_lastProducer);
            }
        }

        for(auto& writeOnlyResource : pass->_shaderParameters->_resources[static_cast<size_t>(RDGResourceState::AccessType::eWriteOnly)]) {
            if (!writeOnlyResource._resourceHandle.isValid()) return;
            if (writeOnlyResource._resourceHandle.isTexture()) {
                auto textureDescription = this->getTextureDescription(writeOnlyResource._resourceHandle);
                pass->_dependencies.push_back(textureDescription->_lastProducer);
            } else {
                auto bufferDescription = this->getBufferDescription(writeOnlyResource._resourceHandle);
                pass->_dependencies.push_back(bufferDescription->_lastProducer);
            }
        }

        for(auto& readWriteResource : pass->_shaderParameters->_resources[static_cast<size_t>(RDGResourceState::AccessType::eReadWrite)]) {
            if (!readWriteResource._resourceHandle.isValid()) return;
            if (readWriteResource._resourceHandle.isTexture()) {
                auto textureDescription = this->getTextureDescription(readWriteResource._resourceHandle);
                pass->_dependencies.push_back(textureDescription->_lastProducer);
                textureDescription->_lastProducer = pass; // Set the last producer for the texture
            } else {
                auto bufferDescription = this->getBufferDescription(readWriteResource._resourceHandle);
                pass->_dependencies.push_back(bufferDescription->_lastProducer);
                bufferDescription->_lastProducer = pass; // Set the last producer for the buffer
            }
        }

        hasCircle();
    }
    clipPasses();
    prepareResource();
}
RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTexture(
    std::string name, VkFormat format, VkImageType type, VkExtent3D extent,
    VkImageUsageFlags usageFlags, VkImageAspectFlags aspectFlags, int sampleCount,int textureCount)
{
    auto handle = this->_rdgTexturePool.alloc();
    if (!handle.isValid()) {
        throw std::runtime_error("RDGTextureDescriptionPool: Failed to allocate texture description handle");
    }
    auto textureDescription = this->getTextureDescription(handle);
    textureDescription->_format = format;
    textureDescription->_type = type;
    textureDescription->_extent = extent;
    textureDescription->_usageFlags = usageFlags;
    textureDescription->_aspectFlags = aspectFlags;
    textureDescription->_sampleCount = sampleCount;
    textureDescription->_textureCnt = textureCount;
    textureDescription->_debugName = name;
    handle._resourceType = this->inferResourceTypeFromImageUsage(usageFlags, textureCount);
    return handle;
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTexture2D(
    VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usageFlags,int textureCount)
{
    return this->createTexture2D(Play::GetUniqueName(), format, width, height, usageFlags, textureCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTexture2D(
    const std::string& name, VkFormat format, uint32_t width, uint32_t height,
    VkImageUsageFlags usageFlags,int textureCount)
{
   return this->createTexture(
        name, format, VK_IMAGE_TYPE_2D, {width, height, 1}, usageFlags,
        VK_IMAGE_ASPECT_COLOR_BIT, 1, textureCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createColorTarget(uint32_t width, uint32_t height, VkFormat format)
{
    return this->createColorTarget(Play::GetUniqueName(), width, height, format);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createColorTarget(
    const std::string& name, uint32_t width, uint32_t height, VkFormat format)
{
    return this->createTexture2D(
        name, format, width, height,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 1);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createDepthTarget(uint32_t width, uint32_t height, VkFormat format,VkSampleCountFlagBits sampleCnt)
{
    return this->createDepthTarget(Play::GetUniqueName(), width, height, format, sampleCnt);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createDepthTarget(
    const std::string& name, uint32_t width, uint32_t height, VkFormat format,VkSampleCountFlagBits sampleCnt)
{
    return this->createTexture2D(
        name, format, width, height,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        sampleCnt);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createComputeTexture2D(
    uint32_t width, uint32_t height, VkFormat format, int textureCount)
{
    return this->createComputeTexture2D(Play::GetUniqueName(), width, height, format, textureCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createComputeTexture2D(
    const std::string& name, uint32_t width, uint32_t height, VkFormat format, int textureCount)
{
    return this->createTexture2D(
        name, format, width, height,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,  textureCount);
}


RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTexture3D(
    uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usageFlags, int textureCount)
{
    return this->createTexture3D(Play::GetUniqueName(), width, height, depth, format, usageFlags, textureCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTexture3D(
    const std::string& name, uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
    VkImageUsageFlags usageFlags, int textureCount)
{
    return this->createTexture(
        name, format, VK_IMAGE_TYPE_3D, {width, height, depth}, usageFlags,
        VK_IMAGE_ASPECT_COLOR_BIT, 1, textureCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createMSAATexture2D(
    uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits samples)
{
    return this->createMSAATexture2D(Play::GetUniqueName(), width, height, format, samples);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createMSAATexture2D(
    const std::string& name, uint32_t width, uint32_t height, VkFormat format,
    VkSampleCountFlagBits samples)
{
    auto handle = this->_rdgTexturePool.alloc();
    if(!handle.isValid()){
        throw std::runtime_error("RDGTextureDescriptionPool: Failed to allocate texture description handle");
    }
    auto textureDescription = this->getTextureDescription(handle);
    textureDescription->_format = format;
    textureDescription->_type = VK_IMAGE_TYPE_2D;
    textureDescription->_extent = {width, height, 1};
    textureDescription->_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT |
                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    textureDescription->_aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT; // Default to color aspect
    textureDescription->_sampleCount = samples;
    textureDescription->_debugName = name;
    return handle;
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTextureLike(
    RDGResourceHandle reference, VkFormat format, VkImageUsageFlags usageFlags,int textureCount)
{
    if (!reference.isValid()) {
        throw std::runtime_error("RDGTextureDescriptionPool: Invalid reference texture handle");
    }
    auto refDesc = this->getTextureDescription(reference);
    if (!refDesc) {
        throw std::runtime_error("RDGTextureDescriptionPool: Reference texture description not found");
    }
    
    return this->createTexture(
        Play::GetUniqueName(), format, refDesc->_type, refDesc->_extent, usageFlags,
        refDesc->_aspectFlags, refDesc->_sampleCount, textureCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createTextureLike(const std::string& name,
                                                                     RDGResourceHandle  reference,
                                                                     VkFormat           format,
                                                                     VkImageUsageFlags  usageFlags,
                                                                     int textureCount)
{
    if (!reference.isValid())
    {
        throw std::runtime_error("RDGTextureDescriptionPool: Invalid reference texture handle");
    }
    auto refDesc = this->getTextureDescription(reference);
    if (!refDesc)
    {
        throw std::runtime_error(
            "RDGTextureDescriptionPool: Reference texture description not found");
    }

    return this->createTexture(name, format, refDesc->_type, refDesc->_extent, usageFlags,
                               refDesc->_aspectFlags, refDesc->_sampleCount, textureCount);
}
RDG::RDGResourceHandle RDG::RenderDependencyGraph::registExternalTexture(Texture* texture) {
    RDGResourceHandle handle = this->_rdgTexturePool.alloc();
    handle._resourceType = inferResourceTypeFromImageUsage(texture->_usageFlags, 1);
    auto* textureDescription = this->getTextureDescription(handle);
    textureDescription->_texture= std::make_shared<RDGTexture>(texture);
    textureDescription->_format = texture->_format;
    textureDescription->_type = texture->_type;
    textureDescription->_extent = texture->_extent;
    textureDescription->_usageFlags = texture->_usageFlags;
    textureDescription->_aspectFlags = texture->_aspectFlags;
    textureDescription->_sampleCount = texture->_sampleCount;
    textureDescription->_isExternalResource = true;
    _externalTextures[texture] = handle;
    return handle;
}
RDG::RDGResourceHandle RDG::RenderDependencyGraph::registExternalBuffer(Buffer* buffer) {
    RDGResourceHandle handle = this->_rdgBufferPool.alloc();
    auto* bufferDescription = this->getBufferDescription(handle);
    bufferDescription->_buffer = std::make_shared<RDGBuffer>(buffer);
    bufferDescription->_size = buffer->_size;
    bufferDescription->_usageFlags = buffer->_usageFlags;
    bufferDescription->_location = RDGBufferDescription::BufferLocation::eDeviceOnly;
    bufferDescription->_debugName = buffer->_debugName;
    bufferDescription->_isExternalResource = true;
    handle._resourceType = inferResourceTypeFromBufferUsage(*bufferDescription, 1);
    _externalBuffers[buffer] = handle;
    return handle;
}
void                   RDG::RenderDependencyGraph::onCreatePass(RDGPass* pass) {}
void                   RDG::RenderDependencyGraph::clipPasses()
{
    std::queue<RDGPass*> dependencyPassQueue;
    for (auto& externalTexture : this->_externalTextures)
    {
        auto textureDescription = this->getTextureDescription(externalTexture.second);
        if (textureDescription->_lastProducer)
        {
            dependencyPassQueue.push(textureDescription->_lastProducer);
        }
    }
    for (auto& externalBuffer : this->_externalBuffers)
    {
        auto bufferDescription = this->getBufferDescription(externalBuffer.second);
        if (bufferDescription->_lastProducer)
        {
            dependencyPassQueue.push(bufferDescription->_lastProducer);
        }
    }
    while (!dependencyPassQueue.empty())
    {
        RDGPass* pass = dependencyPassQueue.front();
        dependencyPassQueue.pop();
        if (pass->_isClipped) continue; // Skip already clipped passes
        pass->_isClipped = false;
        for (auto& dependency : pass->_dependencies)
        {
            dependencyPassQueue.push(dependency);
        }
    }
}
bool RDG::RenderDependencyGraph::hasCircle()
{
    std::unordered_set<RDGPass*> visited;
    std::unordered_set<RDGPass*> checked;
    for (auto& resourceHandle : this->_externalTextures)
    {
        auto* textureDescription = this->getTextureDescription(resourceHandle.second);
        if (!textureDescription->_lastProducer||checked.contains(textureDescription->_lastProducer)) continue;

        if (this->hasCircle(textureDescription->_lastProducer, visited))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency");
            return true;
        }
        checked.insert(textureDescription->_lastProducer);
    }
    for (auto & resourceHandle : this->_externalBuffers)
    {
        auto* bufferDescription = this->getBufferDescription(resourceHandle.second);
        if (!bufferDescription->_lastProducer||checked.contains(bufferDescription->_lastProducer)) continue;

        if (this->hasCircle(bufferDescription->_lastProducer, visited))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency");
            return true;
        }
        checked.insert(bufferDescription->_lastProducer);
    }
    return false;
}
void                   RDG::RenderDependencyGraph::prepareResource() {

    //textures, buffers, fbo, render pass,
    for (auto& pass : this->_rdgPasses) {
        if (pass->_isClipped) continue; // Skip already clipped passes
        
    }


}

void RDG::RenderDependencyGraph::destroyTexture(RDGResourceHandle handle) {
    if (!handle.isValid()||(handle._resourceType>=RDGResourceHandle::ResourceType::eTextureTypeEnd)) {
        LOGW("RDGTextureDescriptionPool: Attempted to destroy an invalid texture handle or a handle that is not a texture");
       return;
    }
    if (handle.getHandle() >= this->_rdgTexturePool._objs.size()) {
        LOGW("RDGTextureDescriptionPool: Attempted to destroy a non-existent texture handle");
        return;
    }
    auto textureDescription = this->getTextureDescription(handle);
    if (!textureDescription) {
        LOGW("RDGTextureDescriptionPool: Texture description not found for handle");
        return;
    }
    while(textureDescription->_texture) {
        Play::PlayApp::FreeTexture(textureDescription->_texture->_pData);
        textureDescription->_texture =  textureDescription->_texture->_next;
    }
    this->_rdgTexturePool.destroy(handle);
}

void RDG::RenderDependencyGraph::destroyBuffer(RDGResourceHandle handle) {
    if (!handle.isValid()||(handle._resourceType<=RDGResourceHandle::ResourceType::eTextureTypeEnd||handle._resourceType>=RDGResourceHandle::ResourceType::eBufferTypeEnd)) {
        LOGW("RDGBufferDescriptionPool: Attempted to destroy an invalid buffer handle or a handle that is not a buffer");
        return;
    }
    if (handle.getHandle() >= this->_rdgBufferPool._objs.size()) {
        LOGW("RDGBufferDescriptionPool: Attempted to destroy a non-existent buffer handle");
        return;
    }
    auto bufferDescription = this->getBufferDescription(handle);
    if (!bufferDescription) {
        LOGW("RDGBufferDescriptionPool: Buffer description not found for handle");
        return;
    }
    while (bufferDescription->_buffer) {
        Play::PlayApp::FreeBuffer(bufferDescription->_buffer->_pData);
        bufferDescription->_buffer = bufferDescription->_buffer->_next;
    }
    this->_rdgBufferPool.destroy(handle);
}
RDG::RDGResourceHandle RDG::RenderDependencyGraph::createBuffer(
    const std::string& name, VkDeviceSize size, VkBufferUsageFlags usageFlags,
    RDGBufferDescription::BufferLocation location,VkDeviceSize range,int bufferCount)
{
    auto resourceHandle = this->_rdgBufferPool.alloc();
    if (!resourceHandle.isValid()) {
        throw std::runtime_error("RDGBufferDescriptionPool: Failed to allocate buffer description handle");
    }
    auto bufferDescription = this->getBufferDescription(resourceHandle);
    bufferDescription->_usageFlags = usageFlags;
    bufferDescription->_size = size;
    bufferDescription->_location = location;
    bufferDescription->_bufferCnt = bufferCount;
    bufferDescription->_range = range;
    bufferDescription->_debugName = name;
    resourceHandle._resourceType = this->inferResourceTypeFromBufferUsage(*bufferDescription, bufferCount);
    return resourceHandle;
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createBuffer(
    VkDeviceSize size, VkBufferUsageFlags usageFlags, RDGBufferDescription::BufferLocation location,VkDeviceSize range,int bufferCount)
{
    return this->createBuffer(Play::GetUniqueName(), size, usageFlags, location, range, bufferCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createUniformBuffer(
    VkDeviceSize size,VkDeviceSize range,RDGBufferDescription::BufferLocation location)
{
    return this->createUniformBuffer(Play::GetUniqueName(), size, range, location);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createUniformBuffer(
    const std::string& name, VkDeviceSize size,VkDeviceSize range,RDGBufferDescription::BufferLocation location)
{
   
    return createBuffer (
        name, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        location, range, 1);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createStorageBuffer(
    VkDeviceSize size, int bufferCount)
{
    return this->createStorageBuffer(Play::GetUniqueName(), size, bufferCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createStorageBuffer(
    const std::string& name, VkDeviceSize size, int bufferCount)
{
    return createBuffer(
        name, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        RDGBufferDescription::BufferLocation::eDeviceOnly, VK_WHOLE_SIZE, bufferCount);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createDynamicUniformBuffer(
    VkDeviceSize size, VkBufferUsageFlags usageFlags, VkDeviceSize range)
{
    return this->createDynamicUniformBuffer(Play::GetUniqueName(), size, usageFlags, range);
}

RDG::RDGResourceHandle RDG::RenderDependencyGraph::createDynamicUniformBuffer(
    const std::string& name, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkDeviceSize range)
{
    return createBuffer(
        name, size, usageFlags  | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        RDGBufferDescription::BufferLocation::eHostVisible, range, 1);
}

RDG::RDGTextureDescription* RDG::RenderDependencyGraph::getTextureDescription(RDGResourceHandle handle)const {
    return _rdgTexturePool[handle];
}

RDG::RDGBufferDescription* RDG::RenderDependencyGraph::getBufferDescription(RDGResourceHandle handle) const{
    return _rdgBufferPool[handle];
}

void RDG::RenderDependencyGraph::createTextureByDescription(
    const RDGTextureDescription& description)
{
}

void RDG::RenderDependencyGraph::createBufferByDescription(
    const RDGBufferDescription& description)
{
}

RDG::RDGResourceHandle::ResourceType RDG::RenderDependencyGraph::inferResourceTypeFromImageUsage(VkImageUsageFlags usage, int textureCount){
    if (usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)) {
        NV_ASSERT(textureCount<=1 && "Texture count should be 1 for color or depth stencil attachments");
            return
                RDG::RDGResourceHandle::ResourceType::eAttachmentTexture;
        }
    else if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
        return textureCount > 1 ? 
            RDG::RDGResourceHandle::ResourceType::eUAVTextureArray : 
            RDG::RDGResourceHandle::ResourceType::eUAVTexture;
    }
    else {
        // 默认为SRV
        return textureCount > 1 ? 
            RDG::RDGResourceHandle::ResourceType::eSRVTextureArray : 
            RDG::RDGResourceHandle::ResourceType::eSRVTexture;
    }
}

RDG::RDGResourceHandle::ResourceType RDG::RenderDependencyGraph::inferResourceTypeFromBufferUsage(
    RDG::RDGBufferDescription& bufferDesc, int bufferCount)
{
    if (bufferDesc._usageFlags &
        (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT))
    {
        NV_ASSERT(bufferCount == 1 && "Buffer count should be 1 for uniform buffers");
        return RDG::RDGResourceHandle::ResourceType::eSRVBuffer;
    }
    else if (bufferDesc._usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    {
        return bufferCount > 1 ? RDG::RDGResourceHandle::ResourceType::eUAVBufferArray
                               : RDG::RDGResourceHandle::ResourceType::eUAVBuffer;
    }
    else if (bufferDesc._usageFlags & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
    {
        return RDG::RDGResourceHandle::ResourceType::eIndirectBuffer;
    }
    else
    {
        return RDG::RDGResourceHandle::ResourceType::eUndefeined;
    }
}

bool RDG::RenderDependencyGraph::hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited)
{
    if(!pass) return false;
    if(visited.find(pass) != visited.end()) {
        return true; // Found a circle
    }
    visited.insert(pass);
    pass->_isClipped = false;
    for(auto* consumer : pass->_dependencies) {
        if(this->hasCircle(consumer, visited)) {
            return true;
        }
    }
    visited.erase(pass);
    return false;

}

} // namespace Play