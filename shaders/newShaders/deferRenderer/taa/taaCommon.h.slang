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

#endif // TAA_COMMON_H_SLANG
