#ifndef _utility_H_
#define _utility_H_
#include "common.glsl"
#include "host_device.h"
#include "constants.h"

void buildCoordSystem(vec3 normal, inout vec3 tangent, inout vec3 bitangent)
{
    vec3 helperVec = vec3(1.0, 0.0, 0.0);
    if (abs(dot(helperVec, normal)) > 0.9999)
    {
        helperVec = normalize(vec3(0.0, 0.0, 1.0));
    }
    tangent   = normalize(cross(normal, helperVec));
    bitangent = normalize(cross(normal, tangent));
}

vec3 toNormalHemisphere(vec3 v, vec3 N)
{
    vec3 helper = vec3(1, 0, 0);
    if (abs(N.x) > 0.999999) helper = vec3(0, 0, 1);
    vec3 tangent   = normalize(cross(N, helper));
    vec3 bitangent = normalize(cross(N, tangent));
    return v.x * tangent + v.y * bitangent + v.z * N;
}
GeomInfo getGeomInfo(PlayLoad pl)
{
    GeomInfo geomInfo;
    Mesh     mesh     = meshes[pl.instanceCustomIndex];
    Vertices vertices = Vertices(mesh._vertexAddress);
    Indices  indices  = Indices(mesh._indexAddress);
    ivec3    idx      = ivec3(indices.i[pl.primitiveID]);
    Vertex   v1       = vertices.v[idx.x];
    Vertex   v2       = vertices.v[idx.y];
    Vertex   v3       = vertices.v[idx.z];

    float weight3 = 1.0 - pl.baryCoord.x - pl.baryCoord.y;
    geomInfo.position =
        pl.baryCoord.x * v2._position + pl.baryCoord.y * v3._position + weight3 * v1._position;
    geomInfo.position = vec3(pl.objectToWorld * vec4(geomInfo.position, 1.0));
    geomInfo.normal =
        pl.baryCoord.x * v2._normal + pl.baryCoord.y * v3._normal + weight3 * v1._normal;
    geomInfo.normal = normalize(vec3(pl.objectToWorld * vec4(geomInfo.normal, 0.0)));
    vec3 helperVec  = normalize(vec3(1.0, 0.0, 0.0));
    if (abs(dot(helperVec, geomInfo.normal)) > 0.9999)
    {
        helperVec = normalize(vec3(0.0, 0.0, 1.0));
    }
    geomInfo.tangent   = normalize(cross(geomInfo.normal, helperVec));
    geomInfo.bitangent = normalize(cross(geomInfo.normal, geomInfo.tangent));
    geomInfo.uv =
        pl.baryCoord.x * v2._texCoord + pl.baryCoord.y * v3._texCoord + weight3 * v1._texCoord;
    geomInfo.materialIdx = mesh._materialIndex;
    return geomInfo;
}

PbrMaterial getMaterialInfo(inout GeomInfo geomInfo)
{
    Material    mat = materials[geomInfo.materialIdx];
    PbrMaterial materialInfo;
    materialInfo                           = defaultPbrMaterial();
    materialInfo.Ng                        = geomInfo.normal;
    materialInfo.T                         = geomInfo.tangent;
    materialInfo.B                         = geomInfo.bitangent;
    materialInfo.N                         = geomInfo.normal;
    materialInfo.roughness                 = vec2(0.01, 0.01);
    materialInfo.metallic                  = 0.0;
    materialInfo.transmission              = 1.0;
    materialInfo.isThinWalled              = false;
    materialInfo.diffuseTransmissionFactor = 1.0;

    if (mat.normalTexture != -1)
    {
        vec3 smaplenormal =
            texture(sceneTextures[nonuniformEXT(mat.normalTexture)], geomInfo.uv).xyz * 2.0 - 1.0;
        materialInfo.N = normalize(toNormalHemisphere(smaplenormal, geomInfo.normal));
    }
    return materialInfo;
}

vec2 directionToSphericalEnvMap(vec3 dir)
{
    dir     = normalize(dir);
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    uv /= vec2(2.0 * M_PI, M_PI);
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}

vec3 sphericalEnvMapToDirection(vec2 uv)
{
    uv.y        = 1.0 - uv.y;
    float phi   = 2.0 * M_PI * (uv.x - 0.5);
    float theta = M_PI * (uv.y - 0.5);
    return vec3(cos(theta) * cos(phi), sin(theta), cos(theta) * sin(phi));
}

