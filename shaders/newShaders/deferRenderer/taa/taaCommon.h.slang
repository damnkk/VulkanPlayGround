#ifndef TAA_COMMON_H_SLANG
#define TAA_COMMON_H_SLANG

// Reinhard compression for TAA, independent of the display tonemapping curve.
// The analytic inverse restores HDR after resolve; texture precision and the
// denominator floor below limit round-trip accuracy near one.
float3 Tonemap(float3 color)
{
    // return color / (1.0 + color);
    return color;
}

float3 InverseTonemap(float3 color)
{
    // return color / max(1.0 - color, 1e-4);
    return color;
}

float3 RGBToYCoCg(float3 color)
{
    return float3(dot(color, float3(0.25, 0.5, 0.25)),
                  dot(color, float3(0.5, 0.0, -0.5)),
                  dot(color, float3(-0.25, 0.5, -0.25)));
}

float3 YCoCgToRGB(float3 color)
{
    // Preserve HDR range; this is a color-space conversion, not display mapping.
    return float3(color.x + color.y - color.z,
                  color.x + color.z,
                  color.x - color.y - color.z);
}

float3 ClipHistoryYCoCg(float3 history, float3 current, float3 neighborhoodMin, float3 neighborhoodMax)
{
    // Current is inside its own neighborhood AABB. Intersect the segment from
    // current to history with that box, preserving one direction across channels.
    float3 delta = history - current;
    float fraction = 1.0;
    for (int channel = 0; channel < 3; ++channel)
    {
        if (delta[channel] > 0.0)
            fraction = min(fraction, (neighborhoodMax[channel] - current[channel]) / delta[channel]);
        else if (delta[channel] < 0.0)
            fraction = min(fraction, (neighborhoodMin[channel] - current[channel]) / delta[channel]);
    }
    return current + saturate(fraction) * delta;
}

#endif // TAA_COMMON_H_SLANG
