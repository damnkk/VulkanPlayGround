#ifndef __pathtrace_H__
#define __pathtrace_H__
#include "utility.glsl"
void closestTrace( Ray ray){
    uint rayFlags = gl_RayFlagsOpaqueEXT;
    rtPload.hitT =INFINITY;
    traceRayEXT(topLevelAS, 
    rayFlags, 
    0xFF, 
    0, 
    0, 
    0, 
    ray.origin, 
    0.0, 
    ray.direction, 
    INFINITY, 
    0);
}

vec3 traceRay(vec2 uv, vec2 resolution){
    vec3 camSpaceUvPos = vec3(uv * 2.0 - 1.0,1.0);
    vec3 direct = (renderUniform.viewInverse * vec4(normalize(camSpaceUvPos.xyz), 1.0)).xyz;
    vec3 origin = renderUniform.cameraPosition;

    Ray ray;
    ray.origin = origin.xyz;
    ray.direction = normalize(direct.xyz);
    closestTrace(ray);
    // GeomInfo geomInfo = getGeomInfo(rtPload);
    if(rtPload.hitT <INFINITY){
        return vec3(1.0);
    }
    return vec3(uv,0.0);
}

#endif // __pathtrace_H__