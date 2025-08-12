#include "RDG.h"
#include <stdexcept>
#include "utils.hpp"
#include "nvh/nvprint.hpp"
#include "nvvk/images_vk.hpp"
namespace Play::RDG
{
// RDGTexturePool implementations
void RDGTexturePool::init(uint32_t poolSize)
{
    this->_objs.resize(poolSize);
    this->_freeIndices.resize(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) {
        this->_freeIndices[i] = i;
    }
    this->_availableIndex = 0;
}

void RDGTexturePool::deinit()
{
    for (auto obj : _objs)
    {
        if (obj)
        {
            Play::PlayApp::FreeTexture(obj->_pData);   
        }
        delete (obj);
    }
}

RDGTexture* RDGTexturePool::alloc()
{
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGTexturePool: No available texture in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGTexture(index);
    return this->_objs[index];
}

void RDGTexturePool::destroy(TextureHandle handle)
{
    if(!handle.isValid()) return ;
    if(handle.index >= this->_objs.size()){
        throw std::runtime_error("RDGTexturePool: Invalid texture handle");
    }
    this->_freeIndices[--this->_availableIndex] = handle.index;
    this->_objs[handle.index] = nullptr; // Clear the pointer
}

void RDGTexturePool::destroy(RDGTexture* texture){
    if (texture == nullptr) {
        return;
    }
    destroy(texture->_handle);
}

// RDGBufferPool implementations
void RDGBufferPool::init(uint32_t poolSize)
{
    this->_objs.resize(poolSize);
    this->_freeIndices.resize(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) {
        this->_freeIndices[i] = i;
    }
    this->_availableIndex = 0;
}

void RDGBufferPool::deinit()
{
    for (auto obj : _objs)
    {
        if (obj )
        {
            Play::PlayApp::FreeBuffer(obj->_pData);
        }
        delete (obj);
    }
}

RDGBuffer* RDGBufferPool::alloc()
{
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGBufferDescriptionPool: No available buffer description in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGBuffer(index);
    return this->_objs[index];
}

void RDGBufferPool::destroy(BufferHandle handle)
{
    if(!handle.isValid()) return ;
    if(handle.index>= this->_objs.size()){
        throw std::runtime_error("RDGBufferPool: Invalid buffer handle");
    }
    this->_freeIndices[--this->_availableIndex] = handle.index;
    this->_objs[handle.index] = nullptr; // Clear the pointer
}

void RDGBufferPool::destroy(RDGBuffer* buffer){
    if (buffer == nullptr) {
        return;
    }
    destroy(buffer->_handle);
}

// RenderDependencyGraph implementations
RenderDependencyGraph::RenderDependencyGraph()
{
    _rdgTexturePool.init(1024);
    _rdgBufferPool.init(1024);
}
RenderDependencyGraph::~RenderDependencyGraph()
{
    _rdgTexturePool.deinit();
    _rdgBufferPool.deinit();
    for(auto& pass : _rdgPasses) {
        delete pass;
    }
}

void             RenderDependencyGraph::execute() {}
RDGTexture* RenderDependencyGraph::registExternalTexture(Texture* texture) {
    RDGTexture* RDGtexture = this->_rdgTexturePool.alloc();
    RDGtexture->_pData = texture;
    RDGtexture->isExternal = true;
    return RDGtexture;
}
RDGBuffer*  RenderDependencyGraph::registExternalBuffer(Buffer* buffer) {
    RDGBuffer* RDGbuffer = this->_rdgBufferPool.alloc();
    RDGbuffer->_pData = buffer;
    RDGbuffer->isExternal = true;
    return RDGbuffer;
}
RDGTexture* RenderDependencyGraph::createTexture(const TextureDesc& desc)
{
    RDGTexture* texture = this->_rdgTexturePool.alloc();
    texture->_pData = PlayApp::AllocTexture();
    texture->_pData->_metadata._debugName = desc._debugName;
    texture->_pData->_metadata._extent = desc._extent;
    texture->_pData->_metadata._format = desc._format;
    texture->_pData->_metadata._type = desc._type;
    texture->_pData->_metadata._usageFlags = desc._usageFlags;
    texture->_pData->_metadata._aspectFlags = desc._aspectFlags;
    texture->_pData->_metadata._sampleCount = desc._sampleCount;
    texture->_pData->_metadata._mipmapLevel = desc._mipmapLevel;
    texture->_pData->_metadata._layerCount = desc._layerCount;
    return texture;
}

RDGBuffer*  RenderDependencyGraph::createBuffer(const BufferDesc& desc)
{
    RDGBuffer* buffer = this->_rdgBufferPool.alloc();
    buffer->_pData = PlayApp::AllocBuffer();
    buffer->_pData->_metadata._debugName = desc._debugName;
    buffer->_pData->_metadata._usageFlags = desc._usageFlags;
    buffer->_pData->_metadata._size = desc._size;
    buffer->_pData->_metadata._range = desc._range;
    buffer->_pData->_metadata._location = desc._location;
    return buffer;
}

void             RenderDependencyGraph::destroyTexture(TextureHandle handle) {
    _rdgTexturePool.destroy(handle);
}
void             RenderDependencyGraph::destroyBuffer(BufferHandle handle) {
    _rdgBufferPool.destroy(handle);
}
void             RenderDependencyGraph::destroyTexture(RDGTexture* texture) {
    _rdgTexturePool.destroy(texture);
}
void             RenderDependencyGraph::destroyBuffer(RDGBuffer* buffer) {
    _rdgBufferPool.destroy(buffer);
}
void             RenderDependencyGraph::compile()
{
    if (_rdgPasses.empty()) {
        LOGE("RDG: No passes to compile");
        return;
    }
    updatePassDependency();
    NV_ASSERT(hasCircle());
    clipPasses();
    prepareResource();

}

void RenderDependencyGraph::updatePassDependency(){
    for(auto& pass:_rdgPasses){
        pass->updateResourceAccessState();
    }
    for(auto& pass:_rdgPasses){
        for(auto& readAccessResource: pass->_shaderParameters->_resources[static_cast<size_t>(AccessType::eReadOnly)]){
            for(auto& resource: readAccessResource._resources){
                NV_ASSERT(resource);
                // 直接转换为基类指针，避免重复的类型检查
                RDGResourceBase* baseResource = nullptr;
                if(readAccessResource._resourceType == ResourceType::eTexture){
                    baseResource = static_cast<RDGTexture*>(resource);
                }
                else if(readAccessResource._resourceType == ResourceType::eBuffer){
                    baseResource = static_cast<RDGBuffer*>(resource);
                }
                
                if(baseResource) {
                    std::optional<uint32_t> lastProducer = baseResource->getLastProducer(pass->_passID.value());
                    if(lastProducer.has_value()){
                        pass->_dependencies.insert(_rdgPasses[lastProducer.value()]);
                    }
                }
            }
        }
        for(auto& rwAccessResource: pass->_shaderParameters->_resources[static_cast<size_t>(AccessType::eReadWrite)]){
            for(auto& resource: rwAccessResource._resources){
                NV_ASSERT(resource);
                // 直接转换为基类指针，避免重复的类型检查
                RDGResourceBase* baseResource = nullptr;
                if(rwAccessResource._resourceType == ResourceType::eTexture){
                    baseResource = static_cast<RDGTexture*>(resource);
                }
                else if(rwAccessResource._resourceType == ResourceType::eBuffer){
                    baseResource = static_cast<RDGBuffer*>(resource);
                }
                
                if(baseResource) {
                    std::optional<uint32_t> lastProducer = baseResource->getLastProducer(pass->_passID.value());
                    if(lastProducer.has_value()){
                        pass->_dependencies.insert(_rdgPasses[lastProducer.value()]);
                    }
                    baseResource->getProducers().push_back(pass->_passID.value());
                }
            } 
        }
    }
}

void                   RenderDependencyGraph::onCreatePass(RDGPass* pass) {}
void                   RenderDependencyGraph::clipPasses()
{
    std::queue<RDGPass*> dependencyPassQueue;
    for (auto& externalTexture : this->_externalTextures)
    {
        RDGPass* passPtr = _rdgPasses[externalTexture->getLastProducer().value()];
        if (passPtr)
        {
            dependencyPassQueue.push(passPtr);
        }
    }
    for (auto& externalBuffer : this->_externalBuffers)
    {
        RDGPass* passPtr = _rdgPasses[externalBuffer->getLastProducer().value()];
        if (passPtr)
        {
            dependencyPassQueue.push(passPtr);
        }
    }
    while (!dependencyPassQueue.empty())
    {
        RDGPass* pass = dependencyPassQueue.front();
        dependencyPassQueue.pop();
        pass->_isClipped = false;
        for (auto& dependency : pass->_dependencies)
        {
            dependencyPassQueue.push(dependency);
        }
    }
}
bool RenderDependencyGraph::hasCircle()
{
    std::unordered_set<RDGPass*> visited;
    _passDepthLayout.resize(_rdgPasses.size(), -1);
    for (auto& externalTexture : this->_externalTextures)
    {
        if (!externalTexture->getProducers().empty()) continue;
        if (this->hasCircle(_rdgPasses[externalTexture->getLastProducer().value()], visited, 0))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency or invalid pass");
            return true;
        }
        visited.clear();
    }
    for (auto& externalBuffer : this->_externalBuffers)
    {
        if (!externalBuffer->getProducers().empty()) continue;
        if (this->hasCircle(_rdgPasses[externalBuffer->getLastProducer().value()], visited, 0))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency or invalid pass");
            return true;
        }
        visited.clear();
    }
    return false;
}

