#ifndef RDG_RESOURCES_H
#define RDG_RESOURCES_H
#include "Resource.h"
#include "RDGHandle.h"
#include "nvvk/pipeline.hpp"
#include "nvvk/graphics_pipeline.hpp"
#include "nvvk/compute_pipeline.hpp"
#include "nvvk/shaders.hpp"
#include "ShaderManager.hpp"
#include <optional>

namespace Play::RDG{

class RDGTexture;
class RDGBuffer;
using TextureHandle = RDGHandle<RDGTexture, uint32_t>;
using BufferHandle = RDGHandle<RDGBuffer, uint32_t>;
using TextureHandleArray = std::vector<TextureHandle>;
using BufferHandleArray = std::vector<BufferHandle>;

class RDGPass;
class RDGTexturePool;
class RenderDependencyGraph;

// 基类，提供通用的生产者/消费者追踪功能
class RDGResourceBase {
public:
    virtual ~RDGResourceBase() = default;
    std::vector<std::optional<uint32_t>>& getProducers() { return _producers; }
    std::vector<std::optional<uint32_t>>& getReaders() { return _readers; }
    std::optional<uint32_t> getLastProducer(uint32_t currentPassIdx) const;
    std::optional<uint32_t> getLastReader(uint32_t currentPassIdx) const;
    std::optional<uint32_t> getLastProducer() const;
    std::optional<uint32_t> getLastReader() const;
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
    RDGTexture(Texture* texture) : _rhi(texture) {}
    RDGTexture(const RDGTexture&) = delete;
    RDGTexture& operator=(const RDGTexture&) = delete;
    ~RDGTexture() = default;
    void setMetaData(Texture::TexMetaData metadata) { this->_rhi->setMetaData(metadata); }
    Texture* RHI(){return _rhi;}
    TextureHandle _handle = TextureHandle::Null;
protected:
    Texture* _rhi = nullptr;
    friend class RDGTexturePool;
    friend class RenderDependencyGraph;
};

class RDGBuffer : public RDGResourceBase {
public:
    RDGBuffer() = default;
    RDGBuffer(BufferHandle handle) :_handle(handle){}
    RDGBuffer(Buffer* buffer) : _rhi(buffer) {}
    RDGBuffer(const RDGBuffer&) = delete;
    RDGBuffer& operator=(const RDGBuffer&) = delete;
    ~RDGBuffer() = default;
    BufferHandle _handle = BufferHandle::Null;
    void setMetaData(Buffer::BufferMetaData metadata) { this->_rhi->setMetaData(metadata); }
    Buffer* RHI(){return _rhi;}
protected:
    Buffer* _rhi = nullptr;
    friend class RDGBufferPool;
    friend class RenderDependencyGraph;
};

class RDGGraphicPipelineState:public nvvk::GraphicsPipelineState{
public:
    RDGGraphicPipelineState() = default;
    RDGGraphicPipelineState(const RDGGraphicPipelineState& other)= default;
    ~RDGGraphicPipelineState() = default;
    RDGGraphicPipelineState& setVertexShaderInfo(uint32_t shaderId);
    RDGGraphicPipelineState& setFragmentShaderInfo(uint32_t shaderId);
    VkPipeline _pipeline = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    uint32_t _vshaderId = size_t(~0);
    uint32_t _fshaderId = size_t(~0);
    void test(){

    }

};

class RDGComputePipelineState{
public: 
    RDGComputePipelineState() = default;
    RDGComputePipelineState(const RDGComputePipelineState& other)= default;
    ~RDGComputePipelineState() = default;
    void setShaderInfo(uint32_t shaderId);
    VkPipeline _pipeline;
    VkPipelineLayout _pipelineLayout;
    uint32_t _cshaderId = size_t(~0);
};




}// namespace Play::RDG

#endif // RDG_RESOURCES_H