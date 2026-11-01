#include "common.slang"
#include "PConstantType.h.slang"
#include "taaCommon.h.slang"

[[vk::push_constant]]
ConstantBuffer<TAAPushConstant> g_TaaPushConstant;

[[vk::binding(0, 3)]]
Texture2D<float4> g_CurrentColor; // Scene-linear HDR; compress samples before resolving.
[[vk::binding(1, 3)]]
Texture2D<float2> g_Velocity;
[[vk::binding(2, 3)]]
Texture2D<float4> g_HistoryColor;
[[vk::binding(3, 3)]]
RWTexture2D<float4> g_ResolvedColor;
[[vk::binding(4, 3)]]
Texture2D<float> g_SceneDepth;

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint width;
    uint height;
    g_CurrentColor.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
        return;

    int2   pixel = int2(dispatchThreadID.xy);
    float2 uv    = (float2(pixel) + 0.5) / float2(width, height);

    float3 current  = Tonemap(g_CurrentColor.Load(int3(pixel, 0)).rgb);
    // Non-finite scene samples must not enter the persistent history or its
    // neighborhood bounds. History itself is checked separately below.
    if (!all(isfinite(current)))
    {
        g_ResolvedColor[pixel] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float3 resolved = current;
    float currentWeight = saturate(g_TaaPushConstant.currentWeight);

    // The first frame after allocation/reset must never sample uninitialized history.
    if (currentWeight < 1.0)
    {
        // Motion is stored on the raster grid but excludes jitter at both endpoints.
        float2 motion = g_Velocity.Load(int3(pixel, 0)).xy;
        float2 previousUnjitteredUV = uv - g_TaaPushConstant.jitterUV - motion;
        bool previousProjectionValid = true;
        if (g_SceneDepth.Load(int3(pixel, 0)) >= 1.0)
        {
            // Sky did not write GVelocity. Reproject a world direction at
            // infinity: camera rotation matters, camera translation does not.
            CameraData camera = ((CameraData*) g_TaaPushConstant.cameraBufferDeviceAddress)[0];
            float2 ndc = 2.0 * (uv - g_TaaPushConstant.jitterUV) - 1.0;
            float4 viewPoint = mul(float4(ndc, 1.0, 1.0), camera.invProjMatrix);
            float3 worldDirection = mul(viewPoint.xyz / viewPoint.w, (float3x3) camera.invViewMatrix);
            float4 previousClip = mul(float4(worldDirection, 0.0), camera.prevViewProjMatrix);
            previousProjectionValid = previousClip.w > 0.0;
            if (previousProjectionValid)
                previousUnjitteredUV = previousClip.xy / previousClip.w * 0.5 + 0.5;
        }
        // Resolve writes to the current raster grid, so saved history retains
        // the previous frame's jitter. Convert back into that texture's grid.
        float2 previousUV = previousUnjitteredUV + g_TaaPushConstant.previousJitterUV;
        float2 halfTexel = 0.5 / float2(width, height);
        bool historyInBounds = previousProjectionValid
                            && all(previousUnjitteredUV >= 0.0) && all(previousUnjitteredUV <= 1.0)
                            && all(previousUV >= halfTexel) && all(previousUV <= 1.0 - halfTexel);

        // Reject missing offscreen history instead of stretching clamp-edge texels.
        if (historyInBounds)
        {
            float3 currentYCoCg = RGBToYCoCg(current);
            float3 neighborhoodMin = currentYCoCg;
            float3 neighborhoodMax = currentYCoCg;
            // Current pixel plus its eight neighbors, all in the same resolve space.
            for (int y = -1; y <= 1; ++y)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    int2 samplePixel = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
                    float3 sampleColor = RGBToYCoCg(Tonemap(g_CurrentColor.Load(int3(samplePixel, 0)).rgb));
                    if (all(isfinite(sampleColor)))
                    {
                        neighborhoodMin = min(neighborhoodMin, sampleColor);
                        neighborhoodMax = max(neighborhoodMax, sampleColor);
                    }
                }
            }

            // History was already mapped by resolve before being stored. Do not Tonemap it again.
            float3 history = g_HistoryColor.SampleLevel(g_GlobalSampler_Linear_Clamp, previousUV, 0).rgb;
            // Inf produces Inf-Inf in YCoCg and 0*Inf during clipping. Reject
            // invalid history before either operation so it cannot spread.
            if (all(isfinite(history)))
            {
                float3 clippedHistory = ClipHistoryYCoCg(RGBToYCoCg(history), currentYCoCg, neighborhoodMin, neighborhoodMax);
                resolved = YCoCgToRGB(lerp(clippedHistory, currentYCoCg, currentWeight));
            }
        }
    }

    g_ResolvedColor[pixel] = float4(resolved, 1.0);
}
