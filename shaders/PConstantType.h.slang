#ifndef P_CONSTANT_TYPE_H
#define P_CONSTANT_TYPE_H
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

#endif // P_CONSTANT_TYPE_H
