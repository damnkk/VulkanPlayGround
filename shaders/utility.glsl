#ifndef _utility_H_
#define _utility_H_
#include "common.glsl"
#include "host_device.h"
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
    geomInfo.normal = pl.baryCoord.x * v2._normal + pl.baryCoord.y * v3._normal + weight3 * v1._normal;
    geomInfo.tangent = pl.baryCoord.x * v2._tangent + pl.baryCoord.y * v3._tangent + weight3 * v1._tangent;
    // geomInfo.bitangent = pl.baryCoord.x * v2.bitangent + pl.baryCoord.y * v3.bitangent + weight3 * v1.bitangent;
    geomInfo.uv = pl.baryCoord.x * v2._texCoord + pl.baryCoord.y * v3._texCoord + weight3 * v1._texCoord;
    return geomInfo;
}

#endif // _utility_H_

