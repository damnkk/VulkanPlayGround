#version 460
precision mediump float;
#extension GL_GOOGLE_include_directive : enable // To be able to use #include
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "host_device.h"
layout(binding = 3, set = 0) uniform RenderUniformBlock
{
    RenderUniform renderUniform;
};
layout(binding = 2, set = 0, scalar) buffer MaterialBuffer
{
    Material materials[];
};
layout(binding = 6, set = 0) uniform sampler2D[] sceneTextures;
layout(location = 0) out vec4 fragColor;
layout(location = 2) in vec2 outUV;
layout(push_constant) uniform PushConstantBlock
{
    Constants constants;
};
void main()
{
    fragColor = vec4(1.0, 1.0, 0.0, 1.0);
    // fragColor.xy = outUV;
    Material currMeshMaterial = materials[constants.matIdx];

    fragColor.xyz =
        texture(sceneTextures[nonuniformEXT(currMeshMaterial.pbrBaseColorTexture)], outUV).xyz;
}