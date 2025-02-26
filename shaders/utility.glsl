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

MaterialInfo getMaterialInfo(int materialIdx)
{
    MaterialInfo materialInfo;
    materialInfo.color    = vec3(1.0);
    materialInfo.emissive = vec3(0.0);
    return materialInfo;
}

vec2 directionToSphericalEnvMap(vec3 dir)
{
    dir     = normalize(dir);
    vec2 uv = vec2(atan(dir.y, dir.x), asin(dir.z));
    uv /= vec2(2.0 * M_PI, M_PI);
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}

// inline vec3 cosineSampleHemisphere(float r1, float r2)
// {
//     float r   = sqrt(r1);
//     float phi = M_TWO_PI * r2;
//     vec3  dir;
//     dir.x = r * cos(phi);
//     dir.y = r * sin(phi);
//     dir.z = sqrt(1.F - r1);
//     return dir;
// }

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

#endif // _utility_H_

