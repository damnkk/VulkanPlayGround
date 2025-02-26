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

// get a sample direction from bsdf
vec3 sampleBSDF(vec3 normal, vec3 viewDir, MaterialInfo materialInfo, inout PlayLoad rtPload)
{
    vec3 tangentRes = cosineSampleHemisphere(rand(rtPload.seed), rand(rtPload.seed));
    vec3 tangent;
    vec3 bitTangent;
    buildCoordSystem(normal, tangent, bitTangent);
    return normalize(tangentRes.x * tangent + tangentRes.y * bitTangent + tangentRes.z * normal);
}

// return bsdf/pdf
vec3 evaluateBSDF(vec3 normal, vec3 viewDir, vec3 sampleDir, MaterialInfo materialInfo)
{
    vec3  brdf = materialInfo.color * dot(sampleDir, normal) / M_PI;
    float pdf  = dot(sampleDir, normal) / M_PI;
    return brdf / pdf;
}

vec3 traceRay(vec2 uv, vec2 resolution, int maxBounce)
{
    vec3 camSpaceUvPos = vec3(uv * 2.0 - 1.0,1.0);
    vec3 direct        = (renderUniform.viewInverse *
                   normalize(inverse(renderUniform.project) * vec4(camSpaceUvPos.xyz, 1.0)))
                      .xyz;
    vec4 origin = (renderUniform.viewInverse * vec4(0, 0, 0, 1));

    vec3 res     = vec3(0.0);
    vec3 history = vec3(1.0);

    Ray ray;
    ray.origin = origin.xyz;
    ray.direction = normalize(direct.xyz);
    for (int i = 0; i < maxBounce; ++i)
    {
        closestTrace(ray);
        // if not intersected with any object, it's all about direct lighting
        if (rtPload.hitT == INFINITY)
        {
            vec2 uv = directionToSphericalEnvMap(ray.direction);

            return res + history * clamp(texture(envTextures, uv).xyz, vec3(0.0), vec3(5000.0));
        }
        // if intersected,base infomation prepare
        GeomInfo     geomInfo     = getGeomInfo(rtPload);
        MaterialInfo materialInfo = getMaterialInfo(geomInfo.materialIdx);
        // if it's a light, return light color
        if (length(materialInfo.emissive) > 0.0)
        {
            res += history * materialInfo.emissive;
        }
        // // step1: sample bsdf, prepare for next bounce
        vec3 sampleDir = sampleBSDF(geomInfo.normal, -ray.direction, materialInfo, rtPload);
        // // step2: evaluate bsdf
        vec3 bsdfDivPdf = evaluateBSDF(geomInfo.normal, -ray.direction, sampleDir, materialInfo);
        // // step3: update history
        history *= abs(dot(sampleDir, geomInfo.normal)) * bsdfDivPdf;
        ray.origin    = geomInfo.position + geomInfo.normal * 0.001;
        ray.direction = sampleDir;
        // res           = geomInfo.normal;
    }

    return res;
}

#endif // __pathtrace_H__