#ifndef P_CONSTANT_TYPE_H
#define P_CONSTANT_TYPE_H

#ifdef __cplusplus
#include <glm/glm.hpp>
using float2 = glm::vec2;
#endif

struct PerFrameConstant
{
    uint64_t cameraBufferDeviceAddress;
};

struct SceneConstant
{
    uint64_t sceneAddress;
    uint32_t bucketID;
    uint32_t _padding;
};

struct GBufferPushConstant
{
    PerFrameConstant perFrameConstant;
    SceneConstant    sceneConstant;
};

struct TAAPushConstant
{
    // 1.0 discards history (first frame after allocation or reset), otherwise the
    // weight of the current frame in the resolve.
    float currentWeight;
    float _padding;
    float2 jitterUV;
    float2 previousJitterUV;
    uint64_t cameraBufferDeviceAddress;
};

#endif // P_CONSTANT_TYPE_H
