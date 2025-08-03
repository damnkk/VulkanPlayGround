#ifndef RDGPASSES_HPP
#define RDGPASSES_HPP

#include "RDGShaderParameters.hpp"
#include "ShaderManager.h"
#include "nvvk/pipeline_vk.hpp"
#include "vulkan/vulkan.h"
#include "RDGResources.h"
#include "RDG.h"

namespace Play {
namespace RDG {

class RDGPass
{
   public:
    RDGPass(uint8_t passType, std::string name = "");
    RDGPass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType, std::optional<uint32_t> passID = std::nullopt,
            std::string name = "");
    RDGPass(const RDGPass&) = delete;
    virtual ~RDGPass() {};
    void         setShaderParameters(const std::shared_ptr<RDGShaderParameters> shaderParameters);
    virtual void prepareResource() ;

   protected:
    RenderDependencyGraph* _hostGraph = nullptr;
    friend class RenderDependencyGraph;
    bool                                 _isClipped = true;
    PassType                             _passType;
    std::string                          _name;
    std::shared_ptr<RDGShaderParameters> _shaderParameters;
    std::set<RDGPass*>                _dependencies;
    std::optional<uint32_t>               _passID = std::nullopt;
};

/*
    So called _executeFunction is a lambda function that include the total render logic in this
   pass,is may include a scene or some geometry data, shader ref as parameters
*/
template<typename FLambdaFunction>
class RDGRenderPass:public RDGPass{
public:
    RDGRenderPass(std::shared_ptr<RDGShaderParameters> shaderParameters,std::optional<uint32_t> passID, std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, uint8_t(PassType::eRenderPass), passID, std::move(name)),_executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
        _RTSlots.reserve(16);
        updateResourceAccessState();
    }
    ~RDGRenderPass() override;
    void prepareResource() override;
    void execute() {
        if (_executeFunction) {
            _executeFunction();
        }
    }
    void addSlot(RDGTexture* rtTexture,RDGTexture* resolveTexture,RDGRTState::LoadType loadType = RDGRTState::LoadType::eDontCare, RDGRTState::StoreType storeType = RDGRTState::StoreType::eStore);
    void setPipelineState(const RDGGraphicPipelineState& pipelineState) {
        _pipelineState = pipelineState;
    }

    void updateResourceAccessState(){
        if(!_shaderParameters){
            LOGE("RDG: Shader parameters is not set for RDGPass");
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

protected:
   friend class RenderDependencyGraph;
private:
    VkRenderPass _renderPass;
    VkFramebuffer _frameBuffer;
    std::vector<VkDescriptorSet> _descriptorSets;
    RDGGraphicPipelineState _pipelineState;
    FLambdaFunction _executeFunction;
    std::vector<RDGRTState> _RTSlots;
};

template<typename FLambdaFunction>
class RDGComputePass:public RDGPass{
public:
    RDGComputePass(std::shared_ptr<RDGShaderParameters> shaderParameters, std::optional<uint32_t> passID, std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, uint8_t(PassType::eComputePass), passID, std::move(name)), _executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
    }
    ~RDGComputePass() override;
    void prepareResource() override;
    void execute() {
        if (_executeFunction) {
            _executeFunction();
        }
    }
protected:
    friend class RenderDependencyGraph;
private:
    FLambdaFunction _executeFunction;
};

template <typename FLambdaFunction>
RDGRenderPass<FLambdaFunction>::~RDGRenderPass()
{
    vkDestroyFramebuffer(_hostGraph->getApp()->getDevice(), _frameBuffer, nullptr);
}
template <typename FLambdaFunction>
void RDGRenderPass<FLambdaFunction>::prepareResource()
{
    RDGPass::prepareResource();
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

template <typename FLambdaFunction>
void RDGRenderPass<FLambdaFunction>::addSlot(RDGTexture* rtTexture, RDGTexture* resolveTexture,
                                             RDGRTState::LoadType  loadType,
                                             RDGRTState::StoreType storeType)
{
    NV_ASSERT(rtTexture);
    _RTSlots.emplace_back(loadType, storeType, rtTexture, resolveTexture);
}

template <typename FLambdaFunction>
RDGComputePass<FLambdaFunction>::~RDGComputePass()
{
}

template <typename FLambdaFunction>
void RDGComputePass<FLambdaFunction>::prepareResource()
{
    
}

} // namespace RDG
} // namespace Play

#endif // RDGPASSES_HPP