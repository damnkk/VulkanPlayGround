#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing :require

#include "common.glsl"
layout(location = 1) rayPayloadInEXT ShadowPlayLoad shadowpLoad;
void main(){
    shadowpLoad.isInShadow = false; 
}