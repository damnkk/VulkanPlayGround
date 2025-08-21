#include "RDGPasses.hpp"
#include "RDG.h"

namespace Play::RDG{
    
RDGPass::RDGPass(std::optional<uint32_t> passID, std::string name):_name(name),_passID(passID){
}

void RDGPass::addRead(RDGTexture* texture, uint32_t setIdx,  uint32_t bindIdx, uint32_t offset){
    _resourceMap[static_cast<size_t>(AccessType::eReadOnly)].emplace_back(texture, setIdx, bindIdx, offset);
}

void RDGPass::addRead(RDGBuffer* buffer, uint32_t setIdx, uint32_t bindIdx, uint32_t offset){
    _resourceMap[static_cast<size_t>(AccessType::eReadOnly)].emplace_back(buffer, setIdx, bindIdx, offset);
}

void RDGPass::addReadWrite(RDGTexture* texture, uint32_t setIdx, uint32_t bindIdx, uint32_t offset){
    _resourceMap[static_cast<size_t>(AccessType::eReadWrite)].emplace_back(texture, setIdx, bindIdx, offset);
}

void RDGPass::addReadWrite(RDGBuffer* buffer, uint32_t setIdx, uint32_t bindIdx, uint32_t offset){
    _resourceMap[static_cast<size_t>(AccessType::eReadWrite)].emplace_back(buffer, setIdx, bindIdx, offset);
}

void RDGPass::updateDependency(){
    for(auto descInfo:_resourceMap[static_cast<size_t>(AccessType::eReadOnly)]) {
        if(descInfo.resource) {
            descInfo.resource->getReaders().push_back(_passID);
        }
    }

    for(auto descInfo : _resourceMap[static_cast<size_t>(AccessType::eReadWrite)]) {
        if(descInfo.resource) {
            descInfo.resource->getProducers().push_back(_passID);
        }
    }
}

void RDGRenderPass::colorAttach(RDGRTState::LoadType loadType,RDGRTState::StoreType storeType,
                                RDGTexture* texture, RDGTexture* resolveTexture,
                                VkImageLayout initLayout, VkImageLayout finalLayout){
    _RTSlots.emplace_back(loadType, storeType, texture, resolveTexture, initLayout, finalLayout);
}

void RDGRenderPass::colorAttach(RDGRTState& state){
    _RTSlots.push_back(state);
}

void RDGRenderPass::depthStencilAttach(RDGRTState::LoadType loadType,RDGRTState::StoreType storeType,
                                        RDGTexture* texture, VkImageLayout initLayout, VkImageLayout finalLayout){
    _RTSlots.emplace_back(loadType, storeType, texture,nullptr, initLayout, finalLayout);
}

void RDGRenderPass::executeFunction(FLambdaFunction&& func){
    _executeFunction = std::forward<FLambdaFunction>(func);
}

void RDGRenderPass::execute(){
    prepareDescriptors();
    preparePipeline();
    prepareRenderPass();
    _executeFunction();
}

}