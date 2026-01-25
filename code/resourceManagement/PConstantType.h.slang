#ifndef P_CONSTANT_TYPE_H
#define P_CONSTANT_TYPE_H
struct PerFrameConstant
{
    uint64_t cameraBufferDeviceAddress;
    uint64_t sceneAnimationBufferDeviceAddress;
};

struct SceneConstant
{
    uint64_t sceneDescAddress;
    uint32_t renderNodeId;
    uint32_t textureOffset;
};
#endif // P_CONSTANT_TYPE_H
