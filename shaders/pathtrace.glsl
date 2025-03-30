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
vec3 sampleBSDF(GeomInfo geomInfo, vec3 dir_in, MaterialInfo materialInfo, inout PlayLoad rtPload,
                vec3 randomVec)
{
    return vec3(1.0, 0.0, 0.0);
}

// return bsdf/pdf
vec3 evaluateBSDF(GeomInfo geomInfo, vec3 dir_in, vec3 dir_out, MaterialInfo materialInfo)
{
    return vec3(0.0);
}

float evaluatepdf(GeomInfo geomInfo, vec3 dir_in, vec3 dir_out, MaterialInfo materialInfo)
{
    return 1.0;
}

vec3 traceRay(vec2 uv, vec2 resolution, int maxBounce)
{
    vec3 camSpaceUvPos = vec3(uv * 2.0 - 1.0,1.0);
    vec3 direct        = (renderUniform.viewInverse *
                   normalize(inverse(renderUniform.project) * vec4(camSpaceUvPos.xyz, 1.0)))
                      .xyz;
    vec4 origin = (renderUniform.viewInverse * vec4(0, 0, 0, 1));

    vec3 radiance = vec3(0.0);
    vec3 history = vec3(1.0);

    ivec2           rtResolution = ivec2(gl_LaunchSizeEXT.x, gl_LaunchSizeEXT.y);
    RayDifferential rayDiff;
    rayDiff.radius = 0.0;
    rayDiff.spread = 0.25 / max(rtResolution.x, rtResolution.y);

    Ray ray;
    ray.origin = origin.xyz;
    ray.direction = normalize(direct.xyz);
    closestTrace(ray);
    if (rtPload.hitT == INFINITY)
    {
        vec3 viewDir = (ray.direction);
        vec2 uv;
        // uv.x        = atan(viewDir.x, viewDir.z) * (1.0 / M_TWO_PI);
        // uv.y        = acos(clamp(viewDir.y, -1.0, 1.0)) * (1.0 / M_PI);
        // uv.x        = uv.x < 0.0 ? uv.x + 1.0 : uv.x;
        uv          = directionToSphericalEnvMap(viewDir);
        float dudwx = -viewDir.z / (viewDir.x * viewDir.x + viewDir.z * viewDir.z);
        float dudwz = viewDir.x / (viewDir.x * viewDir.x + viewDir.z * viewDir.z);
        float dvdwy = -1.0 / sqrt(max(1 - viewDir.y * viewDir.y, 0.0));
        // We only want to know the length of dudw & dvdw
        // The local coordinate transformation is length preserving,
        // so we don't need to differentiate through it.
        float footprint = min(sqrt(dudwx * dudwx + dudwz * dudwz), dvdwy);
        return textureLod(envTexture, uv, footprint).xyz;
    }
    // initialized E kernel in render equation
    vec3  throughput = vec3(1.0);
    float eta_scale  = 1.0;

    GeomInfo     geomInfo = getGeomInfo(rtPload);
    MaterialInfo matInfo  = getMaterialInfo(geomInfo);
    if (matInfo.emissiveTextureIdx != -1)
    {
        radiance += texture(sceneTextures[matInfo.emissiveTextureIdx], geomInfo.uv).xyz *
                    matInfo.emissiveFactor;
    }

    for (int i = 0; i < maxBounce; ++i)
    {
        // In a complete raytracing renderer, we will choise a light source(mesh light & env light)
        // using random number But right now, we temporarily use env light as the only light
        // source,so we just generate a random uv here
        // step1: calculate direct light
        vec2  lightUV        = vec2(rand(rtPload.seed), rand(rtPload.seed));
        vec4  cacheSampleRes = texture(envLookupTexture, lightUV);
        vec2  importanceUV   = cacheSampleRes.xy;
        vec3  lightDir       = sphericalEnvMapToDirection(importanceUV);
        vec3  c1             = vec3(0.0);
        float w1             = 0.0;

        float G = 0.0;
        vec3  dir_light;
        if (false)
        {
            // this branch is for mesh light, I'll implement it later,
        }
        else
        {
            Ray shadowRay;
            shadowRay.origin = offsetRay(geomInfo.position, geomInfo.normal);
            // geomInfo.position + 0.0001;
            shadowRay.direction = lightDir;
            shadowTrace(shadowRay);
            if (!shadowPload.isInShadow)
            {
                G = 1.0;
            }
            else
            {
                return vec3(0.0, 1.0, 1.0);
            }
        }

        // 1.0 here is a placeholder, in future ,we will have many light source(include mesh light
        // and env map light),and we gonna choose one of them randomly,and we will get a pdf here
        float p1 = 1.0 * cacheSampleRes.z;
        if (G > 0.0 && p1 > 0.0)
        {
            vec3 dir_in   = normalize(-ray.direction);
            vec3 bsdf     = evaluateBSDF(geomInfo, dir_in, lightDir, matInfo);
            vec3 envLight = textureLod(envTexture, importanceUV, 0).xyz;
            c1            = G * bsdf * envLight;
            float p2      = evaluatepdf(geomInfo, dir_in, lightDir, matInfo) * G;
            w1            = pow(p1, 2.0) / (pow(p1, 2.0) + pow(p2, 2.0));
            c1 /= p1;
        }
        radiance += throughput * c1 * w1;
        float rd1     = rand(rtPload.seed);
        float rd2     = rand(rtPload.seed);
        float rd3     = rand(rtPload.seed);
        vec3  dir_out = sampleBSDF(geomInfo, ray.direction, matInfo, rtPload, vec3(rd1, rd2, rd3));
        {
            // ray spread shit, implement later
        }
        Ray bsdfRay;
        bsdfRay.origin    = geomInfo.position;
        bsdfRay.direction = dir_out;
        closestTrace(bsdfRay);

        return vec3(1.0);
    }

    return radiance;
}

#endif // __pathtrace_H__