void RenderDependencyGraph::prepareResource()
{
    // textures, buffers, fbo, render pass,
    for (auto& pass : this->_rdgPasses)
    {
        if (pass->_isClipped) continue; // Skip already clipped passes
        pass->prepareResource();
    }
}

RDGTexture* RenderDependencyGraph::allocRHITexture(RDGTexture* texture) {
    if(texture == nullptr) {
        LOGW("RDG: Attempt to allocate invalid RDGTexture");
        return nullptr;
    }
    if(texture->_pData){
        LOGW("RDG: RDGTexture already allocated, skipping allocation");
        return texture;
    }
    if(texture->_pData->_metadata._usageFlags&VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT){
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imgInfo.usage = texture->_pData->_metadata._usageFlags;
        imgInfo.extent = {texture->_pData->_metadata._extent.width, texture->_pData->_metadata._extent.height, 1};
        imgInfo.imageType = texture->_pData->_metadata._type;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.format = texture->_pData->_metadata._format;
        imgInfo.samples = texture->_pData->_metadata._sampleCount;
        imgInfo.mipLevels = texture->_pData->_metadata._mipmapLevel;
        imgInfo.arrayLayers = texture->_pData->_metadata._layerCount;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.flags = 0;
        vkCreateImage(_app->getDevice(), &imgInfo, nullptr, &texture->_pData->image);
        VkMemoryRequirements2          memReqs{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
        VkMemoryDedicatedRequirements  dedicatedRegs = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
        VkImageMemoryRequirementsInfo2 imageReqs{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
        memReqs.pNext = &dedicatedRegs;
        vkGetImageMemoryRequirements2(_app->getDevice(), &imageReqs, &memReqs);
        nvvk::MemAllocateInfo allocInfo (memReqs.memoryRequirements,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,true);
        if(dedicatedRegs.requiresDedicatedAllocation){
            allocInfo.setDedicatedImage(texture->_pData->image);
        }
        texture->_pData->memHandle = _app->_alloc.AllocateMemory(allocInfo);
        if(texture->_pData->memHandle){
            const auto memInfo = _app->_alloc.getMemoryInfo(texture->_pData->memHandle);
            vkBindImageMemory(_app->getDevice(), texture->_pData->image, memInfo.memory, memInfo.offset);
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = texture->_pData->image;
        switch(texture->_pData->_metadata._type)
        {
            case VK_IMAGE_TYPE_1D:{
                viewInfo.viewType = (texture->_pData->_metadata._layerCount > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D);
                break;
            }
            case VK_IMAGE_TYPE_2D:{
                viewInfo.viewType = (texture->_pData->_metadata._layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
                break;
            }
            case VK_IMAGE_TYPE_3D:{
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
                break;
            }
            default:
            assert(0);
        }
        viewInfo.format = texture->_pData->_metadata._format;
        viewInfo.subresourceRange.aspectMask = texture->_pData->_metadata._aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = texture->_pData->_metadata._mipmapLevel;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = texture->_pData->_metadata._layerCount;
        nvvk::Texture nvvkTexture = _app->_alloc.createTexture({texture->_pData->image,texture->_pData->memHandle}, viewInfo, nvvk::makeSamplerCreateInfo());
        texture->_pData->image = nvvkTexture.image;
        texture->_pData->memHandle = nvvkTexture.memHandle;
        texture->_pData->descriptor = nvvkTexture.descriptor;
    }else{
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        switch (texture->_pData->_metadata._type) {
            case VK_IMAGE_TYPE_1D:
                imgInfo.extent = {texture->_pData->_metadata._extent.width, 1, 1};
                imgInfo.imageType = VK_IMAGE_TYPE_1D;
                imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imgInfo.format = texture->_pData->_metadata._format;
                imgInfo.usage = texture->_pData->_metadata._usageFlags;
                imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imgInfo.arrayLayers = 1;
                break;
            case VK_IMAGE_TYPE_2D:
                imgInfo = nvvk::makeImage2DCreateInfo(
                    {texture->_pData->_metadata._extent.width, texture->_pData->_metadata._extent.height},
                    texture->_pData->_metadata._format, texture->_pData->_metadata._usageFlags,
                    texture->_pData->_metadata._mipmapLevel > 1);
                break;
            case VK_IMAGE_TYPE_3D:
                imgInfo = nvvk::makeImage3DCreateInfo(
                    {texture->_pData->_metadata._extent.width, texture->_pData->_metadata._extent.height, texture->_pData->_metadata._extent.depth},
                    texture->_pData->_metadata._format, texture->_pData->_metadata._usageFlags,
                    texture->_pData->_metadata._mipmapLevel > 1);
                break;
            default:
                NV_ASSERT(false);
                break;
        }
        auto cmd = _app->createTempCmdBuffer();
        nvvk::Texture nvvkTexture = _app->_alloc.createTexture(
            cmd, 0, nullptr, imgInfo, nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_UNDEFINED);
        _app->submitTempCmdBuffer(cmd);
        texture->_pData->image = nvvkTexture.image;
        texture->_pData->memHandle = nvvkTexture.memHandle;
        texture->_pData->descriptor = nvvkTexture.descriptor;
    }
    return texture;
}

VkRenderPass RenderDependencyGraph::getOrCreateRenderPass(std::vector<RDGRTState>& rtStates){
    std::vector<RTState> contextRtState;
    for(std::size_t i = 0;i<rtStates.size();++i){
        auto& stat = rtStates[i];
        contextRtState.emplace_back(static_cast<uint8_t>(stat.loadType), static_cast<uint8_t>(stat.storeType),
         stat.rtTexture->RHI(), stat.resolveTexture ? stat.resolveTexture->RHI() : nullptr);
    }
    return this->_app->GetOrCreateRenderPass(contextRtState);
}
void   RenderDependencyGraph::getOrCreatePipeline(RDGGraphicPipelineState& pipelineState,VkRenderPass renderPass){
    if(pipelineState._pipeline!= VK_NULL_HANDLE) return;
    _app->GetOrCreatePipeline(pipelineState,{pipelineState._vshaderInfo,pipelineState._fshaderInfo},renderPass);
}
void   RenderDependencyGraph::getOrCreatePipeline(RDGComputePipelineState& pipelineState){
    if(pipelineState._pipeline!= VK_NULL_HANDLE) return;
    _app->GetOrCreatePipeline(pipelineState._cshaderInfo);
}

RDGBuffer* RenderDependencyGraph::allocRHIBuffer(RDGBuffer* buffer) {
    if(buffer == nullptr){
        LOGW("RDG: Attempt to allocate invalid RDGBufer");
        return nullptr;
    }
    if(buffer->_pData){
        LOGW("RDG: RDGBufer already allocated, skipping allocation");
        return buffer;
    }
    Buffer::BufferMetaData* metaData = &buffer->_pData->_metadata;
    nvvk::Buffer nvvkBuffer = _app->_alloc.createBuffer(metaData->_size,metaData->_usageFlags,metaData->_location==Buffer::BufferMetaData::BufferLocation::eDeviceOnly? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    buffer->_pData->buffer = nvvkBuffer.buffer;
    buffer->_pData->memHandle = nvvkBuffer.memHandle;
    buffer->_pData->address = nvvkBuffer.address;
    buffer->_pData->descriptor.buffer = buffer->_pData->buffer;
    buffer->_pData->descriptor.offset = 0;
    buffer->_pData->descriptor.range = buffer->_pData->_metadata._range;
    return buffer;
}

bool RenderDependencyGraph::hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited,int currDepth)
{
    if(!pass||!pass->_passID.has_value()) return false;
    if(visited.find(pass) != visited.end()) {
        return true; // Found a circle
    }
    visited.insert(pass);
    _passDepthLayout[pass->_passID.value()] = std::max(_passDepthLayout[pass->_passID.value()], currDepth);
    for(auto* consumer : pass->_dependencies) {
        if(this->hasCircle(consumer, visited, currDepth + 1)) {
            return true;
        }
    }
    visited.erase(pass);
    return false;
}

} // namespace Play