#ifndef _utility_H_
#define _utility_H_
#include "common.glsl"
#include "host_device.h"
#include "constants.h"
GeomInfo getGeomInfo(PlayLoad pl)
{
    GeomInfo geomInfo;
    Mesh mesh = meshes[pl.instanceCustomIndex];
    Vertices vertices = Vertices(mesh._vertexAddress);
    Indices indices = Indices(mesh._indexAddress);
    ivec3 idx = ivec3(indices.i[pl.primitiveID]);
    Vertex v1 = vertices.v[idx.x];
    Vertex v2 = vertices.v[idx.y];
    Vertex v3 = vertices.v[idx.z];
    
    float weight3 = 1.0 - pl.baryCoord.x - pl.baryCoord.y;
    geomInfo.position = pl.baryCoord.x * v2._position + pl.baryCoord.y * v3._position + weight3 * v1._position;
    geomInfo.position = vec3(pl.objectToWorld * vec4(geomInfo.position, 1.0));
    geomInfo.normal = pl.baryCoord.x * v2._normal + pl.baryCoord.y * v3._normal + weight3 * v1._normal;
    geomInfo.normal   = normalize(vec3(pl.objectToWorld * vec4(geomInfo.normal, 0.0)));
    geomInfo.tangent = pl.baryCoord.x * v2._tangent + pl.baryCoord.y * v3._tangent + weight3 * v1._tangent;
    // geomInfo.bitangent = pl.baryCoord.x * v2.bitangent + pl.baryCoord.y * v3.bitangent + weight3 * v1.bitangent;
    geomInfo.uv = pl.baryCoord.x * v2._texCoord + pl.baryCoord.y * v3._texCoord + weight3 * v1._texCoord;
    geomInfo.materialIdx = mesh._materialIndex;
    return geomInfo;
}

MaterialInfo getMaterialInfo(GeomInfo geomInfo)
{
    Material mat = materials[geomInfo.materialIdx];

    MaterialInfo materialInfo;
    materialInfo.color    = vec3(0.6, 0.2, 0.8);
    materialInfo.emissiveFactor = vec3(0.0);
    materialInfo.normal   = geomInfo.normal;
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

void buildCoordSystem(vec3 normal, inout vec3 tangent, inout vec3 bitangent)
{
    vec3 helperVec = normalize(vec3(1.0, 0.0, 0.0));
    if (abs(dot(helperVec, normal)) > 0.9999)
    {
        helperVec = normalize(vec3(0.0, 0.0, 1.0));
    }
    tangent   = normalize(cross(normal, helperVec));
    bitangent = normalize(cross(normal, tangent));
}

// vec3 Environment_sample(sampler2D lat_long_tex, in vec3 randVal, out vec3 to_light, out float
// pdf)
// {
//     // Uniformly pick a texel index idx in the environment map
//     vec3  xi     = randVal;
//     uvec2 tsize  = textureSize(lat_long_tex, 0);
//     uint  width  = tsize.x;
//     uint  height = tsize.y;

//     const uint size = width * height;
//     const uint idx  = min(uint(xi.x * float(size)), size - 1);

//     // Fetch the sampling data for that texel, containing the ratio q between its
//     // emitted radiance and the average of the environment map, the texel alias,
//     // the probability distribution function (PDF) values for that texel and its
//     // alias
//     EnvAccel sample_data = envAccels[idx];

//     uint env_idx;

//     if (xi.y < sample_data.q)
//     {
//         // If the random variable is lower than the intensity ratio q, we directly pick
//         // this texel, and renormalize the random variable for later use. The PDF is the
//         // one of the texel itself
//         env_idx = idx;
//         xi.y /= sample_data.q;
//         pdf = sample_data.pdf;
//     }
//     else
//     {
//         // Otherwise we pick the alias of the texel, renormalize the random variable and use
//         // the PDF of the alias
//         env_idx = sample_data.alias;
//         xi.y    = (xi.y - sample_data.q) / (1.0f - sample_data.q);
//         pdf     = sample_data.aliasPdf;
//     }

//     // Compute the 2D integer coordinates of the texel
//     const uint px = env_idx % width;
//     uint       py = env_idx / width;

//     // Uniformly sample the solid angle subtended by the pixel.
//     // Generate both the UV for texture lookup and a direction in spherical coordinates
//     const float u       = float(px + xi.y) / float(width);
//     const float phi     = u * (2.0f * M_PI) - M_PI;
//     float       sin_phi = sin(phi);
//     float       cos_phi = cos(phi);

//     const float step_theta = M_PI / float(height);
//     const float theta0     = float(py) * step_theta;
//     const float cos_theta  = cos(theta0) * (1.0f - xi.z) + cos(theta0 + step_theta) * xi.z;
//     const float theta      = acos(cos_theta);
//     const float sin_theta  = sin(theta);
//     const float v          = theta * M_1_OVER_PI;

//     // Convert to a light direction vector in Cartesian coordinates
//     to_light = vec3(cos_phi * sin_theta, cos_theta, sin_phi * sin_theta);

//     // Lookup the environment value using bilinear filtering
//     return texture(lat_long_tex, vec2(u, v)).xyz;
// }

// vec4 EnvSample(inout vec3 radiance)
// {
//     vec3  lightDir;
//     float pdf;
//     vec3  randVal = vec3(rand(rtPload.seed), rand(rtPload.seed), rand(rtPload.seed));
//     radiance      = Environment_sample(envTexture, randVal, lightDir, pdf);
//     return vec4(lightDir, pdf);
// }

#endif // _utility_H_

