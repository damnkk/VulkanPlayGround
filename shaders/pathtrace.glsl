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
    // //------------------------------diffuse bsdf sample------------------------------
    // float phi = M_TWO_PI * randomVec.x;
    // float tmp = sqrt(clamp(1.0 - randomVec.y, 0.0, 1.0));

    // return toNormalHemisphere(
    //     vec3(cos(phi) * tmp, sin(phi) * tmp, sqrt(clamp(randomVec.y, 0.0, 1.0))),
    //     geomInfo.normal);

    //------------------------------glass bsdf sample------------------------------
    float eta   = dot(geomInfo.normal, dir_in) > 0.0 ? materialInfo.eta : 1.0 / materialInfo.eta;
    float alpha = pow(materialInfo.roughness, 2.0);
    vec3  local_dir_in = toNormalLocal(dir_in, geomInfo);
    vec3  local_micro_normal =
        sampleVisible_normals(local_dir_in, alpha, vec2(rand(rtPload.seed), rand(rtPload.seed)));
    vec3 half_vector = normalize(toNormalHemisphere(local_micro_normal, geomInfo.normal));
    if (dot(half_vector, geomInfo.normal) < 0.0)
    {
        half_vector = -half_vector;
    }
    float h_dot_in = dot(half_vector, dir_in);
    float F        = fresnel_dielectric(h_dot_in, eta);
    if (rand(rtPload.seed) <= F)
    {
        vec3 reflect = normalize(-dir_in + 2.0 * dot(dir_in, half_vector) * half_vector);
        return reflect;
    }
    else
    {
        float h_dot_out_sq = 1.0 - (1.0 - h_dot_in * h_dot_in) / pow(eta, 2.0);
        if (h_dot_out_sq <= 0.0)
        {
            return vec3(0.0);
        }
        if (h_dot_in < 0.0)
        {
            half_vector = -half_vector;
        }
        float h_dot_out = sqrt(h_dot_out_sq);
        vec3  refracted = -dir_in / eta + (abs(h_dot_in) / eta - h_dot_out) * half_vector;
        return refracted;
    }
}

// return bsdf
vec3 evaluateBSDF(GeomInfo geomInfo, vec3 dir_in, vec3 dir_out, MaterialInfo materialInfo)
{
    // //------------------------------diffuse bsdf evaluate------------------------------
    // if (dot(geomInfo.normal, dir_in) <= 0.0 || dot(geomInfo.normal, dir_out) <= 0.0)
    // {
    //     return vec3(0.0);
    // }
    // vec3 diffuseBsdf = Fd(dir_in, dir_out, dir_in, geomInfo.normal, materialInfo) *
    //                    Fd(dir_in, dir_out, dir_out, geomInfo.normal, materialInfo) *
    //                    abs(dot(geomInfo.normal, dir_out)) * materialInfo.baseColor / M_PI;
    // float FssIn                 = Fss(dir_in, dir_out, dir_in, geomInfo.normal, materialInfo);
    // float FssOut                = Fss(dir_in, dir_out, dir_out, geomInfo.normal, materialInfo);
    // float absNdotI              = abs(dot(geomInfo.normal, dir_in));
    // float absNdotO              = abs(dot(geomInfo.normal, dir_out));
    // vec3  diffuseSubsurfaceBsdf = (FssIn * FssOut * (1.0 / (absNdotI + absNdotO) - 0.5) + 0.5) *
    //                              absNdotO * 1.25 * materialInfo.baseColor / M_PI;
    // return mix(diffuseBsdf, diffuseSubsurfaceBsdf, materialInfo.subsurface);

    // //---------------------------------glass bsdf evaluate--------------------------------
    bool  isReflect = dot(geomInfo.normal, dir_in) * dot(geomInfo.normal, dir_out) > 0.0;
    float eta = dot(geomInfo.normal, dir_in) > 0.0 ? materialInfo.eta : 1.0 / materialInfo.eta;
    vec3  h;
    if (isReflect)
    {
        h = normalize(dir_in + dir_out);
    }
    else
    {
        h = normalize(dir_in + dir_out * eta);
    }
    if (dot(h, geomInfo.normal) < 0.0)
    {
        h = -h;
    }
    float G = GTR(dir_in, materialInfo.roughness, materialInfo.anisotropic, geomInfo) *
              GTR(dir_out, materialInfo.roughness, materialInfo.anisotropic, geomInfo);
    float aspect  = sqrt(1.0 - materialInfo.anisotropic * 0.9);
    float ax      = max(0.001, materialInfo.roughness * materialInfo.roughness / aspect);
    float ay      = max(0.001, materialInfo.roughness * materialInfo.roughness * aspect);
    vec3  local_h = toNormalLocal(h, geomInfo);
    float hlx     = local_h.x;
    float hly     = local_h.y;
    float hlz     = local_h.z;
    float hlxyz   = ((hlx * hlx) / pow(ax, 2.0) + (hly * hly) / pow(ay, 2.0) + (hlz * hlz)) *
                  ((hlx * hlx) / pow(ax, 2.0) + (hly * hly) / pow(ay, 2.0) + (hlz * hlz));
    float D  = 1.0 / (M_PI * ax * ay * hlxyz);
    float Fg = fresnel_dielectric(dot(h, dir_in), eta);
    if (isReflect)
    {
        return (materialInfo.baseColor * G * D * Fg) / (4.0 * abs(dot(geomInfo.normal, dir_in)));
    }
    return (sqrt(materialInfo.baseColor) * (1.0 - Fg) * D * G *
            abs(dot(h, dir_out) * dot(h, dir_in))) /
           (abs(dot(geomInfo.normal, dir_in)) * pow((dot(h, dir_in) + eta * dot(h, dir_out)), 2.0));
}

