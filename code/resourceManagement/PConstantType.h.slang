#ifndef P_CONSTANT_TYPE_H
#define P_CONSTANT_TYPE_H
struct PerFrameConstant
{
    uint64_t cameraBufferDeviceAddress;
};

struct SceneConstant
{
    uint64_t instanceBufferAddress;
    uint64_t instanceIndex;
};

struct GBufferPushConstant
{
    PerFrameConstant perFrameConstant;
    SceneConstant    sceneConstant;
};

#endif // P_CONSTANT_TYPE_H
