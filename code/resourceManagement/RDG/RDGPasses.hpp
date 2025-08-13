#ifndef RDGPASSES_HPP
#define RDGPASSES_HPP
#include <set>
#include <optional>
#include "RDGPreDefine.h"
#include "RDGShaderParameters.hpp"
#include "ShaderManager.h"
#include "nvvk/pipeline_vk.hpp"
#include "vulkan/vulkan.h"
#include "RDGResources.h"
namespace Play::RDG{
class RenderDependencyGraph;
class RDGPass
{
public:
    RDGPass(uint8_t passType, std::string name = "");
    RDGPass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType, std::optional<uint32_t> passID = std::nullopt,
            std::string name = "");
    RDGPass(const RDGPass&) = delete;
    virtual ~RDGPass() {};
    void         setShaderParameters(const std::shared_ptr<RDGShaderParameters> shaderParameters);
    virtual void prepareResource();

protected:
    virtual void updateResourceAccessState();
    RenderDependencyGraph* _hostGraph = nullptr;
    friend class RenderDependencyGraph;
    bool                                 _isClipped = true;
    PassType                             _passType;
    std::string                          _name;
    std::shared_ptr<RDGShaderParameters> _shaderParameters;
    std::set<RDGPass*>                   _dependencies;
    std::optional<uint32_t>              _passID = std::nullopt;
};

/*
    So called _executeFunction is a lambda function that include the total render logic in this
   pass,is may include a scene or some geometry data, shader ref as parameters
*/

using FLambdaFunction = std::function<bool()>;

class RDGRenderPass:public RDGPass{
public:
    RDGRenderPass(std::shared_ptr<RDGShaderParameters> shaderParameters,std::optional<uint32_t> passID, std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, uint8_t(PassType::eRenderPass), passID, std::move(name)),_executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
        _RTSlots.reserve(16);
    }
    ~RDGRenderPass() override;
    //setting func
    void addSlot(RDGTexture* rtTexture,RDGTexture* resolveTexture,RDGRTState::LoadType loadType = RDGRTState::LoadType::eDontCare, RDGRTState::StoreType storeType = RDGRTState::StoreType::eStore);
    void setPipelineState(const RDGGraphicPipelineState& pipelineState);
    void updateResourceAccessState() override;
    void prepareResource() override;
    void execute();
    //getter setter
    void setRenderPass(VkRenderPass renderPass);
    void setFramebuffer(VkFramebuffer frameBuffer);
    VkRenderPass getRenderPass() const{return _renderPass;};
    VkFramebuffer getFramebuffer() const{return _frameBuffer;};
protected:
    void createVkRenderPass();
    void createVkFrameBuffer();
    friend class RenderDependencyGraph;
private:
    VkRenderPass _renderPass;
    VkFramebuffer _frameBuffer;
    std::vector<VkDescriptorSet> _descriptorSets;
    RDGGraphicPipelineState _pipelineState;
    FLambdaFunction _executeFunction;
    std::vector<RDGRTState> _RTSlots;
};

class RDGComputePass:public RDGPass{
public:
    RDGComputePass(std::shared_ptr<RDGShaderParameters> shaderParameters, std::optional<uint32_t> passID, std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, uint8_t(PassType::eComputePass), passID, std::move(name)), _executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
    }
    ~RDGComputePass() override;
    void prepareResource() override;
    void execute();
protected:
    friend class RenderDependencyGraph;
    RDGComputePipelineState _pipelineState;
    std::vector<VkDescriptorSet> _descriptorSets;
private:
    FLambdaFunction _executeFunction;
};


} // namespace Play

#endif // RDGPASSES_HPP