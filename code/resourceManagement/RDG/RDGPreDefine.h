#ifndef RDGPREDEFINE
#define RDGPREDEFINE
#include <type_traits>
#include "RDGResources.h"
#include <glm/glm.hpp>
#include <cstdint>
namespace Play{
enum class AccessType{
    eReadOnly, //uniform buffer, sampled texture
    eReadWrite, //storage buffer, storage texture
    eCount
};

namespace RDG{
enum class PassType{
    eRenderPass,
    eComputePass,
    eRayTracingPass
};
struct RDGRTState{
    enum class LoadType{
        eLoad,
        eDontCare,
        eClear
    } loadType = LoadType::eLoad;

    enum class StoreType{
        eStore,
        eDontCare,
    } storeType = StoreType::eStore;
    
    RDGTexture* rtTexture = nullptr;
    RDGTexture* resolveTexture = nullptr;
    VkImageLayout initLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    glm::vec4 clearColor = glm::vec4(0.0f);
    float depthClear = 1.0f;
    uint32_t stencilClear = 0;
};
}

} // namespace Play
#endif // RDGPREDEFINE