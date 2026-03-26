#include "RDGResources.h"
#include "Resource.h"

namespace Play::RDG
{

RDGTexture::~RDGTexture()
{
    // RefPtr 自动释放
}

RDGBuffer::~RDGBuffer()
{
    // RefPtr 自动释放
}
} // namespace Play::RDG
