
#version 460
#extension GL_GOOGLE_include_directive : enable // To be able to use #include
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "host_device.h"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTagent;
layout(location = 3) in vec2 inUV;

layout(binding = 3, set = 0) uniform RenderUniformBlock
{
    RenderUniform renderUniform;
};
layout(binding = 2, set = 0) buffer MaterialBuffer
{
    Material materials[];
};

layout(push_constant) uniform PushConstantBlock
{
    Constants constants;
};

layout(location = 0) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

void main()
{
    outUV       = inUV;
    gl_Position = renderUniform.project * renderUniform.view * constants.model * vec4(inPos, 1.0);
    // renderUniform.model * vec4(inPos, 1.0);
    outNormal = (transpose(inverse(constants.model)) * vec4(inNormal, 1.0)).xyz;
}

// void main()
// {
//     vec2 outUV  = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
//     gl_Position = vec4(outUV * 2.f - 1.f, 0.f, 1.f);
// }