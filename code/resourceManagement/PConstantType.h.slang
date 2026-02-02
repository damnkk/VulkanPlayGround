#ifndef P_CONSTANT_TYPE_H
#define P_CONSTANT_TYPE_H
struct PerFrameConstant
{
    uint64_t cameraBufferDeviceAddress;
};

struct SceneConstant
{
    uint64_t sceneDescAddress;
    uint32_t renderNodeId;
    uint32_t textureOffset;
};

struct GBufferPushConstant
{
    PerFrameConstant perFrameConstant;
    SceneConstant    sceneConstant;
};
#endif // P_CONSTANT_TYPE_H
