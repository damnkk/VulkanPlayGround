#ifndef DESCRIPTOR_MANAGER_H
#define DESCRIPTOR_MANAGER_H
#include "PlayApp.h"
#include "Resource.h"

namespace Play
{
class DescriptorSetManager
{
public:
    DescriptorSetManager(VkDevice device);
    ~DescriptorSetManager();

private:
    Buffer*                                       _bufferDescBuffer;
    Buffer*                                       _samplerDescBuffer;
    VkDevice                                      _device;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT _descriptorBufferProperties;
};

} // namespace Play

#endif // DESCRIPTOR_MANAGER_H