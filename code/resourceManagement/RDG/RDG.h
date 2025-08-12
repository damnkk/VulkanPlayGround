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
#include "RDGPreDefine.h"
#include "vulkan/vulkan.h"
#include "nvvk/images_vk.hpp"
#include "PlayApp.h"
#include "RDGResources.h"
#include "RDGShaderParameters.hpp"
#include "RDGPasses.hpp"
namespace Play::RDG{

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
    VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;
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
    /*
      if the pass shader parameters have readAndWrite resource,that will flush the resource's lastproducer, so if you have several passes
      need the same input resource, the adding order is important. For instance, passX output a textureA, then you add passY to readAndWrite
      textureA, then you add passZ to read textureA to do something. as this order, passZ will get passY's output, if you want passX's "textureA"
      this will lead a mistake, so you should add passZ before passY.
    */
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
    RDGTexture* allocRHITexture(RDGTexture* texture);
    RDGBuffer*  allocRHIBuffer(RDGBuffer* buffer);

    void destroyTexture(TextureHandle handle);
    void destroyBuffer(BufferHandle handle);
    void destroyTexture(RDGTexture* texture);
    void destroyBuffer(RDGBuffer* buffer);

    PlayApp* getApp() const{ return _app; }

protected:
    void onCreatePass(RDGPass* pass);
    void clipPasses();
    bool hasCircle();
    void prepareResource();
    void updatePassDependency();

    VkRenderPass getOrCreateRenderPass(std::vector<RDGRTState>& rtStates);
    void getOrCreatePipeline(RDGGraphicPipelineState& pipelineState,VkRenderPass renderPass);
    void getOrCreatePipeline(RDGComputePipelineState& pipelineState);

private:
    bool hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited,int currDepth);
    RDGTexturePool           _rdgTexturePool;
    RDGBufferPool            _rdgBufferPool;
    std::vector<int>         _rdgAvaliableTextureIndices;
    std::vector<int>         _rdgAvaliableBufferIndices;
    std::vector<RDGPass*>    _rdgPasses;
    std::vector<RDGPass*>    _clippedPasses;
    std::vector<RDGTexture*> _externalTextures;
    std::vector<RDGBuffer*>  _externalBuffers;
    PlayApp*                 _app = nullptr;
    std::vector<int32_t> _passDepthLayout;
    std::optional<uint32_t> _passCounter = 0;
    friend class RDGRenderPass;
};

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

template <typename PipelineState,typename LambdaFunction>
void Play::RDG::RenderDependencyGraph::addPass(
    RDGShaderParameters& shaderParameters, PipelineState pipelineState, LambdaFunction&& executeFunction, uint8_t passType, std::string name)
{
    RDGPass* pass = nullptr;
    if (passType == PassType::eRenderPass) {
        pass = new RDGRenderPass(shaderParameters, this->_passCounter.value()++, std::move(name), std::forward<LambdaFunction>(executeFunction));
    } else {
        pass = new RDGComputePass(shaderParameters, this->_passCounter.value()++, std::move(name), std::forward<LambdaFunction>(executeFunction));
    }
    // createPass(pass);
    _rdgPasses.push_back(pass);
}
}// namespace Play

#endif // RDG_H