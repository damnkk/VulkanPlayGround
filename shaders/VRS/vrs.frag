#version 460
#extension GL_GOOGLE_include_directive : enable // To be able to use #include
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "../host_device.h"
#include "../common.glsl"
layout(set= 0,binding = 0) uniform ubo{
    ShaderRateUniformStruct shaderRateUniform;
};

layout(set= 0,binding = 1,rg8ui) uniform uimage2D inputFrequencyImage;
layout(set = 0,binding = 2) uniform sampler2D[] sceneImage;
layout(binding = 3, set = 0, scalar) buffer MaterialBuffer
{
    Material materials[];
};
layout(location = 0) out vec4 outColor;
layout(location = 1) out uvec2 outFrequency;
void main(){
    outColor = vec4(1.0);
    outFrequency = uvec2(0.5,0.5);
}