float evaluatepdf(GeomInfo geomInfo, vec3 dir_in, vec3 dir_out, MaterialInfo materialInfo)
{
    // //------------------------------diffuse pdf evaluate------------------------------
    // if (dot(geomInfo.normal, dir_in) <= 0.0 || dot(geomInfo.normal, dir_out) <= 0.0)
    // {
    //     return 0.0001;
    // }
    // return max(dot(geomInfo.normal, dir_out), 0.00) / M_PI;

    //------------------------------glass pdf evaluate------------------------------

    bool  isReflect = dot(geomInfo.normal, dir_in) * dot(geomInfo.normal, dir_out) > 0.0;
    float eta = dot(geomInfo.normal, dir_in) > 0.0 ? materialInfo.eta : 1.0 / materialInfo.eta;
    vec3  h   = vec3(0.0);
    if (isReflect)
    {
        h = normalize(dir_in + dir_out);
    }
    else
    {
        h = normalize(dir_in + dir_out * eta);
    }
    if (dot(h, geomInfo.normal) < 0.0)
    {
        h = -h;
    }
    float h_dot_in = dot(h, dir_in);

    float Fg = fresnel_dielectric(h_dot_in, eta);
    float G  = GTR(dir_in, materialInfo.roughness, materialInfo.anisotropic, geomInfo) *
              GTR(dir_out, materialInfo.roughness, materialInfo.anisotropic, geomInfo);
    float aspect  = sqrt(1.0 - materialInfo.anisotropic * 0.9);
    float ax      = max(0.001, materialInfo.roughness * materialInfo.roughness / aspect);
    float ay      = max(0.001, materialInfo.roughness * materialInfo.roughness * aspect);
    vec3  local_h = toNormalLocal(h, geomInfo);
    float hlx     = local_h.x;
    float hly     = local_h.y;
    float hlz     = local_h.z;
    float hlxyz   = ((hlx * hlx) / pow(ax, 2.0) + (hly * hly) / pow(ay, 2.0) + (hlz * hlz)) *
                  ((hlx * hlx) / pow(ax, 2.0) + (hly * hly) / pow(ay, 2.0) + (hlz * hlz));
    float D = 1.0 / (M_PI * ax * ay * hlxyz);

    if (isReflect)
    {
        return max(0.01, (Fg * D * G) / (4.0 * abs(dot(geomInfo.normal, dir_in))));
    }
    float h_dot_out  = dot(h, dir_out);
    float sqrt_denom = h_dot_in + eta * h_dot_out;
    float dh_dout    = eta * eta * h_dot_out / (sqrt_denom * sqrt_denom);
    return max(0.01, (1.0 - Fg) * D * G * abs(dh_dout * h_dot_in / dot(geomInfo.normal, dir_in)));
}

