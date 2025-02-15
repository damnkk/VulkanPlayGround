#ifndef __COMMON_H__
#define __COMMON_H__
struct PlayLoad
{
    uint   seed;
    float  hitT;
    int    primitiveID;
    int    instanceID;
    int    instanceCustomIndex;
    vec2   baryCoord;
    mat4x3 objectToWorld;
    mat4x3 worldToObject;
};

struct ShadowPlayLoad
{
    bool isInShadow;
};

struct Ray{
    vec3 origin;
    vec3 direction;
};

struct GeomInfo{
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 position;
    vec2 uv;
};

struct MaterialInfo{
vec3 color;
};

#define INFINITY 1e32

#endif // __COMMON_H__