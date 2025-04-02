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

struct MaterialInfo
{
    vec3  baseColor;
    float roughness;
    float subsurface;
    float anisotropic;
    float eta;
    // -----------------------------unused-----------------------------
    int  emissiveTextureIdx;
    vec3 emissiveFactor;
};
struct GeomInfo{
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 position;
    vec2 uv;
    int  materialIdx;
};

struct VisibilityContribution
{
    vec3  radiance;  // Radiance at the point if light is visible
    vec3  lightDir;  // Direction to the light, to shoot shadow ray
    float lightDist; // Distance to the light (1e32 for infinite or sky)
    bool  visible;   // true if in front of the face and should shoot shadow ray
};

struct RayDifferential
{
    float radius;
    float spread;
};

#define INFINITY 1e32

#endif // __COMMON_H__