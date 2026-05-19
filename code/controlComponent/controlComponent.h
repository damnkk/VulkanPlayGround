#ifndef CONTROL_COMPONENT_H
#define CONTROL_COMPONENT_H
#include "Resource.h"
#include "nvshaders/tonemap_io.h.slang"
#include "core/RefCounted.h"
#include <rttr/rttr_enable.h>
namespace Play
{
// easy place to add some UI controls for testing, like changing the skybox, or some other settings that are not directly related to the renderer or
// scene manager
template <typename T>
class ControlComponent
{
public:
    ControlComponent();
    T& getCPUHandle()
    {
        return _cpuData;
    }
    Buffer* getGPUBuffer()
    {
        return _gpuBuffer.get();
    }

    void flushToGPU()
    {
        void* data = _gpuBuffer->mapping;
        memcpy(data, &_cpuData, sizeof(T));
    }

    RTTR_ENABLE()

protected:
    T              _cpuData{};
    RefPtr<Buffer> _gpuBuffer;
};

template <typename T>
ControlComponent<T>::ControlComponent()
{
    _gpuBuffer = RefPtr<Buffer>(new Buffer("control uniform", VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(T),
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
}

class ToneMappingControlComponent : public ControlComponent<shaderio::TonemapperData>
{
    RTTR_ENABLE(ControlComponent<shaderio::TonemapperData>)
};
} // namespace Play

#endif // CONTROL_COMPONENT_H
