#version 460
#extension GL_GOOGLE_include_directive : enable // To be able to use #include
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "../host_device.h"
#include "../common.glsl"
layout(set = 0, binding = 0) uniform ubo
{
    ShaderRateUniformStruct shaderRateUniform;
};

layout(set = 0, binding = 1, rg8ui) uniform uimage2D inputFrequencyImage;
layout(set = 0, binding = 2) uniform sampler2D[] sceneImage;
layout(binding = 3, set = 0, scalar) buffer MaterialBuffer
{
    Material materials[];
};

layout(set = 1, binding = 0) uniform InstanceInfo
{
    DynamicStruct instanceInfo;
};

layout(location = 2) in vec2 outUV;
layout(location = 0) out vec4 outColor;
layout(location = 1) out uvec2 outFrequency;
void main()
{
    outColor        = vec4(1.0);
    float    freq_x = 0, freq_y = 0;
    Material currMeshMaterial = materials[instanceInfo.matIdx];
    outColor.xyz =
        texture(sceneImage[nonuniformEXT(currMeshMaterial.pbrBaseColorTexture)], outUV).xyz;
    vec3 dx      = dFdx(outColor.xyz);
    vec3 dy      = dFdy(outColor.xyz);
    freq_x       = dot(dx, dx);
    freq_y       = dot(dy, dy);
    outFrequency = uvec2(255 * freq_x, 255 * freq_y);
}