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

// RDGTexturePool implementations
void RDG::RDGTexturePool::init(uint32_t poolSize)
{
    this->_objs.resize(poolSize);
    this->_freeIndices.resize(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) {
        this->_freeIndices[i] = i;
    }
    this->_availableIndex = 0;
}

void RDG::RDGTexturePool::deinit()
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

RDG::RDGTexture* RDG::RDGTexturePool::alloc()
{
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGTexturePool: No available texture in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGTexture();
    return this->_objs[index];
}

void RDG::RDGTexturePool::destroy(TextureHandle handle)
{
    if(!handle.isValid()) return ;
    if(handle.index >= this->_objs.size()){
        throw std::runtime_error("RDGTexturePool: Invalid texture handle");
    }
    this->_freeIndices[--this->_availableIndex] = handle.index;
    this->_objs[handle.index] = nullptr; // Clear the pointer
}

void RDG::RDGTexturePool::destroy(RDG::RDGTexture* texture){
    if (texture == nullptr) {
        return;
    }
    destroy(texture->handle);
}

// RDGBufferPool implementations
void RDG::RDGBufferPool::init(uint32_t poolSize)
{
    this->_objs.resize(poolSize);
    this->_freeIndices.resize(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) {
        this->_freeIndices[i] = i;
    }
    this->_availableIndex = 0;
}

void RDG::RDGBufferPool::deinit()
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

RDG::RDGBuffer* RDG::RDGBufferPool::alloc()
{
    if(this->_availableIndex >= this->_objs.size()){
        throw std::runtime_error("RDGBufferDescriptionPool: No available buffer description in pool");
    }
    uint32_t index = this->_freeIndices[this->_availableIndex++];
    this->_objs[index] = new RDGBuffer();
    return this->_objs[index];
}

void RDG::RDGBufferPool::destroy(RDG::BufferHandle handle)
{
    if(!handle.isValid()) return ;
    if(handle.index>= this->_objs.size()){
        throw std::runtime_error("RDGBufferPool: Invalid buffer handle");
    }
    this->_freeIndices[--this->_availableIndex] = handle.index;
    this->_objs[handle.index] = nullptr; // Clear the pointer
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
        
       
        hasCircle();
    }
    clipPasses();
    prepareResource();
}

void                   RDG::RenderDependencyGraph::onCreatePass(RDGPass* pass) {}
void                   RDG::RenderDependencyGraph::clipPasses()
{
    std::queue<RDGPass*> dependencyPassQueue;
    for (auto& externalTexture : this->_externalTextures)
    {
        
        if (externalTexture.first->_lastProducer)
        {
            dependencyPassQueue.push(externalTexture.first->_lastProducer);
        }
    }
    for (auto& externalBuffer : this->_externalBuffers)
    {
        if (externalBuffer.first->_lastProducer)
        {
            dependencyPassQueue.push(externalBuffer.first->_lastProducer);
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
    for (auto& [first,second] : this->_externalTextures)
    {

        if (!first->_lastProducer||checked.contains(first->_lastProducer)) continue;

        if (this->hasCircle(first->_lastProducer, visited))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency");
            return true;
        }
        checked.insert(first->_lastProducer);
    }
    for (auto & [first,second] : this->_externalBuffers)
    {
        if (!first->_lastProducer||checked.contains(first->_lastProducer)) continue;

        if (this->hasCircle(first->_lastProducer, visited))
        {
            LOGE("RDG: RenderDependencyGraph has circle dependency");
            return true;
        }
        checked.insert(first->_lastProducer);
    }
    return false;
}
void                   RDG::RenderDependencyGraph::prepareResource() {

    //textures, buffers, fbo, render pass,
    for (auto& pass : this->_rdgPasses) {
        if (pass->_isClipped) continue; // Skip already clipped passes
        
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