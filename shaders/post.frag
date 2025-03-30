#version 460

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(location = 0) in vec2 outUV;

layout(location = 0) out vec4 fragColor ;

const float GAMMA     = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 linearTosRGB(vec3 color)
{
    return pow(color, vec3(INV_GAMMA));
}

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 sRGBToLinear(vec3 srgbIn)
{
    return vec3(pow(srgbIn.xyz, vec3(GAMMA)));
}

vec4 sRGBToLinear(vec4 srgbIn)
{
    return vec4(sRGBToLinear(srgbIn.xyz), srgbIn.w);
}

// http://user.ceng.metu.edu.tr/~akyuz/files/hdrgpu.pdf
const mat3 RGB2XYZ = mat3(0.4124564, 0.3575761, 0.1804375, 0.2126729, 0.7151522, 0.0721750,
                          0.0193339, 0.1191920, 0.9503041);
float      luminance(vec3 color)
{
    return dot(color, vec3(0.2126f, 0.7152f,
                           0.0722f)); // color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722;
}

vec3 toneExposure(vec3 RGB, float logAvgLum)
{
    vec3  XYZ = RGB2XYZ * RGB;
    float Y   = (0.5 / logAvgLum) * XYZ.y;
    float Yd  = (Y * (1.0 + Y / (0.5 * 0.5))) / (1.0 + Y);
    return RGB / XYZ.y * Yd;
}

// Uncharted 2 tone map
// see: http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 toneMapUncharted2Impl(vec3 color)
{
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

vec3 toneMapUncharted(vec3 color)
{
    const float W   = 11.2;
    color           = toneMapUncharted2Impl(color * 2.0);
    vec3 whiteScale = 1.0 / toneMapUncharted2Impl(vec3(W));
    return linearTosRGB(color * whiteScale);
}
void main(){
    fragColor = texture(inputTexture,outUV);
    // vec4  avg     = textureLod(inputTexture, vec2(0.5), 20); // Get the average value of the
    // image float avgLum2 = luminance(avg.rgb); fragColor.xyz = toneMapUncharted(fragColor.xyz);
}