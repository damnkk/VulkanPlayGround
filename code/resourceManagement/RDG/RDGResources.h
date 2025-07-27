#ifndef RDG_RESOURCES_H
#define RDG_RESOURCES_H
#include "Resource.h"
#include "RDGHandle.h"

namespace Play::RDG{
class RDGPass;
class RDGTexturePool;
class RenderDependencyGraph;
class RDGTexture{
public:
RDGTexture() = default;
RDGTexture(Texture* texture) : _pData(texture) {}
RDGTexture(const RDGTexture&) = delete;
RDGTexture& operator=(const RDGTexture&) = delete;
~RDGTexture() = default;
void setMetaData(Texture::TexMetaData metadata) { this->_pData->_metadata = metadata; }
Texture* RHI(){return _pData;}
bool isExternal = false;
TextureHandle handle = TextureHandle::Null;
RDGPass* _lastProducer = nullptr;
private:
Texture* _pData;
friend class RDGTexturePool;
friend class RenderDependencyGraph;
};

class RDGBuffer{
public:
RDGBuffer() = default;
RDGBuffer(Buffer* buffer) : _pData(buffer) {}
RDGBuffer(const RDGBuffer&) = delete;
RDGBuffer& operator=(const RDGBuffer&) = delete;
~RDGBuffer() = default;
BufferHandle handle = BufferHandle::Null;
void setMetaData(Buffer::BufferMetaData metadata) { this->_pData->_metadata = metadata; }
Buffer* RHI(){return _pData;}
bool isExternal = false;
RDGPass* _lastProducer = nullptr;
private:
Buffer* _pData;
friend class RDGBufferPool;
friend class RenderDependencyGraph;
};


}// namespace Play::RDG

#endif // RDG_RESOURCES_H