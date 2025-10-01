#include "DescriptorManager.h"

namespace Play
{

DescriptorSetManager::~DescriptorSetManager()
{
    Buffer::Destroy(_bufferDescBuffer);
    Buffer::Destroy(_samplerDescBuffer);
}

} // namespace Play