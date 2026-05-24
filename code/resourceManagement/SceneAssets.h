#ifndef SCENE_ASSETS_H
#define SCENE_ASSETS_H

#include "CpuScene.h"
#include <filesystem>
#include "nvshaders/gltf_scene_io.h.slang"
#include "Resource.h"

namespace Play
{

struct TextureAssetID
{
    uint32_t index      = INVALID_SCENE_ID;
    uint32_t generation = 0;

    bool isValid() const
    {
        return index != INVALID_SCENE_ID;
    }
};

struct MaterialAssetID
{
    uint32_t index      = INVALID_SCENE_ID;
    uint32_t generation = 0;

    bool isValid() const
    {
        return index != INVALID_SCENE_ID;
    }
};

struct TextureAsset
{
    std::string           name;
    std::filesystem::path sourcePath;
    RefPtr<Texture>       texture;
    uint32_t              generation    = 1;
    uint32_t              bindlessIndex = INVALID_SCENE_ID;
    uint32_t              mipLevels     = 1;
    bool                  isSrgb        = true;

    bool isResident() const
    {
        return texture && texture->isValid();
    }
};

struct MaterialTextureSlot
{
    TextureAssetID texture;
    glm::mat2x3    uvTransform = glm::mat2x3(1.0f);
    int            texCoord    = 0;

    bool hasTexture() const
    {
        return texture.isValid();
    }
};

struct ImportedMaterialTextureSlot
{
    int         localTextureIndex = -1;
    glm::mat2x3 uvTransform      = glm::mat2x3(1.0f);
    int         texCoord         = 0;

    bool hasTexture() const
    {
        return localTextureIndex >= 0;
    }
};

struct MaterialTextureSet
{
    MaterialTextureSlot baseColor;
    MaterialTextureSlot normal;
    MaterialTextureSlot metallicRoughness;
    MaterialTextureSlot emissive;
    MaterialTextureSlot transmission;
    MaterialTextureSlot thickness;
    MaterialTextureSlot clearcoat;
    MaterialTextureSlot clearcoatRoughness;
    MaterialTextureSlot clearcoatNormal;
    MaterialTextureSlot specular;
    MaterialTextureSlot specularColor;
    MaterialTextureSlot iridescence;
    MaterialTextureSlot iridescenceThickness;
    MaterialTextureSlot anisotropy;
    MaterialTextureSlot sheenColor;
    MaterialTextureSlot sheenRoughness;
    MaterialTextureSlot occlusion;
    MaterialTextureSlot pbrDiffuse;
    MaterialTextureSlot pbrSpecularGlossiness;
    MaterialTextureSlot diffuseTransmission;
    MaterialTextureSlot diffuseTransmissionColor;
};

struct ImportedMaterialTextureSet
{
    ImportedMaterialTextureSlot baseColor;
    ImportedMaterialTextureSlot normal;
    ImportedMaterialTextureSlot metallicRoughness;
    ImportedMaterialTextureSlot emissive;
    ImportedMaterialTextureSlot transmission;
    ImportedMaterialTextureSlot thickness;
    ImportedMaterialTextureSlot clearcoat;
    ImportedMaterialTextureSlot clearcoatRoughness;
    ImportedMaterialTextureSlot clearcoatNormal;
    ImportedMaterialTextureSlot specular;
    ImportedMaterialTextureSlot specularColor;
    ImportedMaterialTextureSlot iridescence;
    ImportedMaterialTextureSlot iridescenceThickness;
    ImportedMaterialTextureSlot anisotropy;
    ImportedMaterialTextureSlot sheenColor;
    ImportedMaterialTextureSlot sheenRoughness;
    ImportedMaterialTextureSlot occlusion;
    ImportedMaterialTextureSlot pbrDiffuse;
    ImportedMaterialTextureSlot pbrSpecularGlossiness;
    ImportedMaterialTextureSlot diffuseTransmission;
    ImportedMaterialTextureSlot diffuseTransmissionColor;
};

struct MaterialAsset
{
    std::string                   name;
    shaderio::GltfShadeMaterial   parameters = shaderio::defaultGltfMaterial();
    MaterialTextureSet            textures;
    uint32_t                      generation = 1;
};

struct ImportedMaterialDesc
{
    std::string                  name;
    shaderio::GltfShadeMaterial  parameters = shaderio::defaultGltfMaterial();
    ImportedMaterialTextureSet   textures;
};

struct ModelSubmeshAsset
{
    std::string     name;
    uint32_t        firstIndex  = 0;
    uint32_t        indexCount  = 0;
    uint32_t        vertexBase  = 0;
    MaterialAssetID material;
    glm::vec3       boundsMin = glm::vec3(0.0f);
    glm::vec3       boundsMax = glm::vec3(0.0f);
};

struct ModelNodeAsset
{
    std::string name;
    uint32_t    parent          = INVALID_SCENE_ID;
    uint32_t    firstChild      = INVALID_SCENE_ID;
    uint32_t    nextSibling     = INVALID_SCENE_ID;
    glm::mat4   localTransform  = glm::mat4(1.0f);
    glm::mat4   modelTransform  = glm::mat4(1.0f);
    uint32_t    firstRenderable = 0;
    uint32_t    renderableCount = 0;
};

struct ModelRenderableTemplate
{
    uint32_t  submeshIndex = INVALID_SCENE_ID;
    uint32_t  nodeIndex    = INVALID_SCENE_ID;
    glm::mat4 localToModel = glm::mat4(1.0f);
};

struct ModelAsset
{
    std::string                         name;
    std::filesystem::path               sourcePath;
    std::vector<glm::vec3>              positions;
    std::vector<glm::vec3>              normals;
    std::vector<glm::vec4>              tangents;
    std::vector<glm::vec2>              texCoords0;
    std::vector<glm::vec2>              texCoords1;
    std::vector<uint32_t>               colors;
    std::vector<uint32_t>               indices;
    std::vector<TextureAssetID>          localTextures;
    std::vector<MaterialAssetID>         materials;
    std::vector<ModelNodeAsset>          nodes;
    std::vector<ModelSubmeshAsset>       submeshes;
    std::vector<ModelRenderableTemplate> renderables;
    uint32_t                             generation = 1;
};

class AssetRegistry
{
public:
    void clear();

