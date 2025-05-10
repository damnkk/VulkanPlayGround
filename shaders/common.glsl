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

// volume rendering
struct VolumeUniform
{
    mat4 ProjectMatrix;
    mat4 ViewMatrix;
    mat4 WorldMatrix;
    mat4 NormalMatrix;

    mat4 InvProjectMatrix;
    mat4 InvViewMatrix;
    mat4 InvWorldMatrix;
    mat4 InvNormalMatrix;

    uint  frameCount;
    float StepSize;
    vec2  FrameOffset;

    vec2  RTDimensions;
    float Density;
    float Exposure;
    vec3  CameraPos;
    vec3  BBoxMin;
    vec3  BBoxMax;
};

struct AABB
{
    vec3 Min;
    vec3 Max;
};

vec3 GetNormalizedTexcoord(vec3 position, AABB aabb)
{
    return (position - aabb.Min) / (aabb.Max - aabb.Min);
}

struct Intersection
{
    float Min;
    float Max;
};

struct VolumeRay
{
    vec3  origin;
    vec3  direction;
    float Min;
    float Max;
};
const float  FLT_EPSILON = 1.192092896e-07f;
const float  FLT_MAX     = 1000f;
const float  FLT_MIN     = 1.175494351e-38f;
Intersection IntersectAABB(VolumeRay ray, AABB aabb)
{
    Intersection intersect;
    intersect.Min   = +FLT_MAX;
    intersect.Max   = -FLT_MAX;
    const vec3 invR = 1.0 / ray.direction;
    const vec3 bot  = invR * (aabb.Min - ray.origin);
    const vec3 top  = invR * (aabb.Max - ray.origin);
    const vec3 tmin = min(top, bot);
    const vec3 tmax = max(top, bot);

    const float largestMin = max(tmin.x, max(tmin.y, tmin.z));
    const float largestMax = min(tmax.x, min(tmax.y, tmax.z));

    intersect.Min = largestMin;
    intersect.Max = largestMax;
    return intersect;
}


#endif // __COMMON_H__