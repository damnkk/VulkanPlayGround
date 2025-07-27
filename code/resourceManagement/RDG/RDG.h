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
#include "nvvk/images_vk.hpp"
#include "PlayApp.h"
#include "RDGResources.h"
#include "RDGShaderParameters.hpp"
#include "RDGPasses.hpp"
namespace Play{
namespace RDG{
// class PlayApp;
class RDGPass;
class RenderDependencyGraph;

class RDGGraphicPipelineState:public nvvk::GraphicsPipelineState{
public:
    RDGGraphicPipelineState() = default;
    RDGGraphicPipelineState(const RDGGraphicPipelineState& other)= default;
    ~RDGGraphicPipelineState() = default;

    RDGGraphicPipelineState& setVertexShaderInfo(const ShaderInfo& shaderInfo);
    RDGGraphicPipelineState& setFragmentShaderInfo(const ShaderInfo& shaderInfo);
    RDGGraphicPipelineState& setGeometryShaderInfo(const ShaderInfo& shaderInfo);
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


class RDGPass
{
   public:
    RDGPass(uint8_t passType, std::string name = "");
    RDGPass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType,
            std::string name = "");
    RDGPass(const RDGPass&) = delete;
    virtual ~RDGPass() {};
    void         setShaderParameters(const std::shared_ptr<RDGShaderParameters> shaderParameters);
    virtual void prepareResource() {};
    enum class PassType : uint8_t
    {
        eRenderPass,
        eComputePass,
    };

   protected:
    RenderDependencyGraph* _hostGraph = nullptr;
    friend class RenderDependencyGraph;
    bool                                 _isClipped = true;
    PassType                             _passType;
    std::string                          _name;
    std::shared_ptr<RDGShaderParameters> _shaderParameters;
    std::vector<RDGPass*>                _dependencies;
};

inline RDGPass::RDGPass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType,
                        std::string name)
{
    NV_ASSERT(shaderParameters);
    _shaderParameters = std::move(shaderParameters);
    _passType = static_cast<PassType>(passType);
    _name = std::move(name);
}
/*
    So called _executeFunction is a lambda function that include the total render logic in this
   pass,is may include a scene or some geometry data, shader ref as parameters
*/
template<typename FLambdaFunction>
class RDGRenderPass:public RDGPass{
public:
    RDGRenderPass(std::shared_ptr<RDGShaderParameters> shaderParameters, RDGGraphicPipelineState& pipelineState,uint8_t passType,std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, passType, std::move(name)),_executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
        NV_ASSERT(pipelineState._fshaderInfo&&pipelineState._vshaderInfo);
        _RTSlots.reserve(16);
    }
    ~RDGRenderPass() override;
    void prepareResource() override;
    void execute() {
        if (_executeFunction) {
            _executeFunction();
        }
    }
    void addSlot(RDGTexture* rtTexture,RDGTexture* resolveTexture,RDGRTState::LoadType loadType = RDGRTState::LoadType::eDontCare, RDGRTState::StoreType storeType = RDGRTState::StoreType::eStore);

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
    RDGComputePass(std::shared_ptr<RDGShaderParameters> shaderParameters, uint8_t passType,std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(shaderParameters, passType, std::move(name)), _executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
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

struct TextureDesc{
    VkFormat    _format      = VK_FORMAT_UNDEFINED;
    VkImageType _type        = VK_IMAGE_TYPE_2D;
    VkExtent3D _extent = {1, 1, 1};
    VkImageUsageFlags _usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageAspectFlags _aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSampleCountFlags _sampleCount = VK_SAMPLE_COUNT_1_BIT;
    uint32_t     _mipmapLevel = 1;
    uint32_t     _layerCount = 1;
    std::string _debugName;
};

struct BufferDesc{
    VkBufferUsageFlags _usageFlags= VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
    VkDeviceSize _size = 1;
    VkDeviceSize _range = VK_WHOLE_SIZE;
    Buffer::BufferMetaData::BufferLocation _location= Buffer::BufferMetaData::BufferLocation::eDeviceOnly;
    std::string _debugName;
};
class RenderDependencyGraph
{
public:
    RenderDependencyGraph();
    ~RenderDependencyGraph();
    template <typename PipelineState, typename LambdaFunction>
    void addPass(RDGShaderParameters& shaderParameters, PipelineState pipelineState,
                 LambdaFunction&& executeFunction, uint8_t passType = 0, std::string name = "");
    template <typename PipelineState, typename LambdaFunction>
    void addComputePass(RDGShaderParameters& shaderParameters, PipelineState pipelineState,
                        LambdaFunction&& executeFunction, std::string name = "");
    template <typename PipelineState, typename LambdaFunction>
    void addRenderPass(RDGShaderParameters& shaderParameters, PipelineState pipelineState,
                       LambdaFunction&& executeFunction, std::string name = "");
    void compile();
    void execute();

    RDGTexture* registExternalTexture(Texture* texture);
    RDGBuffer*  registExternalBuffer(Buffer* buffer);

    RDGTexture* createTexture(const TextureDesc& desc);
    RDGBuffer*  createBuffer(const BufferDesc& desc);

    void destroyTexture(TextureHandle handle);
    void destroyBuffer(BufferHandle handle);
    void destroyTexture(RDGTexture* texture);
    void destroyBuffer(RDGBuffer* buffer);

    PlayApp* getApp() const
    {
        return _app;
    }

protected:
    void onCreatePass(RDGPass* pass);
    void clipPasses();
    bool hasCircle();
    void prepareResource();

    void allocRHITexture(RDGTexture* texture);
    void allocRHIBuffer(RDGBuffer* buffer);

private:
    bool hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited);

protected:
    RDGTexturePool           _rdgTexturePool;
    RDGBufferPool            _rdgBufferPool;
    std::vector<int>         _rdgAvaliableTextureIndices;
    std::vector<int>         _rdgAvaliableBufferIndices;
    std::vector<RDGPass*>    _rdgPasses;
    std::vector<RDGPass*>    _clippedPasses;
    std::vector<RDGTexture*> _externalTextures;
    std::vector<RDGBuffer*>  _externalBuffers;
    PlayApp*                 _app = nullptr;
    template<typename FLambdaFunction>
    friend class RDGRenderPass;
};

template <typename PipelineState,typename LambdaFunction>
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


template <typename FLambdaFunction>
RDGRenderPass<FLambdaFunction>::~RDGRenderPass()
{
    vkDestroyFramebuffer(_hostGraph->getApp()->getDevice(), _frameBuffer, nullptr);
}
template <typename FLambdaFunction>
void RDGRenderPass<FLambdaFunction>::prepareResource()
{
    for(auto& accessTypeArray: _shaderParameters->_resources){
        for(auto& resourceState: accessTypeArray){
            for(auto& resource: resourceState._resources){
                if(resourceState._resourceType == RDGResourceState::ResourceType::eTexture){
                    NV_ASSERT(resource);
                    _hostGraph->allocRHITexture(static_cast<RDGTexture*>(resource));
                }else if(resourceState._resourceType == RDGResourceState::ResourceType::eBuffer){
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
} //namespace RDG
}// namespace Play

#endif // RDG_H