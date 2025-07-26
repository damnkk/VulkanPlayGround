#ifndef RDG_RESOURCES_H
#define RDG_RESOURCES_H
#include "Resource.h"
#include "RDGHandle.h"

namespace Play::RDG{
class RDGPass;
class RDGTexturePool;
class RDGTexture{
public:
    friend class RenderDependencyGraph;
    friend class RDGTextureDescriptionPool;
    RDGTexture() = default;
    RDGTexture(Texture* texture) : _pData(texture) {}
    RDGTexture(const RDGTexture&) = delete;
    RDGTexture& operator=(const RDGTexture&) = delete;
    ~RDGTexture() = default;
    void setMetaData(Texture::TexMetaData metadata) { this->_pData->_metadata = metadata; }
    bool isExternal = false;
    TextureHandle handle = TextureHandle::Null;
    RDGPass* _lastProducer = nullptr;
private:
    Texture* _pData;
    friend class RDGTexturePool;
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
    BufferHandle handle = BufferHandle::Null;
    void setMetaData(Buffer::BufferMetaData metadata) { this->_pData->_metadata = metadata; }
    bool isExternal = false;
    RDGPass* _lastProducer = nullptr;
private:
    Buffer* _pData;
    friend class RDGBufferPool;
};


}// namespace Play::RDG

#endif // RDG_RESOURCES_H