#version 460
#extension GL_EXT_ray_tracing : require // This is about ray tracing
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"
layout(location = 0) rayPayloadInEXT PlayLoad rtPload;
hitAttributeEXT vec2 bary;

void main()
{
    // prd.seed;
    rtPload.hitT                = gl_HitTEXT;
    rtPload.primitiveID         = gl_PrimitiveID;
    rtPload.instanceID          = gl_InstanceID;
    rtPload.instanceCustomIndex = gl_InstanceCustomIndexEXT;
    rtPload.baryCoord           = bary;
    rtPload.objectToWorld       = gl_ObjectToWorldEXT;
    rtPload.worldToObject       = gl_WorldToObjectEXT;
}