#ifndef SCENE_ASSETS_H
#define SCENE_ASSETS_H

#include "CpuScene.h"
#include <filesystem>
#include "nvshaders/gltf_scene_io.h.slang"
#include "Resource.h"

namespace Play
{

struct AABB
{
    glm::vec3 min = {0.0f, 0.0f, 0.0f};
    glm::vec3 max = {0.0f, 0.0f, 0.0f};
};

struct ModelSubmeshAsset
{
    uint32_t meshID = INVALID_SCENE_ID; // idx to meshInfos
    AABB     bbox;
};

struct ModelNodeAsset
{
    std::string           name;
    uint32_t              parent       = INVALID_SCENE_ID;
    uint32_t              firstChild   = INVALID_SCENE_ID;
    uint32_t              nextSibling  = INVALID_SCENE_ID;
    uint32_t              transformIdx = INVALID_SCENE_ID;
    glm::vec3             translation  = {0.0f, 0.0f, 0.0f};
    glm::vec3             rotation     = {0.0f, 0.0f, 0.0f};
    glm::vec3             scale        = {1.0f, 1.0f, 1.0f};
    std::vector<uint32_t> submeshIdx;
};

struct ModelRenderableTemplate
{
    uint32_t  submeshIndex = INVALID_SCENE_ID;
    uint32_t  nodeIndex    = INVALID_SCENE_ID;
    glm::mat4 localToModel = glm::mat4(1.0f);
};

struct RayTracingASInfo
{
    VkAccelerationStructureCreateInfoKHR createInfo;
};

struct MeshInfo
{
    uint64_t vertexBufferAddress;
    uint64_t IndexBufferAddress;
    uint32_t indexCount;
    uint32_t materialIdx;
};

struct VertexStreamInfo
{
    uint64_t positionBufferAddress  = 0;
    uint64_t normalBufferAddress    = 0;
    uint64_t tangentBufferAddress   = 0;
    uint64_t texCoord0BufferAddress = 0;
    uint64_t texCoord1BufferAddress = 0;
    uint64_t colorBufferAddress     = 0;
};

struct LightInfo
{
    glm::vec3 lightPosition;
};

struct GltfShadeMaterial
{
    glm::vec3 pbrBaseColorFactor; // offset 0    - 16 bytes
    glm::vec3 emissiveFactor;     // offset 16   - 12 bytes
    float     normalTextureScale; // offset 28   - 4 bytes

    float pbrRoughnessFactor; // offset 32   - 4 bytes
    float pbrMetallicFactor;  // offset 36   - 4 bytes
    int   alphaMode;          // offset 40   - 4 bytes
    float alphaCutoff;        // offset 44   - 4 bytes

    glm::vec3 attenuationColor; // offset 48   - 12 bytes
    float     ior;              // offset 60   - 4 bytes

    float transmissionFactor;  // offset 64   - 4 bytes
    float thicknessFactor;     // offset 68   - 4 bytes
    float attenuationDistance; // offset 72   - 4 bytes
    float clearcoatFactor;     // offset 76   - 4 bytes

    glm::vec3 specularColorFactor; // offset 80   - 12 bytes
    float     clearcoatRoughness;  // offset 92   - 4 bytes

    float specularFactor;              // offset 96   - 4 bytes
    int   unlit;                       // offset 100  - 4 bytes
    float iridescenceFactor;           // offset 104  - 4 bytes
    float iridescenceThicknessMaximum; // offset 108  - 4 bytes

    float     iridescenceThicknessMinimum; // offset 112  - 4 bytes
    float     iridescenceIor;              // offset 116  - 4 bytes
    glm::vec2 anisotropyRotation;          // offset 120  - 8 bytes

    glm::vec3 sheenColorFactor;   // offset 128  - 12 bytes
    float     anisotropyStrength; // offset 140  - 4 bytes

    float sheenRoughnessFactor;     // offset 144  - 4 bytes
    float occlusionStrength;        // offset 148  - 4 bytes
    float dispersion;               // offset 152  - 4 bytes
    int   usePbrSpecularGlossiness; // offset 156  - 4 bytes

    glm::vec4 pbrDiffuseFactor;    // offset 160  - 16 bytes
    glm::vec3 pbrSpecularFactor;   // offset 176  - 12 bytes
    float     pbrGlossinessFactor; // offset 188  - 4 bytes

