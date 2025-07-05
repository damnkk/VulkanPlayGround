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
#include "Resource.h"
#include "PlayApp.h"
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

class RDGTexture{
public:
    friend class RenderDependencyGraph;
    friend class RDGTextureDescriptionPool;
    RDGTexture() = default;
    RDGTexture(Texture* texture) : _pData(texture) {}
    RDGTexture(const RDGTexture&) = delete;
    RDGTexture& operator=(const RDGTexture&) = delete;
    ~RDGTexture() = default;
    std::shared_ptr<RDGTexture> _next;
private:
    Texture* _pData;
};

class TextureStates:public RDGResourceState{
   
};

class RDGBuffer{
public:
    friend class RenderDependencyGraph;
    friend class RDGBufferDescriptionPool;
    RDGBuffer() = default;
    RDGBuffer(Buffer* buffer) : _pData(buffer) {}
    RDGBuffer(const RDGBuffer&) = delete;
    RDGBuffer& operator=(const RDGBuffer&) = delete;
    ~RDGBuffer() = default;
    std::shared_ptr<RDGBuffer> _next;
private:
    Buffer* _pData;
};

class BufferStates:public RDGResourceState{

};

class RDGTextureDescriptionPool;
class RDGTextureDescription{
public:
    RDGTextureDescription() = default;
    ~RDGTextureDescription() = default;
    
    VkFormat getFormat() const { return _format; }
    VkImageType getType() const { return _type; }
    VkExtent3D getExtent() const { return _extent; }
    VkImageUsageFlags getUsageFlags() const { return _usageFlags; }
    VkImageAspectFlags getAspectFlags() const { return _aspectFlags; }
    int getSampleCount() const { return _sampleCount; }
private:
    friend class RDGTextureDescriptionPool;
    friend class RenderDependencyGraph;
    VkFormat _format;
    VkImageType _type;
    VkExtent3D _extent;
    VkImageUsageFlags _usageFlags;
    VkImageAspectFlags _aspectFlags;
    RDGPass* _lastProducer = nullptr;
    int _sampleCount = 1;
    bool _isExternalResource = false;
    std::shared_ptr<RDGTexture> _texture = nullptr;
    int32_t _textureCnt = 1;
    std::string _debugName;
};

class RDGBufferDescriptionPool;
class RDGBufferDescription{
public:
    VkBufferUsageFlags getUsageFlags() const { return _usageFlags; }
    VkDeviceSize getSize() const { return _size; }
    enum class BufferLocation {
        eHostOnly,
        eHostVisible,
        eDeviceOnly,
        eCount
    };
private:
    friend class RDGBufferDescriptionPool;
    friend class RenderDependencyGraph;
    VkBufferUsageFlags _usageFlags;
    VkDeviceSize _size;
    VkDeviceSize _range = VK_WHOLE_SIZE;
    RDGPass* _lastProducer = nullptr;
    BufferLocation _location;
    std::string _debugName;
    std::shared_ptr<RDGBuffer> _buffer;
    bool _isExternalResource = false;
    int32_t _bufferCnt;
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
    std::vector<TextureStates> _textureStates;
    std::vector<BufferStates> _bufferStates;
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

class RDGTextureDescriptionPool : public BasePool<RDGTextureDescription>
{
public:
    void init(uint32_t poolSize);
    void deinit();
    RDGResourceHandle alloc();
    void destroy(RDGResourceHandle handle);
    RDGTextureDescription* operator[](RDGResourceHandle handle) const {
        if (!handle.isValid() || handle.getHandle() >= _objs.size()) {
            throw std::runtime_error("RDGTextureDescriptionPool: Invalid texture description handle");
        }
        return _objs[handle.getHandle()];
    }
    
private:
    using BasePool<RDGTextureDescription>::init;
};

class RDGBufferDescriptionPool : public BasePool<RDGBufferDescription>
{
public:
    void init(uint32_t poolSize);
    void deinit();
    RDGResourceHandle alloc();
    void destroy(RDGResourceHandle handle);
    RDGBufferDescription* operator[](RDGResourceHandle handle) const {
        if (!handle.isValid() || handle.getHandle() >= _objs.size()) {
            throw std::runtime_error("RDGBufferDescriptionPool: Invalid buffer description handle");
        }
        return _objs[handle.getHandle()];
    }
    
private:
    using BasePool<RDGBufferDescription>::init;
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
    RDGResourceHandle createTexture(std::string name,VkFormat format,VkImageType type,VkExtent3D extent,VkImageUsageFlags usageFlags,VkImageAspectFlags aspectFlags,int textureCount,int sampleCount);
    RDGResourceHandle createTexture2D(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usageFlags,int textureCount=1);
    RDGResourceHandle createTexture2D(const std::string& name, VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usageFlags,int textureCount);
    RDGResourceHandle createColorTarget(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
    RDGResourceHandle createColorTarget(const std::string& name, uint32_t width, uint32_t height, VkFormat format);
    RDGResourceHandle createDepthTarget(uint32_t width, uint32_t height, VkFormat format ,VkSampleCountFlagBits sampleCnt);
    RDGResourceHandle createDepthTarget(const std::string& name, uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT,VkSampleCountFlagBits sampleCnt= VK_SAMPLE_COUNT_1_BIT);
    RDGResourceHandle createComputeTexture2D(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,int textureCount = 1);
    RDGResourceHandle createComputeTexture2D(const std::string& name, uint32_t width, uint32_t height, VkFormat format, int textureCount);
    RDGResourceHandle createTexture3D(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usageFlags,int textureCount = 1);
    RDGResourceHandle createTexture3D(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usageFlags,int textureCount);
    using MSAALevel = VkSampleCountFlagBits;
    RDGResourceHandle createMSAATexture2D(uint32_t width, uint32_t height, VkFormat format, MSAALevel samples);
    RDGResourceHandle createMSAATexture2D(const std::string& name, uint32_t width, uint32_t height, VkFormat format, MSAALevel samples);
    RDGResourceHandle createTextureLike(RDGResourceHandle reference, VkFormat format, VkImageUsageFlags usageFlags,int TextureCount = 1);
    RDGResourceHandle createTextureLike(const std::string& name, RDGResourceHandle reference, VkFormat format, VkImageUsageFlags usageFlags,int TextureCount);

    RDGResourceHandle registExternalTexture(Texture* texture);
    RDGResourceHandle registExternalBuffer(Buffer* buffer);

    void destroyTexture(RDGResourceHandle handle);
    void destroyBuffer(RDGResourceHandle handle);
    
    RDGResourceHandle createBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usageFlags, RDGBufferDescription::BufferLocation location,VkDeviceSize range,int bufferCount);
    RDGResourceHandle createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, RDGBufferDescription::BufferLocation location = RDGBufferDescription::BufferLocation::eDeviceOnly,VkDeviceSize range = VK_WHOLE_SIZE,int bufferCount = 1);

