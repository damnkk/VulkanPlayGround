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

struct GaussianSceneConstant
{
    uint64_t positionBufferDeviceAddress;
    uint64_t colorBufferDeviceAddress;
    uint64_t opacityBufferDeviceAddress;
    uint64_t scaleBufferDeviceAddress;
    uint64_t rotationBufferDeviceAddress;
    uint32_t positionStride;
    uint32_t colorStride;
    uint32_t opacityStride;
    uint32_t scaleStride;
    uint32_t rotationStride;
    uint64_t metaDataAddress;
};

struct GaussianPushConstant
{
    PerFrameConstant      perFrameConstant;
    GaussianSceneConstant sceneConstant;
};
#endif // P_CONSTANT_TYPE_H
