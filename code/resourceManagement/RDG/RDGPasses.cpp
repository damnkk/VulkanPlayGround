#include "RDGPasses.hpp"
#include "RDG.h"

namespace Play::RDG{
    
inline RDGPass::RDGPass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType, std::optional<uint32_t> passID, std::string name)
{
    NV_ASSERT(shaderParameters);
    _shaderParameters = std::move(shaderParameters);
    _passType = static_cast<PassType>(passType);
    _name = std::move(name);
    _passID = passID;
}

void RDGPass::updateResourceAccessState(){
    if(!_shaderParameters){
        LOGW("RDG: Shader parameters is not set for RDGPass");
        return;
    }
    for(auto& accessTypeArray: _shaderParameters->_resources){
        for(auto& resourceState: accessTypeArray){
            for(auto& resource: resourceState._resources){
                if(resourceState._resourceType == ResourceType::eTexture){
                    NV_ASSERT(resource);
                    RDGTexture* texture = static_cast<RDGTexture*>(resource);
                    switch (resourceState._accessType) {
                        case AccessType::eReadOnly:
                            texture->getReaders().push_back(_passID);
                            break;
                        case AccessType::eWriteOnly:
                            texture->getProducers().push_back(_passID);
                            break;
                        case AccessType::eReadWrite:
                            texture->getProducers().push_back(_passID);
                            texture->getReaders().push_back(_passID);
                            break;
                        default:
                            LOGE("RDG: Unknown access type for RDGTexture");
                            break;
                    }
                }
                else if(resourceState._resourceType == ResourceType::eBuffer){
                    NV_ASSERT(resource);
                    RDGBuffer* buffer = static_cast<RDGBuffer*>(resource);
                    switch (resourceState._accessType) {
                        case AccessType::eReadOnly:
                            buffer->getReaders().push_back(_passID);
                            break;
                        case AccessType::eWriteOnly:
                            buffer->getProducers().push_back(_passID);
                            break;
                        case AccessType::eReadWrite:
                            buffer->getProducers().push_back(_passID);
                            buffer->getReaders().push_back(_passID);
                            break;
                        default:
                            LOGE("RDG: Unknown access type for RDGBufer");
                            break;
                    }
                }
            }
        }
    }
}

void RDGPass::prepareResource(){
    if(!_shaderParameters){
        LOGW("RDG: Shader parameters is not set for RDGPass");
        return;
    }
    for(auto& accessTypeArray: _shaderParameters->_resources){
        for(auto& resourceState: accessTypeArray){
            for(auto& resource: resourceState._resources){
                if(resourceState._resourceType == ResourceType::eTexture){
                    NV_ASSERT(resource);
                    _hostGraph->allocRHITexture(static_cast<RDGTexture*>(resource));
                }else if(resourceState._resourceType == ResourceType::eBuffer){
                    NV_ASSERT(resource);
                    _hostGraph->allocRHIBuffer(static_cast<RDGBuffer*>(resource));
                }
            }
        }
    }
}

RDGRenderPass::~RDGRenderPass()
{
    vkDestroyFramebuffer(_hostGraph->getApp()->getDevice(), _frameBuffer, nullptr);

}

void RDGRenderPass::execute(){
        NV_ASSERT( _executeFunction());
}

void RDGRenderPass::setPipelineState(const RDGGraphicPipelineState& pipelineState){
        _pipelineState = pipelineState;
}

void RDGRenderPass::updateResourceAccessState(){
    RDGPass::updateResourceAccessState();
    for(auto& rt: _RTSlots){
        if(!rt.rtTexture){
            LOGE("RDG: Render target texture is invalid");
            return;
        }
        if(rt.rtTexture){
            if(rt.loadType==RDGRTState::LoadType::eLoad){
                rt.rtTexture->getReaders().push_back(_passID);
            }
            if(rt.storeType==RDGRTState::StoreType::eStore){
                rt.rtTexture->getProducers().push_back(_passID);
            }
        }
        if(rt.resolveTexture){
            NV_ASSERT(rt.resolveTexture);
            rt.resolveTexture->getReaders().push_back(_passID);
        }
    }
}

void RDGRenderPass::prepareResource()
{
    RDGPass::prepareResource();

    for(auto& rt: _RTSlots){
        if(rt.rtTexture){
            NV_ASSERT(rt.rtTexture);
            _hostGraph->allocRHITexture(rt.rtTexture);
        }
        if(rt.resolveTexture){
            NV_ASSERT(rt.resolveTexture);
            _hostGraph->allocRHITexture(rt.resolveTexture);
        }
    }
}

void RDGRenderPass::createVkRenderPass(){
    // this->_hostGraph
}
void RDGRenderPass::createVkFrameBuffer(){

}

void RDGRenderPass::addSlot(RDGTexture* rtTexture, RDGTexture* resolveTexture,
                                             RDGRTState::LoadType  loadType,
                                             RDGRTState::StoreType storeType)
{
    NV_ASSERT(rtTexture);
    _RTSlots.emplace_back(loadType, storeType, rtTexture, resolveTexture);
}

RDGComputePass::~RDGComputePass()
{
}

void RDGComputePass::prepareResource()
{
    RDGPass::prepareResource();
}

void RDGComputePass::execute(){
    NV_ASSERT(_executeFunction());
}

}