vec3 OffsetRay(in vec3 p, in vec3 n)
{
    const float intScale   = 256.0f;
    const float floatScale = 1.0f / 65536.0f;
    const float origin     = 1.0f / 32.0f;

    ivec3 of_i = ivec3(intScale * n.x, intScale * n.y, intScale * n.z);

    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
                    intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
                    intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return vec3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x, //
                abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y, //
                abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

float getEnvSamplePDF(float envWidth, float height, vec2 importanceUV)
{
    float pdf       = texture(envLookupTexture, importanceUV).z;
    float theta     = M_PI * importanceUV.y;
    float sin_theta = max(sin(theta), 1e-10);
    float p_convert = float(envWidth * envWidth) / (2.0 * M_PI * M_PI * sin_theta);
    return pdf * p_convert;
}

float Fd(vec3 dir_in, vec3 dir_out, vec3 v, vec3 n, MaterialInfo mat)
{
    vec3  h    = normalize(dir_in + dir_out);
    float Fd90 = 0.5 + 2.0 * mat.roughness * abs(dot(h, dir_out) * dot(h, dir_out));
    return (1.0 + (Fd90 - 1.0) * pow(1.0 - abs(dot(n, v)), 5));
}

float Fss(vec3 dir_in, vec3 dir_out, vec3 v, vec3 n, MaterialInfo mat)
{
    vec3  h     = normalize(dir_in + dir_out);
    float Fss90 = mat.roughness * abs(dot(h, dir_out)) * abs(dot(h, dir_out));
    return (1.0 + (Fss90 - 1.0) * pow(1.0 - abs(dot(n, v)), 5));
}

float fresnel_dielectric(float n_dot_i, float n_dot_t, float eta)
{
    float rs = (n_dot_i - eta * n_dot_t) / (n_dot_i + eta * n_dot_t);
    float rp = (eta * n_dot_i - n_dot_t) / (eta * n_dot_i + n_dot_t);
    float F  = (rs * rs + rp * rp) / 2.0;
    return F;
}

float fresnel_dielectric(float n_dot_i, float eta)
{
    float n_dot_t_sq = 1.0 - (1.0 - n_dot_i * n_dot_i) / (eta * eta);
    if (n_dot_t_sq < 0.0)
    {
        // total internal reflection
        return 1.0;
    }
    float n_dot_t = sqrt(n_dot_t_sq);
    return fresnel_dielectric(abs(n_dot_i), n_dot_t, eta);
}

vec3 toNormalLocal(vec3 v, GeomInfo geomInfo)
{
    return vec3(dot(v, geomInfo.tangent), dot(v, geomInfo.bitangent), dot(v, geomInfo.normal));
}

vec3 desampleVisible_normals(vec3 local_dir_in, float alpha, vec2 rnd_param)
{
    vec3 hemi_dir_in =
        normalize(vec3(alpha * local_dir_in.x, alpha * local_dir_in.y, local_dir_in.z));
    float r     = sqrt(rnd_param.x);
    float phi   = 2 * M_PI * rnd_param.y;
    float t1    = r * cos(phi);
    float t2    = r * sin(phi);
    float s     = (1 + hemi_dir_in.z) / 2;
    t2          = (1 - s) * sqrt(1 - t1 * t1) + s * t2;
    vec3 disk_N = vec3(t1, t2, sqrt(max(0.0, 1.0 - pow(t1, 2.0) - pow(t2, 2.0))));
    vec3 hemi_N = toNormalHemisphere(hemi_dir_in, disk_N);
    return normalize(vec3(alpha * hemi_N.x, alpha * hemi_N.y, max(0.0, hemi_N.z)));
}

vec3 sampleVisible_normals(vec3 local_dir_in, float alpha, vec2 rnd_param)
{
    if (local_dir_in.z < 0)
    {
        return -desampleVisible_normals(-local_dir_in, alpha, rnd_param);
    }
    vec3 hemi_dir_in =
        normalize(vec3(alpha * local_dir_in.x, alpha * local_dir_in.y, local_dir_in.z));
    float r     = sqrt(rnd_param.x);
    float phi   = 2 * M_PI * rnd_param.y;
    float t1    = r * cos(phi);
    float t2    = r * sin(phi);
    float s     = (1 + hemi_dir_in.z) / 2;
    t2          = (1 - s) * sqrt(1 - t1 * t1) + s * t2;
    vec3 disk_N = vec3(t1, t2, sqrt(max(0.0, 1.0 - pow(t1, 2.0) - pow(t2, 2.0))));
    vec3 hemi_N = toNormalHemisphere(disk_N, hemi_dir_in);
    return normalize(vec3(alpha * hemi_N.x, alpha * hemi_N.y, max(0.0, hemi_N.z)));
}

float GTR(vec3 v, float ax, float ay, GeomInfo geomInfo)
{
    vec3  wl = toNormalLocal(v, geomInfo);
    float A =
        (sqrt(1.0 + ((pow(wl.x * ax, 2.0) + pow(wl.y * ay, 2.0)) / pow(wl.z, 2.0))) - 1.0) / 2.0;
    return 1.0 / (1.0 + A);
}

vec3 OffsetHitPos(in vec3 p, in vec3 n)
{
    const float intScale   = 256.0f;
    const float floatScale = 1.0f / 65536.0f;
    const float origin     = 1.0f / 32.0f;

    ivec3 of_i = ivec3(intScale * n.x, intScale * n.y, intScale * n.z);

    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
                    intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
                    intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return vec3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x, //
                abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y, //
                abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

#endif // _utility_H_
