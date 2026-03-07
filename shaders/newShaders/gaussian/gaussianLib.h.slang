#ifndef GAUSSIAN_LIB_H
#define GAUSSIAN_LIB_H
#ifdef _cplusplus
#define DEFAULT(val) = val
#else
#define DEFAULT(val)
#endif // _cplusplus
#include "../Hdevice.h"
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
    float3 color;
    float  opacity;
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
#endif // GAUSSIAN_LIB_H