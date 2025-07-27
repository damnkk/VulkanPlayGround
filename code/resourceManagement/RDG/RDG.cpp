#include "RDG.h"
#include <stdexcept>
#include "utils.hpp"
#include "nvh/nvprint.hpp"
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
    this->_objs[index] = new RDGTexture();
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
    destroy(texture->handle);
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
    this->_objs[index] = new RDGBuffer();
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

// RenderDependencyGraph implementations
RenderDependencyGraph::RenderDependencyGraph()
{

}
RenderDependencyGraph::~RenderDependencyGraph()
{

}

void             RenderDependencyGraph::execute() {}
RDGTexture* RenderDependencyGraph::registExternalTexture(Texture* texture) {
    return nullptr;
}
RDGBuffer*  RenderDependencyGraph::registExternalBuffer(Buffer* buffer) {
    return nullptr;
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

void             RenderDependencyGraph::destroyTexture(TextureHandle handle) {}
void             RenderDependencyGraph::destroyBuffer(BufferHandle handle) {}
void             RenderDependencyGraph::destroyTexture(RDGTexture* texture) {}
void             RenderDependencyGraph::destroyBuffer(RDGBuffer* buffer) {}
void                         RenderDependencyGraph::compile()
{
    for(auto& pass :this->_rdgPasses){    
        hasCircle();
    }
    clipPasses();
    prepareResource();
}

void                   RenderDependencyGraph::onCreatePass(RDGPass* pass) {}
void                   RenderDependencyGraph::clipPasses()
{
    std::queue<RDGPass*> dependencyPassQueue;
    for (auto& externalTexture : this->_externalTextures)
    {
        
        if (externalTexture->_lastProducer)
        {
            dependencyPassQueue.push(externalTexture->_lastProducer);
        }
    }
    for (auto& externalBuffer : this->_externalBuffers)
    {
        if (externalBuffer->_lastProducer)
        {
            dependencyPassQueue.push(externalBuffer->_lastProducer);
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
bool RenderDependencyGraph::hasCircle()
{
    std::unordered_set<RDGPass*> visited;
    std::unordered_set<RDGPass*> checked;
    for (auto& externalTexture : this->_externalTextures)
    {

        if (!externalTexture->_lastProducer||checked.contains(externalTexture->_lastProducer)) continue;

        if (this->hasCircle(externalTexture->_lastProducer, visited))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency");
            return true;
        }
        checked.insert(externalTexture->_lastProducer);
    }
    for (auto& externalBuffer : this->_externalBuffers)
    {
        if (!externalBuffer->_lastProducer || checked.contains(externalBuffer->_lastProducer)) continue;

        if (this->hasCircle(externalBuffer->_lastProducer, visited))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency");
            return true;
        }
        checked.insert(externalBuffer->_lastProducer);
    }
    return false;
}
void RenderDependencyGraph::prepareResource()
{
    // textures, buffers, fbo, render pass,
    for (auto& pass : this->_rdgPasses)
    {
        if (pass->_isClipped) continue; // Skip already clipped passes
    }
}
void RenderDependencyGraph::allocRHITexture(RDGTexture* texture) {
    // if(texture->_metadata._usageFlags&VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT){}
    //                 else{
    //                    VkImageCreateInfo imgInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    //                    switch (texture->_metadata._type) {
    //                     case VK_IMAGE_TYPE_1D:
    //                         imgInfo.extent = {texture->_metadata._extent.width, 1, 1};
    //                         imgInfo.imageType = VK_IMAGE_TYPE_1D;
    //                         imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //                         imgInfo.format = texture->_metadata._format;
    //                         imgInfo.usage = texture->_metadata._usageFlags;
    //                         imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    //                         imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    //                         imgInfo.arrayLayers = 1;
    //                     case VK_IMAGE_TYPE_2D:
    //                         imgInfo = nvvk::makeImage2DCreateInfo(
    //                             {texture->_metadata._extent.width, texture->_metadata._extent.height},
    //                             texture->_metadata._format, texture->_metadata._usageFlags,
    //                             texture->_metadata._mipmapLevel > 1);
    //                         break;
    //                     case VK_IMAGE_TYPE_3D:
    //                         imgInfo = nvvk::makeImage3DCreateInfo(
    //                             {texture->_metadata._extent.width, texture->_metadata._extent.height, texture->_metadata._extent.depth},
    //                             texture->_metadata._format, texture->_metadata._usageFlags,
    //                             texture->_metadata._mipmapLevel > 1);
    //                         break;
    //                     default:
    //                         NV_ASSERT(false);
    //                         break;
    //                    }
    //                    auto cmd= _hostGraph->getApp()->createTempCmdBuffer();
    //                     nvvk::Texture image = _hostGraph->getApp()->_alloc.createTexture(const Image &image, const VkImageViewCreateInfo &imageViewCreateInfo, const VkSamplerCreateInfo &samplerCreateInfo);
    //                 }
}
void RenderDependencyGraph::allocRHIBuffer(RDGBuffer* buffer) {}

bool RenderDependencyGraph::hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited)
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