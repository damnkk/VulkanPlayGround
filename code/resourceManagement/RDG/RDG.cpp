#include "RDG.h"
#include <stdexcept>
#include "queue"
#include "utils.hpp"
#include <nvutils/logger.hpp>
#include <nvvk/check_error.hpp>
#include "nvvk/debug_util.hpp"
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
            Play::TexturePool::Instance().free(obj->_rhi);
        }
        delete (obj);
    }
}

RDGTexture* RDGTexturePool::alloc()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGTexturePool: No available texture in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGTexture(index);
    return this->_objs[index];
}

void RDGTexturePool::destroy(TextureHandle handle)
{
    std::unique_lock<std::mutex> lock(_mutex);
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
            Play::BufferPool::Instance().free(obj->_rhi);
        }
        delete (obj);
    }
}

RDGBuffer* RDGBufferPool::alloc()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGBufferDescriptionPool: No available buffer description in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGBuffer(index);
    return this->_objs[index];
}

void RDGBufferPool::destroy(BufferHandle handle)
{
    std::unique_lock<std::mutex> lock(_mutex);
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
    RDGtexture->_rhi = texture;
    RDGtexture->isExternal = true;
    return RDGtexture;
}
RDGBuffer*  RenderDependencyGraph::registExternalBuffer(Buffer* buffer) {
    RDGBuffer* RDGbuffer = this->_rdgBufferPool.alloc();
    RDGbuffer->_rhi = buffer;
    RDGbuffer->isExternal = true;
    return RDGbuffer;
}
RDGTexture* RenderDependencyGraph::createTexture(const TextureDesc& desc)
{
    RDGTexture* texture = this->_rdgTexturePool.alloc();
    texture->_rhi  = new Texture(-1);
    texture->_rhi->Extent() = desc._extent;
    texture->_rhi->Format() = desc._format;
    texture->_rhi->Type() = desc._type;
    texture->_rhi->UsageFlags() = desc._usageFlags;
    texture->_rhi->AspectFlags() = desc._aspectFlags;
    texture->_rhi->SampleCount() = desc._sampleCount;
    texture->_rhi->MipLevel() = desc._mipmapLevel;
    texture->_rhi->LayerCount() = desc._layerCount;
    return texture;
}

RDGBuffer*  RenderDependencyGraph::createBuffer(const BufferDesc& desc)
{
    RDGBuffer* buffer = this->_rdgBufferPool.alloc();
    buffer->_rhi->DebugName() = desc._debugName;
    buffer->_rhi->BufferSize() = desc._size;
    buffer->_rhi->BufferRange() = desc._range;
    buffer->_rhi->BufferProperty() = desc._location==BufferDesc::MemoryLocation::eDeviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
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
    (hasCircle());
    clipPasses();
    prepareResource();

}

void RenderDependencyGraph::updatePassDependency(){
   
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
   
}

void RenderDependencyGraph::allocRHITexture(RDGTexture* texture) {
    if(texture == nullptr) {
        LOGW("RDG: Attempt to allocate invalid RDGTexture");
        return;
    }
    if(texture->_rhi){
        if(texture->_rhi->Id() != -1){
            LOGW("RDG: RDGTexture already allocated, skipping allocation");
            return;
        }else{
            delete texture->_rhi;
        }
    }
    if(texture->_rhi->UsageFlags()&VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT){
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imgInfo.usage = texture->_rhi->UsageFlags();
        imgInfo.extent = {texture->_rhi->Extent().width, texture->_rhi->Extent().height, 1};
        imgInfo.imageType = texture->_rhi->Type();
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.format = texture->_rhi->Format();
        imgInfo.samples = texture->_rhi->SampleCount();
        imgInfo.mipLevels = texture->_rhi->MipLevel();
        imgInfo.arrayLayers = texture->_rhi->LayerCount();
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.flags = 0;

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = texture->_rhi->image;
        switch(texture->_rhi->Type())
        {
            case VK_IMAGE_TYPE_1D:{
                viewInfo.viewType = (texture->_rhi->LayerCount() > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D);
                break;
            }
            case VK_IMAGE_TYPE_2D:{
                viewInfo.viewType = (texture->_rhi->LayerCount() > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
                break;
            }
            case VK_IMAGE_TYPE_3D:{
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
                break;
            }
            default:
            assert(0);
        }
        viewInfo.format = texture->_rhi->Format();
        viewInfo.subresourceRange.aspectMask = texture->_rhi->AspectFlags();
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = texture->_rhi->MipLevel();
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = texture->_rhi->LayerCount();
        texture->_rhi = Play::TexturePool::Instance().alloc(&imgInfo, &viewInfo);
        NVVK_DBG_NAME(texture->_rhi->image);
    }else{
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        switch (texture->_rhi->Type()) {
            case VK_IMAGE_TYPE_1D:
                imgInfo.extent = {texture->_rhi->Extent().width, 1, 1};
                imgInfo.imageType = VK_IMAGE_TYPE_1D;
                imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imgInfo.format = texture->_rhi->Format();
                imgInfo.usage = texture->_rhi->UsageFlags();
                imgInfo.samples = texture->_rhi->SampleCount();
                imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imgInfo.arrayLayers = 1;
                break;
            case VK_IMAGE_TYPE_2D:
                imgInfo = makeImage2DCreateInfo(
                    {texture->_rhi->Extent().width, texture->_rhi->Extent().height},
                    texture->_rhi->Format(), texture->_rhi->UsageFlags(),
                    texture->_rhi->MipLevel() > 1);
                break;
            case VK_IMAGE_TYPE_3D:
                imgInfo = makeImage3DCreateInfo(
                    {texture->_rhi->Extent().width, texture->_rhi->Extent().height, texture->_rhi->Extent().depth},
                    texture->_rhi->Format(), texture->_rhi->UsageFlags(),
                    texture->_rhi->MipLevel() > 1);
                break;
            default:
                assert(false);
                break;
        }
        texture->_rhi = TexturePool::Instance().alloc(imgInfo.extent.width, imgInfo.extent.height, imgInfo.extent.depth, imgInfo.format, imgInfo.usage, imgInfo.initialLayout);
        NVVK_DBG_NAME(texture->_rhi->image);
    }
    return;
}

// VkRenderPass RenderDependencyGraph::getOrCreateRenderPass(std::vector<RDGRTState>& rtStates){
//     std::vector<RTState> contextRtState;
//     for(std::size_t i = 0;i<rtStates.size();++i){
//         auto& stat = rtStates[i];
//         contextRtState.emplace_back(static_cast<uint8_t>(stat.loadType), static_cast<uint8_t>(stat.storeType),
//          stat.rtTexture->RHI(), stat.resolveTexture ? stat.resolveTexture->RHI() : nullptr);
//     }
//     return this->_element->GetOrCreateRenderPass(contextRtState);
// }


void RenderDependencyGraph::allocRHIBuffer(RDGBuffer* buffer) {
    if(buffer == nullptr){
        LOGW("RDG: Attempt to allocate invalid RDGBufer");
        return;
    }
    if(buffer->_rhi){
        if(buffer->_rhi->Id() != -1){
            LOGW("RDG: RDGBufer already allocated, skipping allocation");
            return;
        }else{
            delete buffer->_rhi;
        }
    }
    BufferPool::Instance().alloc(buffer->_rhi->BufferSize(), buffer->_rhi->UsageFlags(), buffer->_rhi->BufferProperty());
    return;
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