    TextureAssetID  registerTexture(const std::string& name, const std::filesystem::path& sourcePath);
    TextureAssetID  registerTexture(const std::string& name, const RefPtr<Texture>& texture);
    MaterialAssetID registerMaterial(const MaterialAsset& material);
    MaterialAssetID registerMaterialFromModelTextureTable(ModelAssetID modelID, const ImportedMaterialDesc& desc);
    ModelAssetID    registerModel(const std::string& name, const std::filesystem::path& sourcePath);

    TextureAssetID addModelLocalTexture(ModelAssetID modelID, TextureAssetID textureID);
    uint32_t       addModelNode(ModelAssetID modelID, const ModelNodeAsset& node);
    uint32_t       addModelSubmesh(ModelAssetID modelID, const ModelSubmeshAsset& submesh);
    uint32_t       addModelRenderable(ModelAssetID modelID, const ModelRenderableTemplate& renderable);

    bool isValid(TextureAssetID textureID) const;
    bool isValid(MaterialAssetID materialID) const;
    bool isValid(ModelAssetID modelID) const;

    TextureAsset*       getTexture(TextureAssetID textureID);
    const TextureAsset* getTexture(TextureAssetID textureID) const;
    MaterialAsset*       getMaterial(MaterialAssetID materialID);
    const MaterialAsset* getMaterial(MaterialAssetID materialID) const;
    ModelAsset*         getModel(ModelAssetID modelID);
    const ModelAsset*   getModel(ModelAssetID modelID) const;

    const std::vector<TextureAsset>& getTextures() const
    {
        return _textures;
    }

    const std::vector<MaterialAsset>& getMaterials() const
    {
        return _materials;
    }

    const std::vector<ModelAsset>& getModels() const
    {
        return _models;
    }

private:
    TextureAssetID  makeTextureID(uint32_t index) const;
    MaterialAssetID makeMaterialID(uint32_t index) const;
    ModelAssetID    makeModelID(uint32_t index) const;

    std::vector<TextureAsset>  _textures;
    std::vector<MaterialAsset> _materials;
    std::vector<ModelAsset>    _models;
};

} // namespace Play

#endif // SCENE_ASSETS_H
