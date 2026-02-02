#include "RDGResources.h"
#include "Resource.h"
#include "PlayAllocator.h"

namespace Play::RDG
{

RDGTexture::~RDGTexture()
{
    if (_rhi)
    {
        TexturePool::Instance().free(_rhi);
        _rhi = nullptr;
    }
}

RDGBuffer::~RDGBuffer()
{
    if (_rhi)
    {
        BufferPool::Instance().free(_rhi);
        _rhi = nullptr;
    }
}
} // namespace Play::RDG
