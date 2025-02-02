
#version 460
#extension GL_GOOGLE_include_directive : enable         // To be able to use #include
#include "host_device.h"


layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2)in vec3 inTagent;
layout(location = 3) in vec2 inUV;

layout(binding = 3,set = 0) uniform RenderUniformBlock{
    RenderUniform renderUniform;
};
layout(binding = 2,set = 0) buffer MaterialBuffer{
    Material materials[];
};

layout(location = 0) out vec3 outNormal;


void  main(){
    gl_Position = renderUniform.project * renderUniform.view * renderUniform.model * vec4(inPos,1.0);
    outNormal = (transpose(inverse(renderUniform.model))* vec4(inNormal,1.0)).xyz;
}
