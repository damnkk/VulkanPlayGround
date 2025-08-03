#ifndef RDG_RESOURCES_H
#define RDG_RESOURCES_H
#include "Resource.h"
#include "RDGHandle.h"
#include "nvvk/pipeline_vk.hpp"
#include "ShaderManager.h"

namespace Play::RDG{
class RDGPass;
class RDGTexturePool;
class RenderDependencyGraph;

// 基类，提供通用的生产者/消费者追踪功能
class RDGResourceBase {
public:
    virtual ~RDGResourceBase() = default;
    const std::vector<std::optional<uint32_t>>& getProducers() const { return _producers; }
    const std::vector<std::optional<uint32_t>>& getReaders() const { return _readers; }
    std::optional<uint32_t> getLastProducer(uint32_t currentPassIdx) const;
    std::optional<uint32_t> getLastReader(uint32_t currentPassIdx) const;
    bool isExternal = false;
    
protected:
    std::vector<std::optional<uint32_t>> _producers;
    std::vector<std::optional<uint32_t>> _readers;
    friend class RenderDependencyGraph;
};

class RDGTexture : public RDGResourceBase {
public:
    RDGTexture() = default;
    RDGTexture(TextureHandle handle) :_handle(handle){}
    RDGTexture(Texture* texture) : _pData(texture) {}
    RDGTexture(const RDGTexture&) = delete;
    RDGTexture& operator=(const RDGTexture&) = delete;
    ~RDGTexture() = default;
    void setMetaData(Texture::TexMetaData metadata) { this->_pData->_metadata = metadata; }
    Texture* RHI(){return _pData;}
    TextureHandle _handle = TextureHandle::Null;
protected:
    Texture* _pData= nullptr;
    friend class RDGTexturePool;
    friend class RenderDependencyGraph;
};

class RDGBuffer : public RDGResourceBase {
public:
    RDGBuffer() = default;
    RDGBuffer(BufferHandle handle) :_handle(handle){}
    RDGBuffer(Buffer* buffer) : _pData(buffer) {}
    RDGBuffer(const RDGBuffer&) = delete;
    RDGBuffer& operator=(const RDGBuffer&) = delete;
    ~RDGBuffer() = default;
    BufferHandle _handle = BufferHandle::Null;
    void setMetaData(Buffer::BufferMetaData metadata) { this->_pData->_metadata = metadata; }
    Buffer* RHI(){return _pData;}
protected:
    Buffer* _pData = nullptr;
    friend class RDGBufferPool;
    friend class RenderDependencyGraph;
};

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
    void setShaderInfo(const Play::ShaderType& shaderInfo);
    VkPipeline _pipeline;
    ShaderInfo* _cshaderInfo;
};




}// namespace Play::RDG

#endif // RDG_RESOURCES_H