/*
For increase multi-pass rendering development efficiency,and performance,I designed a simple render dependency graph.
for the first development stage, the graph can not be modified at runtime and only support single threaded execution(or not).
Usage of the graph shoule as simple as possible,only shader needed and resource specified in the pass.And dependency between passes
should be specified by the resource usage.The design must considering RT resuse in the future,so, a texture handle wrap is needed. we
use resource handle as resource itself,to confirm the dependency, and specify the real GPU resource when RDG compile.
 */
#ifndef RDG_H
#define RDG_H
#include "string"
#include "unordered_set"
#include "functional"
#include "array"
#include "list"
#include "vulkan/vulkan.h"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "ShaderManager.h"
#include "PlayApp.h"
#include "RDGResources.h"
namespace Play{
namespace RDG{

class PlayApp;

class RDGPass;
class RDGResourceHandle{
public:
    enum class ResourceType:int32_t{
        eSRVTexture=1,
        eSRVTextureArray,
        eUAVTexture,
        eUAVTextureArray,
        eAttachmentTexture,
        eTextureTypeEnd,
        eSRVBuffer,
        eDynamicSRVBuffer,
        eUAVBuffer,
        eUAVBufferArray,
        eIndirectBuffer,
        eBufferTypeEnd,
        eTLAS,
        eUndefeined
    };
    RDGResourceHandle() = default;

    RDGResourceHandle(int32_t handle, ResourceType resourceType)
        :_resourceType(resourceType),_handle(handle) {}
    
    ResourceType _resourceType;
    bool isTexture() const { return _resourceType >= ResourceType::eSRVTexture && _resourceType < ResourceType::eTextureTypeEnd; }
    bool isBuffer() const { return _resourceType >= ResourceType::eSRVBuffer && _resourceType < ResourceType::eBufferTypeEnd; }
    bool isArray() const { return _resourceType == ResourceType::eSRVTextureArray || _resourceType == ResourceType::eUAVTextureArray || _resourceType == ResourceType::eUAVBufferArray; }
    bool isValid(){return _handle != -1&&_resourceType!=ResourceType::eUndefeined;}
    void invalidate() { _handle = -1; _resourceType = ResourceType::eUndefeined; }
    int32_t getHandle() const { return _handle; }
private:
    int32_t _handle;
};

class RDGResourceState{
 public:
    enum class AccessType{
        eReadOnly, //uniform buffer, sampled texture
        eWriteOnly, //most used for color attachments, depth attachments, msaa attachments, shading rate resolve attachments
        eReadWrite, //storage buffer, storage texture
        eCount
    };
    enum class AccessStage{
        eVertexShader = 1 << 0,
        eFragmentShader = 1 << 1,
        eComputeShader = 1 << 2,
        eRayTracingShader = 1 << 3,
        eAllGraphics = eVertexShader | eFragmentShader | eComputeShader,
        eAllCompute = eComputeShader | eRayTracingShader,
        eAll = eAllGraphics | eAllCompute
    };
    AccessType _accessType;
    AccessStage _accessStage;
    RDGResourceHandle _resourceHandle;
};

struct RDGShaderParameters{
    bool addResource(RDGResourceHandle resource, RDGResourceState::AccessType accessType, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    std::array<std::vector<RDGResourceState>, static_cast<size_t>(RDGResourceState::AccessType::eCount)> _resources;
};

class RDGGraphicPipelineState:public nvvk::GraphicsPipelineState{
public:
    RDGGraphicPipelineState() = default;
    RDGGraphicPipelineState(const RDGGraphicPipelineState& other)= default;
    ~RDGGraphicPipelineState() = default;
    