vec3 traceRay(vec2 uv, vec2 resolution, int maxBounce)
{
    vec3 camSpaceUvPos = vec3(uv * 2.0 - 1.0,1.0);
    vec3 direct        = (renderUniform.viewInverse *
                   normalize(inverse(renderUniform.project) * vec4(camSpaceUvPos.xyz, 1.0)))
                      .xyz;
    vec4 origin = (renderUniform.viewInverse * vec4(0, 0, 0, 1));

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
    vec3  radiance   = vec3(0.0);
    vec3  throughput = vec3(1.0);
    float eta_scale  = 1.0;

    GeomInfo     geomInfo = getGeomInfo(rtPload);
    PbrMaterial  matInfo  = getMaterialInfo(geomInfo);
    if (length(matInfo.emissive) > 0.0)
    {
        radiance += matInfo.emissive;
    }

    for (int i = 0; i < 30; ++i)
    {
        geomInfo = getGeomInfo(rtPload);
        if (dot(-ray.direction, geomInfo.normal) < 0.0)
        {
            geomInfo.normal    = -geomInfo.normal;
            geomInfo.tangent   = -geomInfo.tangent;
            geomInfo.bitangent = -geomInfo.bitangent;
        }
        matInfo  = getMaterialInfo(geomInfo);

        // In a complete raytracing renderer, we will choise a light source(mesh light & env
        // light)
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
            vec3 ffnormal1 =
                dot(lightDir, geomInfo.normal) > 0.0 ? geomInfo.normal : -geomInfo.normal;
            Ray shadowRay;
            shadowRay.origin = offsetRay(geomInfo.position, ffnormal1);
            // geomInfo.position + 0.0001;
            shadowRay.direction = lightDir;
            shadowTrace(shadowRay);
            if (!shadowPload.isInShadow)
            {
                // ！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
                //   这里的G= 1.0可能有问题，因为你的微表面不一定正对这lightDir
                G = abs(dot(lightDir, geomInfo.normal));
            }
        }

        // 1.0 here is a placeholder, in future ,we will have many light source(include mesh
        // light
        // and env map light),and we gonna choose one of them randomly,and we will get a pdf
        // here
        float p1 = 1.0 * getEnvSamplePDF(2048, 1024, importanceUV);
        if (G > 0.0 && p1 > 0.0)
        {
            vec3 dir_in   = normalize(-ray.direction);
            BsdfEvaluateData bsdfEvaluateData;
            bsdfEvaluateData.k1 = dir_in;
            bsdfEvaluateData.k2 = lightDir;
            bsdfEvaluateData.xi = vec3(rand(rtPload.seed), rand(rtPload.seed), rand(rtPload.seed));
            bsdfEvaluate(bsdfEvaluateData, matInfo);
            vec3 bsdf     = bsdfEvaluateData.bsdf_diffuse + bsdfEvaluateData.bsdf_glossy;
            vec3 envLight = textureLod(envTexture, importanceUV, 0).xyz;
            c1            = G * bsdf * envLight;
            float p2      = bsdfEvaluateData.pdf * G;
            w1            = pow(p1, 2.0) / (pow(p1, 2.0) + pow(p2, 2.0));
            c1 /= p1;
        }

        radiance += throughput * c1 * w1;

        float rd1     = rand(rtPload.seed);
        float rd2     = rand(rtPload.seed);
        float rd3     = rand(rtPload.seed);
        BsdfSampleData bsdfSampleData;
        bsdfSampleData.k1 = -ray.direction;
        bsdfSampleData.xi = vec3(rd1, rd2, rd3);
        float materialEta = 1.8;
        float ior1        = 1.0;
        float ior2        = materialEta;
        if (dot(ray.direction, geomInfo.normal) > 0.0)
        {
            ior1 = materialEta;
            ior2 = 1.0;
        }
        matInfo.ior1 = ior1;
        matInfo.ior2 = ior2;

        bsdfSample(bsdfSampleData, matInfo);

        if (length(bsdfSampleData.k2) < 0.0001)
        {
            break;
        }
        {
            // ray spread shit, implement later
        }
        vec3 ffnormal2 =
            dot(bsdfSampleData.k2, geomInfo.normal) > 0.0 ? geomInfo.normal : -geomInfo.normal;
        Ray bsdfRay;
        bsdfRay.origin = offsetRay(geomInfo.position, ffnormal2);

        bsdfRay.direction = bsdfSampleData.k2;
        closestTrace(bsdfRay);
        G = 0.0;

        if (rtPload.hitT < INFINITY)
        {
            GeomInfo hitGeomInfo = getGeomInfo(rtPload);
            G                    = abs(dot(bsdfSampleData.k2, hitGeomInfo.normal));
        }
        else
        {
            G = 1.0;
        }

        if (bsdfSampleData.pdf <= 0.0)
        {
            break;
        }
        bsdfSampleData.pdf *= G;

        if (rtPload.hitT < INFINITY && false)
        {
            // this is mesh light shit,implement later
        }
        else if (rtPload.hitT == INFINITY)
        {
            vec2 envUV    = directionToSphericalEnvMap(bsdfSampleData.k2);
            vec3 envLight = textureLod(envTexture, envUV, 0).xyz;
            vec3 C2       = G * bsdfSampleData.bsdf_over_pdf * envLight;
            // 1.0 is same as before,it will be a specific value ,when we have more than one
            // light
            // source in scene, and currently ,we only have envmap
            float p1 = 1.0 * getEnvSamplePDF(2048, 1024, envUV);

            float w2 = pow(bsdfSampleData.pdf, 2.0) / (pow(p1, 2.0) + pow(bsdfSampleData.pdf, 2.0));

            radiance += throughput * C2 * w2;
            break;
        }

        // Russian roulette heuristics
        {
        }

        ray        = bsdfRay;
        throughput = G * bsdfSampleData.bsdf_over_pdf;
    }

    return radiance;
}

#endif // __pathtrace_H__