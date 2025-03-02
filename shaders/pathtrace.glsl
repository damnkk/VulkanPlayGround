#ifndef __pathtrace_H__
#define __pathtrace_H__
#extension GL_GOOGLE_include_directive : enable
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

float SchlickFresnel(float u)
{
    float m  = clamp(1.0 - u, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2 * m; // pow(m,5)
}

void shadowTrace(Ray ray)
{
    uint rayFlags          = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT;
    shadowPload.isInShadow = true;
    traceRayEXT(topLevelAS, rayFlags, 0xFF, 1, 0, 1, ray.origin, 0.0, ray.direction, INFINITY, 1);
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
vec3 evaluateBSDF(vec3 normal, vec3 viewDir, vec3 sampleDir, MaterialInfo materialInfo,
                  inout float pdf)
{
    if (dot(normal, sampleDir) < 0.0)
    {
        return vec3(0.0);
    }
    vec3  H            = normalize(viewDir + sampleDir);
    float diffuseRatio = 0.5;
    pdf                = max(0.001, dot(normal, sampleDir) * (1.0 / M_PI));
    float FL           = SchlickFresnel(dot(normal, sampleDir));
    float FV           = SchlickFresnel(dot(normal, viewDir));
    float Fd90         = 0.5 + 2.0 * dot(sampleDir, H) * dot(sampleDir, H) * 1.0;
    float Fd           = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);
    return ((1.0 / M_PI) * Fd * materialInfo.color) / pdf;
}

VisibilityContribution DirectLight(in Ray r, in MaterialInfo info)
{
    vec3                   Li = vec3(0.0);
    float                  lightPdf;
    vec3                   lightContrib;
    vec3                   lightDir;
    float                  lightDist = 1e32;
    bool                   isLight   = false;
    VisibilityContribution res;
    res.radiance = vec3(0.0);
    res.visible  = false;
    // Environment light
    {
        vec4 dirPdf = EnvSample(lightContrib);
        lightDir    = dirPdf.xyz;
        lightPdf    = dirPdf.w;
    }
    {
        float pdf;
        vec3  bsdfValue = evaluateBSDF(info.normal, -r.direction, lightDir, info, pdf);
        float misWeight = max(0.0, powerHeuristic(lightPdf, pdf));
        Li += misWeight * bsdfValue * abs(dot(lightDir, info.normal)) * lightContrib / lightPdf;
    }
    res.visible   = true;
    res.lightDir  = lightDir;
    res.lightDist = lightDist;
    res.radiance  = Li;

    return res;
    // 实现 DirectLight 函数的实际代码
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
            vec2 uv = directionToSphericalEnvMap2(ray.direction);
            return res + history * texture(envTextures, uv).xyz;
        }
        // if intersected,base infomation prepare
        GeomInfo     geomInfo     = getGeomInfo(rtPload);
        MaterialInfo materialInfo = getMaterialInfo(geomInfo.materialIdx, geomInfo);
        // if it's a light, return light color
        res += history * materialInfo.emissive;
        // // step1: sample bsdf, prepare for next bounce
        vec3 sampleDir = sampleBSDF(geomInfo.normal, -ray.direction, materialInfo, rtPload);
        float pdf;
        // // step2: evaluate bsdf
        vec3 bsdfDivPdf =
            evaluateBSDF(geomInfo.normal, -ray.direction, sampleDir, materialInfo, pdf);
        // // step3: update history
        if (pdf > 0.0)
        {
            history *= abs(dot(sampleDir, geomInfo.normal)) * bsdfDivPdf;
        }
        else
        {
            break;
        }

        // // step4: get direct light
        VisibilityContribution visContribution = DirectLight(ray, materialInfo);
        visContribution.radiance *= history;
        ray.origin    = OffsetRay(geomInfo.position, geomInfo.normal);
        ray.direction = sampleDir;
        // // step5: shoot shadow ray
        Ray shadowray;
        shadowray.origin    = ray.origin;
        shadowray.direction = visContribution.lightDir;
        shadowTrace(shadowray);
        if (!shadowPload.isInShadow)
        {
            res += visContribution.radiance;
            // res += vec3(1.0, 0.0, 0.0);
        }
    }

    return res;
}

#endif // __pathtrace_H__