    void setShaderInfo(const ShaderInfo& shaderInfo);
    VkPipeline _pipeline;
    ShaderInfo* _vshaderInfo;
    ShaderInfo* _fshaderInfo;
    ShaderInfo* _gshaderInfo;
};

class RDGComputePipelineState{
public: 
    RDGComputePipelineState() = default;
    RDGComputePipelineState(const RDGComputePipelineState& other)= default;
    ~RDGComputePipelineState() = default;
    void setShaderInfo(const ShaderType& shaderInfo);
    VkPipeline _pipeline;
    ShaderInfo* _cshaderInfo;
};


class RDGPass{
public:
    RDGPass(uint8_t passType,std::string name="");
    RDGPass(RDGShaderParameters& shaderParameters, uint8_t passType,std::string name="");
    RDGPass(const RDGPass&) = delete;
    void setShaderParameters(const RDGShaderParameters& shaderParameters);
    virtual void prepareResource();
    enum class PassType:uint8_t{
        eRenderPass,
        eComputePass,
    };
protected:
    RenderDependencyGraph* _hostGraph = nullptr;
private:
    friend class RenderDependencyGraph;
    bool _isClipped= true;
    PassType _passType;
    std::string _name;
    RDGShaderParameters* _shaderParameters;
    std::vector<RDGPass*> _dependencies;
};


/*
    So called _executeFunction is a lambda function that include the total render logic in this pass,is may include a scene or some geometry data, shader ref as 
    parameters
 */
template<typename FLambdaFunction>
class RDGRenderPass:public RDGPass{
public:
    RDGRenderPass(RDGShaderParameters& shaderParameters, RDGGraphicPipelineState& pipelineState,uint8_t passType,std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, passType, std::move(name)), _pipelineState(pipelineState), _executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
        int RTCnt = 0;
        for(auto& resourceState:shaderParameters._resources[static_cast<size_t>(RDGResourceState::AccessType::eWriteOnly)]) {
            if(!resourceState._resourceHandle.isTexture()||resourceState._resourceHandle.isArray()) continue;
            NV_ASSERT(RTCnt < _RTSlots.size());
            _RTSlots[RTCnt++] = resourceState._resourceHandle;
        }
    }
    void prepareResource() override;
    void execute() {
        if (_executeFunction) {
            _executeFunction();
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
    std::array<RDGResourceHandle,32> _RTSlots;
};

template<typename FLambdaFunction>
class RDGComputePass:public RDGPass{
public:
    RDGComputePass(RDGShaderParameters& shaderParameters, uint8_t passType,std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, passType, std::move(name)), _executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
    }
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

class RDGTexturePool : public BasePool<RDGTexture>
{
public:
    void init(uint32_t poolSize);
    void deinit();
    RDGTexture* alloc();
    void destroy(TextureHandle handle);
    void destroy(RDGTexture* texture);
    RDGTexture* operator[](TextureHandle handle) const {
        if (!handle.isValid() || handle.index >= _objs.size()) {
            throw std::runtime_error("RDGTexturePool: Invalid texture handle");
        }
        return _objs[handle.index];
    }
private:
    using BasePool<RDGTexture>::init;
};

class RDGBufferPool : public BasePool<RDGBuffer>
{
public:
    void init(uint32_t poolSize);
    void deinit();
    RDGBuffer* alloc();
    void destroy(BufferHandle handle);
    void destroy(RDGBuffer* buffer);
    RDGBuffer* operator[](BufferHandle handle) const {
        if (!handle.isValid() || handle.index >= _objs.size()) {
            throw std::runtime_error("RDGBufferPool: Invalid buffer handle");
        }
        return _objs[handle.index];
    }
private:
    using BasePool<RDGBuffer>::init;
};

class RenderDependencyGraph{
public:
    RenderDependencyGraph();
    ~RenderDependencyGraph();
    template<typename PipelineState,typename LambdaFunction>
    void addPass(RDGShaderParameters& shaderParameters, PipelineState pipelineState,LambdaFunction&& executeFunction, uint8_t passType = 0,std::string name="");
    template<typename PipelineState,typename LambdaFunction>
    void addComputePass(RDGShaderParameters& shaderParameters,PipelineState pipelineState, LambdaFunction&& executeFunction, std::string name = "");
    template<typename PipelineState,typename LambdaFunction>
    void addRenderPass(RDGShaderParameters& shaderParameters, PipelineState pipelineState, LambdaFunction&& executeFunction, std::string name = "");
    void compile();
    void execute();
    
    RDGResourceHandle registExternalTexture(Texture* texture);
    RDGResourceHandle registExternalBuffer(Buffer* buffer);

    void destroyTexture(TextureHandle handle);
    void destroyBuffer(BufferHandle handle);

protected:
    void onCreatePass(RDGPass* pass);
    void clipPasses();
    bool hasCircle();
    void prepareResource();

private:
    bool hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited);

protected:
    RDGTexturePool _rdgTexturePool;
    RDGBufferPool _rdgBufferPool;
    std::vector<int> _rdgAvaliableTextureIndices;
    std::vector<int> _rdgAvaliableBufferIndices;
    std::vector<RDGPass*> _rdgPasses;
    std::vector<RDGPass*> _clippedPasses;
    std::unordered_map<RDGTexture*, RDGResourceHandle> _externalTextures;
    std::unordered_map<RDGBuffer*, RDGResourceHandle> _externalBuffers;
    PlayApp* _app = nullptr;
};

template<typename PipelineState,typename LambdaFunction>
void Play::RDG::RenderDependencyGraph::addPass(
    RDGShaderParameters& shaderParameters, PipelineState pipelineState, LambdaFunction&& executeFunction, uint8_t passType, std::string name)
{
    RDGPass* pass = nullptr;
    if (passType == 0) {
        pass = new RDGRenderPass(shaderParameters, passType, std::move(name), std::forward<LambdaFunction>(executeFunction));
    } else {
        pass = new RDGComputePass(shaderParameters, passType, std::move(name), std::forward<LambdaFunction>(executeFunction));
    }
    // createPass(pass);
    _rdgPasses.push_back(pass);
}


template<typename PipelineState,typename LambdaFunction>
void Play::RDG::RenderDependencyGraph::addComputePass(
    RDGShaderParameters& shaderParameters, PipelineState pipelineState, LambdaFunction&& executeFunction, std::string name)
{
    addPass(shaderParameters, pipelineState,std::forward<LambdaFunction>(executeFunction), 1, std::move(name));
}

template<typename PipelineState,typename LambdaFunction>
void Play::RDG::RenderDependencyGraph::addRenderPass(
    RDGShaderParameters& shaderParameters, PipelineState pipelineState, LambdaFunction&& executeFunction, std::string name)
{
    addPass(shaderParameters,pipelineState,std::forward<LambdaFunction>(executeFunction), 0, std::move(name));
}


} //namespace RDG
}// namespace Play

#endif // RDG_H