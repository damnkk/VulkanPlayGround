#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require // This is about ray tracing
#include "common.glsl"
layout(location = 0) rayPayloadInEXT PlayLoad rtPload;
void main(){
    rtPload.hitT = -1;
}