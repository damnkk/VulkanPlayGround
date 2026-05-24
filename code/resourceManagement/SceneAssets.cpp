#include "SceneAssets.h"

namespace Play
{

namespace
{
MaterialTextureSlot resolveImportedTextureSlot(const AssetRegistry& assets, ModelAssetID modelID, const ImportedMaterialTextureSlot& importedSlot)
{
    MaterialTextureSlot slot;
    slot.uvTransform = importedSlot.uvTransform;
    slot.texCoord    = importedSlot.texCoord;

    if (!importedSlot.hasTexture())
    {
        return slot;
    }

    const ModelAsset* model = assets.getModel(modelID);
    if (!model || static_cast<uint32_t>(importedSlot.localTextureIndex) >= model->localTextures.size())
    {
        return slot;
    }

    slot.texture = model->localTextures[importedSlot.localTextureIndex];
    return slot;
}
} // namespace

void AssetRegistry::clear()
{
    _textures.clear();
    _materials.clear();
    _models.clear();
}

TextureAssetID AssetRegistry::registerTexture(const std::string& name, const std::filesystem::path& sourcePath)
{
    TextureAsset asset;
    asset.name       = name;
    asset.sourcePath = sourcePath;

    const uint32_t index = static_cast<uint32_t>(_textures.size());
    _textures.push_back(asset);
    return makeTextureID(index);
}

TextureAssetID AssetRegistry::registerTexture(const std::string& name, const RefPtr<Texture>& texture)
{
    TextureAsset asset;
    asset.name    = name;
    asset.texture = texture;

    const uint32_t index = static_cast<uint32_t>(_textures.size());
    _textures.push_back(asset);
    return makeTextureID(index);
}

MaterialAssetID AssetRegistry::registerMaterial(const MaterialAsset& material)
{
    const uint32_t index = static_cast<uint32_t>(_materials.size());
    _materials.push_back(material);
    return makeMaterialID(index);
}

MaterialAssetID AssetRegistry::registerMaterialFromModelTextureTable(ModelAssetID modelID, const ImportedMaterialDesc& desc)
{
    MaterialAsset material;
    material.name       = desc.name;
    material.parameters = desc.parameters;

    material.textures.baseColor                  = resolveImportedTextureSlot(*this, modelID, desc.textures.baseColor);
    material.textures.normal                     = resolveImportedTextureSlot(*this, modelID, desc.textures.normal);
    material.textures.metallicRoughness          = resolveImportedTextureSlot(*this, modelID, desc.textures.metallicRoughness);
    material.textures.emissive                   = resolveImportedTextureSlot(*this, modelID, desc.textures.emissive);
    material.textures.transmission               = resolveImportedTextureSlot(*this, modelID, desc.textures.transmission);
    material.textures.thickness                  = resolveImportedTextureSlot(*this, modelID, desc.textures.thickness);
    material.textures.clearcoat                  = resolveImportedTextureSlot(*this, modelID, desc.textures.clearcoat);
    material.textures.clearcoatRoughness         = resolveImportedTextureSlot(*this, modelID, desc.textures.clearcoatRoughness);
    material.textures.clearcoatNormal            = resolveImportedTextureSlot(*this, modelID, desc.textures.clearcoatNormal);
    material.textures.specular                   = resolveImportedTextureSlot(*this, modelID, desc.textures.specular);
    material.textures.specularColor              = resolveImportedTextureSlot(*this, modelID, desc.textures.specularColor);
    material.textures.iridescence                = resolveImportedTextureSlot(*this, modelID, desc.textures.iridescence);
    material.textures.iridescenceThickness       = resolveImportedTextureSlot(*this, modelID, desc.textures.iridescenceThickness);
    material.textures.anisotropy                 = resolveImportedTextureSlot(*this, modelID, desc.textures.anisotropy);
    material.textures.sheenColor                 = resolveImportedTextureSlot(*this, modelID, desc.textures.sheenColor);
    material.textures.sheenRoughness             = resolveImportedTextureSlot(*this, modelID, desc.textures.sheenRoughness);
    material.textures.occlusion                  = resolveImportedTextureSlot(*this, modelID, desc.textures.occlusion);
    material.textures.pbrDiffuse                 = resolveImportedTextureSlot(*this, modelID, desc.textures.pbrDiffuse);
    material.textures.pbrSpecularGlossiness      = resolveImportedTextureSlot(*this, modelID, desc.textures.pbrSpecularGlossiness);
    material.textures.diffuseTransmission        = resolveImportedTextureSlot(*this, modelID, desc.textures.diffuseTransmission);
    material.textures.diffuseTransmissionColor   = resolveImportedTextureSlot(*this, modelID, desc.textures.diffuseTransmissionColor);

    MaterialAssetID materialID = registerMaterial(material);
    ModelAsset*     model     = getModel(modelID);
    if (model)
    {
        model->materials.push_back(materialID);
    }
    return materialID;
}

ModelAssetID AssetRegistry::registerModel(const std::string& name, const std::filesystem::path& sourcePath)
{
    ModelAsset asset;
    asset.name       = name;
    asset.sourcePath = sourcePath;

    const uint32_t index = static_cast<uint32_t>(_models.size());
    _models.push_back(asset);
    return makeModelID(index);
}

TextureAssetID AssetRegistry::addModelLocalTexture(ModelAssetID modelID, TextureAssetID textureID)
{
    ModelAsset* model = getModel(modelID);
    if (!model || !isValid(textureID))
    {
        return {};
    }

    model->localTextures.push_back(textureID);
    return textureID;
}

uint32_t AssetRegistry::addModelSubmesh(ModelAssetID modelID, const ModelSubmeshAsset& submesh)
{
    ModelAsset* model = getModel(modelID);
    if (!model)
    {
        return INVALID_SCENE_ID;
    }

    const uint32_t index = static_cast<uint32_t>(model->submeshes.size());
    model->submeshes.push_back(submesh);
    return index;
}

uint32_t AssetRegistry::addModelRenderable(ModelAssetID modelID, const ModelRenderableTemplate& renderable)
{
    ModelAsset* model = getModel(modelID);
    if (!model)
    {
        return INVALID_SCENE_ID;
    }

    const uint32_t index = static_cast<uint32_t>(model->renderables.size());
    model->renderables.push_back(renderable);
    return index;
}

bool AssetRegistry::isValid(TextureAssetID textureID) const
{
    return textureID.index < _textures.size() && _textures[textureID.index].generation == textureID.generation;
}

bool AssetRegistry::isValid(MaterialAssetID materialID) const
{
    return materialID.index < _materials.size() && _materials[materialID.index].generation == materialID.generation;
}

bool AssetRegistry::isValid(ModelAssetID modelID) const
{
    return modelID.index < _models.size() && _models[modelID.index].generation == modelID.generation;
}

TextureAsset* AssetRegistry::getTexture(TextureAssetID textureID)
{
    if (!isValid(textureID))
    {
        return nullptr;
    }
    return &_textures[textureID.index];
}

const TextureAsset* AssetRegistry::getTexture(TextureAssetID textureID) const
{
    if (!isValid(textureID))
    {
        return nullptr;
    }
    return &_textures[textureID.index];
}

MaterialAsset* AssetRegistry::getMaterial(MaterialAssetID materialID)
{
    if (!isValid(materialID))
    {
        return nullptr;
    }
    return &_materials[materialID.index];
}

const MaterialAsset* AssetRegistry::getMaterial(MaterialAssetID materialID) const
{
    if (!isValid(materialID))
    {
        return nullptr;
    }
    return &_materials[materialID.index];
}

ModelAsset* AssetRegistry::getModel(ModelAssetID modelID)
{
    if (!isValid(modelID))
    {
        return nullptr;
    }
    return &_models[modelID.index];
}

const ModelAsset* AssetRegistry::getModel(ModelAssetID modelID) const
{
    if (!isValid(modelID))
    {
        return nullptr;
    }
    return &_models[modelID.index];
}

TextureAssetID AssetRegistry::makeTextureID(uint32_t index) const
{
    TextureAssetID id;
    id.index      = index;
    id.generation = _textures[index].generation;
    return id;
}

MaterialAssetID AssetRegistry::makeMaterialID(uint32_t index) const
{
    MaterialAssetID id;
    id.index      = index;
    id.generation = _materials[index].generation;
    return id;
}

ModelAssetID AssetRegistry::makeModelID(uint32_t index) const
{
    ModelAssetID id;
    id.index      = index;
    id.generation = _models[index].generation;
    return id;
}

} // namespace Play
