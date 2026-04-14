#ifndef GAUSSIAN_LIB_H
#define GAUSSIAN_LIB_H

#include "Hdevice.h"
struct IndrectBuffer
{
    uint32_t indexCount    DEFAULT(6); // allways = 6 indices for the quad (2 triangles)
    uint32_t instanceCount DEFAULT(0); // will be incremented by the distance compute shader
    uint32_t firstIndex    DEFAULT(0); // allways zero
    uint32_t vertexOffset  DEFAULT(0); // allways zero
    uint32_t firstInstance DEFAULT(0); // allways zero

    // for vkCmdDrawMeshTasksIndirectEXT
    uint32_t groupCountX DEFAULT(0); // Will be incremented by the distance compute shader
    uint32_t groupCountY DEFAULT(1); // Allways one workgroup on Y
    uint32_t groupCountZ DEFAULT(1); // Allways one workgroup on Z

    // for debug info readback, TODO shall be in some other buffer
    int32_t particleID DEFAULT(-1);  // Will be set by the ID of the nearest splat on the ray path
    float particleDist DEFAULT(0.0); // Will be set by the dist to the nearest splat on the ray path
    float val1         DEFAULT(0.0);
    float val2         DEFAULT(0.0);
    float val3         DEFAULT(0.0);
    float val4         DEFAULT(0.0);
    float val5         DEFAULT(0.0);
    float val6         DEFAULT(0.0);
    float val7         DEFAULT(0.0);
    float val8         DEFAULT(0.0);
};

// Per-vertex data layout matching the PLY binary format exactly (56 bytes)
struct GaussianVertex
{
    float3 position;
    float4 color;
    float3 scale;
    float4 rotation;
};

// Camera metadata embedded in the PLY file
struct GaussianSceneMeta
{
    float    extrinsic[16]; // 4x4 camera extrinsic matrix
    float    intrinsic[9];  // 3x3 camera intrinsic matrix
    uint32_t imageSize[2];  // width, height
    int32_t  frame[2];
    float    disparity[2]; // min, max disparity
    uint8_t  colorSpace;
    uint8_t  version[3];
    uint32_t splatCount;
};
struct GaussianSceneUniform
{
    uint64_t positionBufferDeviceAddress;
    uint64_t colorBufferDeviceAddress;
    uint64_t covarianceBufferDeviceAddress;
    uint64_t shBufferDeviceAddress;
    uint64_t metaDataAddress;
    uint32_t positionStride;
    uint32_t colorStride;
    uint32_t covarianceStride;
    uint32_t shStride;
};

static const float sqrt8    = sqrt(8.0);
static const float SH_C1    = 0.4886025119029199f;
static const float SH_C2[5] = { 1.0925484, -1.0925484, 0.3153916, -1.0925484, 0.5462742 };
static const float SH_C3[7] = { -0.5900435899266435f, 2.890611442640554f, -0.4570457994644658f, 0.3731763325901154f,
                                -0.4570457994644658f, 1.445305721320277f, -0.5900435899266435f };
// todo:
// void fetchSH(in const GaussianPushConstant& constant, in uint splatIndex, out float4 sh[9]) {

// }
#endif // GAUSSIAN_LIB_H
