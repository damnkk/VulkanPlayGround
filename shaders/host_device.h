#ifndef HOST_DIVICE_H
#define HOST_DIVICE_H
#ifdef __cplusplus
#include "stdint.h"
using ivec2 = glm::ivec2;
using vec2  = glm::vec2;
using vec3  = glm::vec3;
using vec4  = glm::vec4;
using mat4  = glm::mat4;
using uint  = unsigned int;

#define ENUM_BEGIN(name) \
    enum name            \
    {
#define ENUM_END() }
#else
#define ENUM_BEGIN(name) const uint
#define ENUM_END()
#endif

struct Material
{
    // 0
    vec4 pbrBaseColorFactor;
    // 4
    int   pbrBaseColorTexture;
    float pbrMetallicFactor;
    float pbrRoughnessFactor;
    int   pbrMetallicRoughnessTexture;
    // 8
    int emissiveTexture;
    int _pad0;
    // 10
    vec3 emissiveFactor;
    int  alphaMode;
    // 14
    float alphaCutoff;
    int   doubleSided;
    int   normalTexture;
    float normalTextureScale;
    // 18
    mat4 uvTransform;
    // 22
    int unlit;

    float transmissionFactor;
    int   transmissionTexture;

    float ior;
    // 26
    vec3  anisotropyDirection;
    float anisotropy;
    // 30
    vec3  attenuationColor;
    float thicknessFactor; // 34
    int   thicknessTexture;
    float attenuationDistance;
    // --
    float clearcoatFactor;
    float clearcoatRoughness;
    // 38
    int  clearcoatTexture;
    int  clearcoatRoughnessTexture;
    uint sheen;
    int  _pad1;
};

struct Vertex
{
    vec3 _position;
    vec3 _normal;
    vec3 _tangent;
    vec2 _texCoord;
};

struct RenderUniform
{
    mat4 view;
    mat4 viewInverse;
    mat4 project;
    vec3 cameraPosition;

    uint frameCount;
};

struct EnvAccel
{
    uint  alias;
    float q;
    float pdf;
    float aliasPdf;
};

struct Constants
{
    mat4 model;
    uint matIdx;
};

struct Mesh
{
    uint64_t _vertexAddress;
    uint64_t _indexAddress;
    int      _materialIndex;
    uint     _faceCnt;
    uint     _vertCnt;
    uint     _vBufferIdx;
    uint     _iBufferIdx;
};


#endif // HOST_DIVICE_H