    // Uniform缓冲便利接口 - 作为UBO描述符绑定
    RDGResourceHandle createUniformBuffer(VkDeviceSize size,VkDeviceSize range = VK_WHOLE_SIZE,RDGBufferDescription::BufferLocation location = RDGBufferDescription::BufferLocation::eHostVisible);
    RDGResourceHandle createUniformBuffer(const std::string& name, VkDeviceSize size,VkDeviceSize range, RDGBufferDescription::BufferLocation location);
    template<typename T>
    RDGResourceHandle createUniformBuffer(const T& data);
    template<typename T>
    RDGResourceHandle createUniformBuffer(const std::string& name, const T& data);
    // 动态缓冲接口 - 每帧更新的描述符缓冲
    RDGResourceHandle createDynamicUniformBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags,VkDeviceSize range = VK_WHOLE_SIZE);
    RDGResourceHandle createDynamicUniformBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usageFlags,VkDeviceSize range);

    // 存储缓冲便利接口 - 作为SSBO描述符绑定（计算着色器常用）
    RDGResourceHandle createStorageBuffer(VkDeviceSize size,int bufferCount = 1);
    RDGResourceHandle createStorageBuffer(const std::string& name, VkDeviceSize size,int bufferCount);
    template<typename T>
    RDGResourceHandle createStorageBuffer(const std::vector<T>& data);
    template<typename T>
    RDGResourceHandle createStorageBuffer(const std::string& name, const std::vector<T>& data);

protected:
    void onCreatePass(RDGPass* pass);
    void createTextureByDescription(const RDGTextureDescription& description);
    void createBufferByDescription(const RDGBufferDescription& description);
    RDGTextureDescription* getTextureDescription(RDGResourceHandle handle) const;
    RDGBufferDescription* getBufferDescription(RDGResourceHandle handle) const;
    RDGResourceHandle::ResourceType inferResourceTypeFromImageUsage(VkImageUsageFlags usage, int textureCount);
    RDGResourceHandle::ResourceType inferResourceTypeFromBufferUsage(RDG::RDGBufferDescription& bufferDesc, int bufferCount);
    void clipPasses();
    bool hasCircle();
    void prepareResource();

private:
    bool hasCircle(RDGPass* pass, std::unordered_set<RDGPass*>& visited);

protected:
    RDGTextureDescriptionPool _rdgTexturePool;
    RDGBufferDescriptionPool _rdgBufferPool;
    std::vector<int> _rdgAvaliableTextureIndices;
    std::vector<int> _rdgAvaliableBufferIndices;
    std::vector<RDGPass*> _rdgPasses;
    std::vector<RDGPass*> _clippedPasses;
    std::unordered_map<Texture*, RDGResourceHandle> _externalTextures;
    std::unordered_map<Buffer*, RDGResourceHandle> _externalBuffers;
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

template <typename T>
Play::RDG::RDGResourceHandle Play::RDG::RenderDependencyGraph::createUniformBuffer(const T& data)
{

}

template <typename T>
Play::RDG::RDGResourceHandle Play::RDG::RenderDependencyGraph::createUniformBuffer(
    const std::string& name, const T& data)
{
}

template <typename T>
Play::RDG::RDGResourceHandle Play::RDG::RenderDependencyGraph::createStorageBuffer(
    const std::vector<T>& data)
{
}

template <typename T>
Play::RDG::RDGResourceHandle Play::RDG::RenderDependencyGraph::createStorageBuffer(
    const std::string& name, const std::vector<T>& data)
{
}

} //namespace RDG
}// namespace Play

#endif // RDG_H