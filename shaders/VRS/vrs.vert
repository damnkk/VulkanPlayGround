#version 460
#extension GL_GOOGLE_include_directive : enable         // To be able to use #include
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "../common.glsl"
#include "../host_device.h"
layout(set= 0,binding = 0) uniform ubo{
    ShaderRateUniformStruct shaderRateUniform;
};

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTagent;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
void main(){
    outUV       = inUV;
    gl_Position = shaderRateUniform.ProjectMatrix * shaderRateUniform.ViewMatrix *  vec4(inPos, 1.0);
    // renderUniform.model * vec4(inPos, 1.0);
    outNormal = vec4(inNormal, 1.0).xyz;
}