    glm::vec3 diffuseTransmissionColor;  // offset 192  - 12 bytes
    float     diffuseTransmissionFactor; // offset 204  - 4 bytes

    glm::vec3 multiscatterColorFactor; // offset 208  - 12 bytes  // KHR_materials_volume_scatter
    float     scatterAnisotropy;       // offset 220  - 4 bytes   // KHR_materials_volume_scatter

    int doubleSided; // offset 224  - 4 bytes
    // Texture infos (uint16_t, 2 bytes each)
    uint16_t pbrBaseColorTexture;             // offset 228  - 2 bytes
    uint16_t normalTexture;                   // offset 230  - 2 bytes
    uint16_t pbrMetallicRoughnessTexture;     // offset 232  - 2 bytes
    uint16_t emissiveTexture;                 // offset 234  - 2 bytes
    uint16_t transmissionTexture;             // offset 236  - 2 bytes
    uint16_t thicknessTexture;                // offset 238  - 2 bytes
    uint16_t clearcoatTexture;                // offset 240  - 2 bytes
    uint16_t clearcoatRoughnessTexture;       // offset 242  - 2 bytes
    uint16_t clearcoatNormalTexture;          // offset 244  - 2 bytes
    uint16_t specularTexture;                 // offset 246  - 2 bytes
    uint16_t specularColorTexture;            // offset 248  - 2 bytes
    uint16_t iridescenceTexture;              // offset 250  - 2 bytes
    uint16_t iridescenceThicknessTexture;     // offset 252  - 2 bytes
    uint16_t anisotropyTexture;               // offset 254  - 2 bytes
    uint16_t sheenColorTexture;               // offset 256  - 2 bytes
    uint16_t sheenRoughnessTexture;           // offset 258  - 2 bytes
    uint16_t occlusionTexture;                // offset 260  - 2 bytes
    uint16_t pbrDiffuseTexture;               // offset 262  - 2 bytes
    uint16_t pbrSpecularGlossinessTexture;    // offset 264  - 2 bytes
    uint16_t diffuseTransmissionTexture;      // offset 266  - 2 bytes
    uint16_t diffuseTransmissionColorTexture; // offset 268  - 2 bytes
    uint16_t _pad1;                           // offset 270  - 2 bytes
                                              // Total size: 272 bytes
};

struct ModelAsset
{
    std::string                    name;
    std::filesystem::path          sourcePath;
    std::vector<ModelSubmeshAsset> submeshes;
    std::vector<ModelNodeAsset>    nodes;
    std::vector<glm::mat4>         transforms;
    uint32_t                       rootNode = INVALID_SCENE_ID;

    RefPtr<Buffer>                transformBuffer = nullptr;
    RefPtr<Buffer>                materialBuffer  = nullptr;
    RefPtr<Buffer>                meshInfoBuffer  = nullptr;
    RefPtr<Buffer>                lightInfoBuffer = nullptr;
    std::vector<RayTracingASInfo> accelerationStructures;

    uint32_t generation = 1;
};

struct ModelTextureResource
{
    std::string           name;
    std::filesystem::path sourcePath;
    RefPtr<Texture>       texture;
    uint32_t              mipLevels = 1;
    bool                  isSrgb    = true;

    bool isResident() const
    {
        return texture && texture->isValid();
    }
};

struct ModelMeshRange
{
    uint32_t firstVertex = 0;
    uint32_t vertexCount = 0;
    uint32_t firstIndex  = 0;
    uint32_t indexCount  = 0;
    uint32_t materialIdx = 0;
    AABB     bbox;
};

struct ModelGeometryPayload
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec2> texCoords0;
    std::vector<glm::vec2> texCoords1;
    std::vector<uint32_t>  colors;
    std::vector<uint32_t>  indices;
    std::vector<ModelMeshRange> ranges;

    bool empty() const
    {
        return positions.empty() || indices.empty() || ranges.empty();
    }
};

struct ModelAssetPackage
{
    ModelAsset                               asset;
    ModelGeometryPayload                     geometry;
    std::vector<MeshInfo>                    meshInfos;
    std::vector<shaderio::GltfShadeMaterial> materials;
    std::vector<shaderio::GltfTextureInfo>   textureInfos;
    std::vector<ModelTextureResource>        textures;
    std::vector<RefPtr<Buffer>>              ownedBuffers;
};

} // namespace Play

#endif // SCENE_ASSETS_H
