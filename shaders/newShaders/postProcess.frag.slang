#include "common.slang"
#include "tonemap_functions.h.slang"
struct FragmentInput
{
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
};

// Per-Pass bindings
[vk::binding(0, 3)]
Texture2D<float4> inputTexture;

[shader("fragment")]
void main(FragmentInput fragIn, out FragmentOutput fragOut)
{
    uint64_t    cameraBufferPtr    = g_PerFrameConstant.cameraBufferDeviceAddress;
    uint64_t    sceneAnimBufferPtr = g_PerFrameConstant.sceneAnimationBufferDeviceAddress;
    CameraData* cameraData         = (CameraData*) cameraBufferPtr;
    float3      viewMat            = cameraData->cameraPosition;
    fragOut.color                  = float4(normalize(viewMat), 1.0);
}
