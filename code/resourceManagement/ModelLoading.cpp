#include "ModelLoading.h"

#include "nvutils/file_operations.hpp"
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Play
{

namespace
{

struct ImportedTextureSlot
{
    int         localTextureIndex = -1;
    glm::mat2x3 uvTransform      = glm::mat2x3(1.0f);
    int         texCoord         = 0;

    bool hasTexture() const
    {
        return localTextureIndex >= 0;
    }
};

std::string lowerAscii(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return value;
}

bool sameText(const char* lhs, const char* rhs)
{
    if (!lhs || !rhs)
    {
        return false;
    }

    while (*lhs && *rhs)
    {
        char l = *lhs;
        char r = *rhs;
        if (l >= 'A' && l <= 'Z')
        {
            l = static_cast<char>(l - 'A' + 'a');
        }
        if (r >= 'A' && r <= 'Z')
        {
            r = static_cast<char>(r - 'A' + 'a');
        }
        if (l != r)
        {
            return false;
        }
        ++lhs;
        ++rhs;
    }

    return *lhs == *rhs;
}

bool isGltfPath(const std::filesystem::path& path)
{
    const std::string extension = lowerAscii(path.extension().string());
    return extension == ".gltf" || extension == ".glb";
}

bool isObjPath(const std::filesystem::path& path)
{
    return lowerAscii(path.extension().string()) == ".obj";
}

glm::mat4 toGlm(const aiMatrix4x4& matrix)
{
    return glm::mat4(matrix.a1, matrix.b1, matrix.c1, matrix.d1,
                     matrix.a2, matrix.b2, matrix.c2, matrix.d2,
                     matrix.a3, matrix.b3, matrix.c3, matrix.d3,
                     matrix.a4, matrix.b4, matrix.c4, matrix.d4);
}

std::string makeIndexedName(const char* prefix, uint32_t index)
{
    return std::string(prefix) + "_" + std::to_string(index);
}

std::string makeAssimpName(const aiString& name, const char* fallbackPrefix, uint32_t index)
{
    if (name.length > 0)
    {
        return name.C_Str();
    }
    return makeIndexedName(fallbackPrefix, index);
}

std::filesystem::path resolveTexturePath(const std::filesystem::path& modelPath, const aiString& texturePath)
{
    std::filesystem::path path(texturePath.C_Str());
    if (path.is_relative())
    {
        path = modelPath.parent_path() / path;
    }
    return path.lexically_normal();
}

bool isEmbeddedTextureName(const aiString& texturePath)
{
    return texturePath.length > 0 && texturePath.C_Str()[0] == '*';
}

AABB computeBounds(const aiMesh* mesh)
{
    AABB bounds;
    bool hasBounds = false;

    if (!mesh)
    {
        return bounds;
    }

    for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
    {
        const aiVector3D position = mesh->HasPositions() ? mesh->mVertices[vertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);
        const glm::vec3  p(position.x, position.y, position.z);

        if (!hasBounds)
        {
            bounds.min = p;
            bounds.max = p;
            hasBounds  = true;
        }
        else
        {
            bounds.min = glm::min(bounds.min, p);
            bounds.max = glm::max(bounds.max, p);
        }
    }

    return bounds;
}

uint32_t countTriangleIndices(const aiMesh* mesh)
{
    uint32_t indexCount = 0;
    if (!mesh)
    {
        return indexCount;
    }

    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
    {
        if (mesh->mFaces[faceIndex].mNumIndices == 3)
        {
            indexCount += 3;
        }
    }
    return indexCount;
}

int findLocalTextureIndex(const ModelAssetPackage& package, const std::filesystem::path& sourcePath, const std::string& name, bool embedded)
{
    for (uint32_t localIndex = 0; localIndex < package.textures.size(); ++localIndex)
    {
        const ModelTextureResource& texture = package.textures[localIndex];
        if (embedded)
        {
            if (texture.sourcePath.empty() && texture.name == name)
            {
                return static_cast<int>(localIndex);
            }
        }
        else if (texture.sourcePath == sourcePath)
        {
            return static_cast<int>(localIndex);
        }
    }

    return -1;
}

int ensureLocalTextureIndex(ModelAssetPackage& package, const std::filesystem::path& modelPath, const aiScene* assimpScene,
                            const aiString& texturePath, const ModelLoadingConfig& loadingCfg, bool isSrgb)
{
    if (!loadingCfg.loadTextures)
    {
        return -1;
    }

    const bool embedded = isEmbeddedTextureName(texturePath);
    if (embedded && !loadingCfg.registerEmbeddedTexturePlaceholders)
    {
        return -1;
    }

    const std::filesystem::path sourcePath = embedded ? std::filesystem::path() : resolveTexturePath(modelPath, texturePath);
    std::string                 name       = embedded ? texturePath.C_Str() : sourcePath.filename().string();
    if (name.empty())
    {
        name = texturePath.C_Str();
    }

    const int existingLocalIndex = findLocalTextureIndex(package, sourcePath, name, embedded);
    if (existingLocalIndex >= 0)
    {
        return existingLocalIndex;
    }

    if (embedded && assimpScene)
    {
        const char* embeddedName  = texturePath.C_Str() + 1;
        int         embeddedIndex = 0;
        while (*embeddedName)
        {
            if (*embeddedName < '0' || *embeddedName > '9')
            {
                return -1;
            }
            embeddedIndex = embeddedIndex * 10 + (*embeddedName - '0');
            ++embeddedName;
        }
        if (embeddedIndex >= static_cast<int>(assimpScene->mNumTextures))
        {
            return -1;
        }
    }

    ModelTextureResource texture;
    texture.name       = name;
    texture.sourcePath = sourcePath;
    texture.mipLevels  = loadingCfg.textureMipLevels;
    texture.isSrgb     = isSrgb;

    const int localIndex = static_cast<int>(package.textures.size());
    package.textures.push_back(texture);
    return localIndex;
}

void readTextureSlot(const aiMaterial* material, aiTextureType textureType, ImportedTextureSlot& slot, ModelAssetPackage& package,
                     const std::filesystem::path& modelPath, const aiScene* assimpScene, const ModelLoadingConfig& loadingCfg, bool isSrgb)
{
    if (!material || material->GetTextureCount(textureType) == 0)
    {
        return;
    }

    aiString texturePath;
    unsigned int uvIndex = 0;
    if (material->GetTexture(textureType, 0, &texturePath, nullptr, &uvIndex) != AI_SUCCESS)
    {
        return;
    }

    slot.localTextureIndex = ensureLocalTextureIndex(package, modelPath, assimpScene, texturePath, loadingCfg, isSrgb);
    slot.texCoord          = static_cast<int>(uvIndex);

    aiUVTransform transform;
    if (material->Get(AI_MATKEY_UVTRANSFORM(textureType, 0), transform) == AI_SUCCESS)
    {
        const float c = glm::cos(transform.mRotation);
        const float s = glm::sin(transform.mRotation);
        slot.uvTransform[0][0] = transform.mScaling.x * c;
        slot.uvTransform[0][1] = transform.mScaling.x * s;
        slot.uvTransform[1][0] = -transform.mScaling.y * s;
        slot.uvTransform[1][1] = transform.mScaling.y * c;
        slot.uvTransform[0][2] = transform.mTranslation.x;
        slot.uvTransform[1][2] = transform.mTranslation.y;
    }
}

uint16_t appendTextureInfo(ModelAssetPackage& package, const ImportedTextureSlot& slot)
{
    if (!slot.hasTexture() || package.textureInfos.size() >= 0xFFFF)
    {
        return 0;
    }

    shaderio::GltfTextureInfo textureInfo = shaderio::defaultGltfTextureInfo();
    textureInfo.index    = slot.localTextureIndex;
    textureInfo.texCoord = slot.texCoord;
    textureInfo.uvTransform =
        shaderio::float3x2(slot.uvTransform[0][0], slot.uvTransform[1][0], slot.uvTransform[0][1], slot.uvTransform[1][1],
                           slot.uvTransform[0][2], slot.uvTransform[1][2]);

    const uint16_t textureInfoIndex = static_cast<uint16_t>(package.textureInfos.size());
    package.textureInfos.push_back(textureInfo);
    return textureInfoIndex;
}

shaderio::GltfShadeMaterial importMaterial(const aiMaterial* material, ModelAssetPackage& package, const std::filesystem::path& modelPath,
                                           const aiScene* assimpScene, const ModelLoadingConfig& loadingCfg)
{
    shaderio::GltfShadeMaterial importedMaterial = shaderio::defaultGltfMaterial();
    if (!material)
    {
        return importedMaterial;
    }

    aiColor4D baseColor;
    if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &baseColor) == AI_SUCCESS ||
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &baseColor) == AI_SUCCESS)
    {
        importedMaterial.pbrBaseColorFactor = shaderio::float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
    }

    aiColor4D emissive;
    if (aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissive) == AI_SUCCESS)
    {
        importedMaterial.emissiveFactor = shaderio::float3(emissive.r, emissive.g, emissive.b);
    }

    float metallic = 0.0f;
    if (aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR, &metallic) == AI_SUCCESS)
    {
        importedMaterial.pbrMetallicFactor = metallic;
    }

    float roughness = 1.0f;
    if (aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS)
    {
        importedMaterial.pbrRoughnessFactor = roughness;
    }

    float opacity = 1.0f;
    if (aiGetMaterialFloat(material, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
    {
        importedMaterial.pbrBaseColorFactor.a = opacity;
    }

    int twoSided = 0;
    if (aiGetMaterialInteger(material, AI_MATKEY_TWOSIDED, &twoSided) == AI_SUCCESS)
    {
        importedMaterial.doubleSided = twoSided != 0 ? 1 : 0;
    }

    float alphaCutoff = 0.5f;
    if (aiGetMaterialFloat(material, AI_MATKEY_GLTF_ALPHACUTOFF, &alphaCutoff) == AI_SUCCESS)
    {
        importedMaterial.alphaCutoff = alphaCutoff;
    }

    aiString alphaMode;
    if (aiGetMaterialString(material, AI_MATKEY_GLTF_ALPHAMODE, &alphaMode) == AI_SUCCESS)
    {
        if (sameText(alphaMode.C_Str(), "MASK"))
        {
            importedMaterial.alphaMode = shaderio::eAlphaModeMask;
        }
        else if (sameText(alphaMode.C_Str(), "BLEND"))
        {
            importedMaterial.alphaMode = shaderio::eAlphaModeBlend;
        }
        else
        {
            importedMaterial.alphaMode = shaderio::eAlphaModeOpaque;
        }
    }
    else if (importedMaterial.pbrBaseColorFactor.a < 1.0f)
    {
        importedMaterial.alphaMode = shaderio::eAlphaModeBlend;
    }

    if (!loadingCfg.loadTextures)
    {
        return importedMaterial;
    }

    ImportedTextureSlot baseColorSlot;
    readTextureSlot(material, aiTextureType_BASE_COLOR, baseColorSlot, package, modelPath, assimpScene, loadingCfg,
                    loadingCfg.srgbBaseColorTextures);
    if (!baseColorSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE, baseColorSlot, package, modelPath, assimpScene, loadingCfg,
                        loadingCfg.srgbBaseColorTextures);
    }
    importedMaterial.pbrBaseColorTexture = appendTextureInfo(package, baseColorSlot);

    ImportedTextureSlot normalSlot;
    readTextureSlot(material, aiTextureType_NORMALS, normalSlot, package, modelPath, assimpScene, loadingCfg, false);
    if (!normalSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_NORMAL_CAMERA, normalSlot, package, modelPath, assimpScene, loadingCfg, false);
    }
    importedMaterial.normalTexture = appendTextureInfo(package, normalSlot);

    ImportedTextureSlot metallicRoughnessSlot;
    readTextureSlot(material, aiTextureType_GLTF_METALLIC_ROUGHNESS, metallicRoughnessSlot, package, modelPath, assimpScene, loadingCfg, false);
    if (!metallicRoughnessSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE_ROUGHNESS, metallicRoughnessSlot, package, modelPath, assimpScene, loadingCfg, false);
    }
    importedMaterial.pbrMetallicRoughnessTexture = appendTextureInfo(package, metallicRoughnessSlot);

    ImportedTextureSlot emissiveSlot;
    readTextureSlot(material, aiTextureType_EMISSIVE, emissiveSlot, package, modelPath, assimpScene, loadingCfg,
                    loadingCfg.srgbEmissiveTextures);
    importedMaterial.emissiveTexture = appendTextureInfo(package, emissiveSlot);

    ImportedTextureSlot occlusionSlot;
    readTextureSlot(material, aiTextureType_AMBIENT_OCCLUSION, occlusionSlot, package, modelPath, assimpScene, loadingCfg, false);
    importedMaterial.occlusionTexture = appendTextureInfo(package, occlusionSlot);

    ImportedTextureSlot specularSlot;
    readTextureSlot(material, aiTextureType_SPECULAR, specularSlot, package, modelPath, assimpScene, loadingCfg, false);
    importedMaterial.specularTexture = appendTextureInfo(package, specularSlot);

    return importedMaterial;
}

struct AssimpImportContext
{
    ModelAssetPackage*    package = nullptr;
    std::vector<uint32_t> meshSubmeshIndices;
};

uint32_t appendAssimpNode(const aiNode* assimpNode, uint32_t parentNodeIndex, AssimpImportContext& context)
{
    if (!assimpNode || !context.package)
    {
        return INVALID_SCENE_ID;
    }

    ModelAsset& asset = context.package->asset;

    const glm::mat4 localTransform = toGlm(assimpNode->mTransformation);
    aiVector3D      scaling;
    aiVector3D      position;
    aiQuaternion    rotation;
    assimpNode->mTransformation.Decompose(scaling, rotation, position);

    ModelNodeAsset modelNode;
    modelNode.name           = makeAssimpName(assimpNode->mName, "Node", static_cast<uint32_t>(asset.nodes.size()));
    modelNode.parent         = parentNodeIndex;
    modelNode.transformIdx   = static_cast<uint32_t>(asset.transforms.size());
    modelNode.translation    = glm::vec3(position.x, position.y, position.z);
    modelNode.rotation       = glm::vec3(rotation.x, rotation.y, rotation.z);
    modelNode.scale          = glm::vec3(scaling.x, scaling.y, scaling.z);

    const uint32_t nodeIndex = static_cast<uint32_t>(asset.nodes.size());
    asset.transforms.push_back(localTransform);
    asset.nodes.push_back(modelNode);

    if (asset.rootNode == INVALID_SCENE_ID)
    {
        asset.rootNode = nodeIndex;
    }

    for (uint32_t meshSlot = 0; meshSlot < assimpNode->mNumMeshes; ++meshSlot)
    {
        const uint32_t meshIndex = assimpNode->mMeshes[meshSlot];
        if (meshIndex < context.meshSubmeshIndices.size() && context.meshSubmeshIndices[meshIndex] != INVALID_SCENE_ID)
        {
            asset.nodes[nodeIndex].submeshIdx.push_back(context.meshSubmeshIndices[meshIndex]);
        }
    }

    uint32_t previousChildIndex = INVALID_SCENE_ID;
    for (uint32_t childIndex = 0; childIndex < assimpNode->mNumChildren; ++childIndex)
    {
        const uint32_t childNodeIndex = appendAssimpNode(assimpNode->mChildren[childIndex], nodeIndex, context);
        if (childNodeIndex == INVALID_SCENE_ID)
        {
            continue;
        }

        if (previousChildIndex == INVALID_SCENE_ID)
        {
            asset.nodes[nodeIndex].firstChild = childNodeIndex;
        }
        else if (previousChildIndex < asset.nodes.size())
        {
            asset.nodes[previousChildIndex].nextSibling = childNodeIndex;
        }

        previousChildIndex = childNodeIndex;
    }

    return nodeIndex;
}

class ModelFormatImporter
{
public:
    virtual ~ModelFormatImporter() = default;

    virtual bool              canImport(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg) const = 0;
    virtual ModelImportResult import(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg) const    = 0;
};

class AssimpFormatImporter : public ModelFormatImporter
{
public:
    explicit AssimpFormatImporter(ModelFileFormat format) : _format(format) {}

    bool canImport(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg) const override
    {
        if (loadingCfg.format != ModelFileFormat::eAuto)
        {
            return loadingCfg.format == _format;
        }

        if (_format == ModelFileFormat::eGltf)
        {
            return isGltfPath(path);
        }
        if (_format == ModelFileFormat::eObj)
        {
            return isObjPath(path);
        }
        return false;
    }

    ModelImportResult import(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg) const override
    {
        ModelImportResult result;

        Assimp::Importer importer;
        importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, loadingCfg.globalScale);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

        uint32_t assimpFlags = loadingCfg.assimpPostProcessFlags | loadingCfg.extraAssimpProcessFlags;
        if (loadingCfg.globalScale != 1.0f)
        {
            assimpFlags |= aiProcess_GlobalScale;
        }

        const std::string pathUtf8    = nvutils::utf8FromPath(path);
        const aiScene*    assimpScene = importer.ReadFile(pathUtf8, assimpFlags);
        if (!assimpScene || !assimpScene->mRootNode)
        {
            result.message = importer.GetErrorString();
            return result;
        }

        ModelAssetPackage& package = result.model.package;
        package.asset.name         = path.stem().string();
        package.asset.sourcePath   = path;
        package.textureInfos.push_back(shaderio::defaultGltfTextureInfo());

        if (loadingCfg.loadMaterials && assimpScene->mNumMaterials > 0)
        {
            package.materials.reserve(assimpScene->mNumMaterials);
            for (uint32_t materialIndex = 0; materialIndex < assimpScene->mNumMaterials; ++materialIndex)
            {
                package.materials.push_back(importMaterial(assimpScene->mMaterials[materialIndex], package, path, assimpScene, loadingCfg));
            }
        }

        if (package.materials.empty())
        {
            package.materials.push_back(shaderio::defaultGltfMaterial());
        }

        AssimpImportContext context;
        context.package = &package;
        context.meshSubmeshIndices.reserve(assimpScene->mNumMeshes);

        for (uint32_t meshIndex = 0; meshIndex < assimpScene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* mesh = assimpScene->mMeshes[meshIndex];
            if (!mesh)
            {
                context.meshSubmeshIndices.push_back(INVALID_SCENE_ID);
                continue;
            }

            MeshInfo meshInfo;
            meshInfo.vertexBufferAddress = 0;
            meshInfo.IndexBufferAddress  = 0;
            meshInfo.indexCount          = countTriangleIndices(mesh);

            uint32_t materialIndex = mesh->mMaterialIndex;
            if (materialIndex >= package.materials.size())
            {
                materialIndex = 0;
            }
            meshInfo.materialIdx = materialIndex;

            ModelSubmeshAsset submesh;
            submesh.meshID = static_cast<uint32_t>(package.meshInfos.size());
            submesh.bbox   = computeBounds(mesh);

            const uint32_t submeshIndex = static_cast<uint32_t>(package.asset.submeshes.size());
            package.meshInfos.push_back(meshInfo);
            package.asset.submeshes.push_back(submesh);
            context.meshSubmeshIndices.push_back(submeshIndex);
        }

        if (appendAssimpNode(assimpScene->mRootNode, INVALID_SCENE_ID, context) == INVALID_SCENE_ID)
        {
            result.message = "Model node hierarchy import failed.";
            return result;
        }

        result.success = true;
        return result;
    }

private:
    ModelFileFormat _format = ModelFileFormat::eAuto;
};

} // namespace

uint32_t ModelLoadingConfig::DefaultAssimpPostProcessFlags()
{
    return aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
           aiProcess_ImproveCacheLocality | aiProcess_SortByPType | aiProcess_FindInvalidData | aiProcess_GenBoundingBoxes |
           aiProcess_TransformUVCoords;
}

ModelImportResult model_loading::importModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig)
{
    const AssimpFormatImporter gltfImporter(ModelFileFormat::eGltf);
    const AssimpFormatImporter objImporter(ModelFileFormat::eObj);

    const ModelFormatImporter* importers[] = {&gltfImporter, &objImporter};
    for (const ModelFormatImporter* importer : importers)
    {
        if (importer->canImport(path, loadingConfig))
        {
            return importer->import(path, loadingConfig);
        }
    }

    ModelImportResult result;
    result.message = "Unsupported model format: " + path.extension().string();
    return result;
}

ModelOptimizeResult model_loading::optimizeModel(ImportedModel&& importedModel, const ModelLoadingConfig& loadingConfig)
{
    (void) loadingConfig;

    ModelOptimizeResult result;
    result.success       = true;
    result.model.package = std::move(importedModel.package);
    return result;
}

ModelLoadResult model_loading::uploadModel(OptimizedModel&& optimizedModel, const ModelLoadingConfig& loadingConfig)
{
    ModelLoadResult result;
    result.success = true;
    result.model   = std::move(optimizedModel.package);

    if (loadingConfig.loadTextures)
    {
        for (ModelTextureResource& texture : result.model.textures)
        {
            if (!texture.texture && !texture.sourcePath.empty())
            {
                texture.texture = RefPtr<Texture>(
                    new Texture(texture.sourcePath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.mipLevels, texture.isSrgb));
            }
        }
    }

    return result;
}

ModelLoadResult model_loading::loadModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig)
{
    ModelImportResult importResult = importModelFromFile(path, loadingConfig);
    if (!importResult.success)
    {
        ModelLoadResult result;
        result.message = importResult.message;
        return result;
    }

    ModelOptimizeResult optimizeResult = optimizeModel(std::move(importResult.model), loadingConfig);
    if (!optimizeResult.success)
    {
        ModelLoadResult result;
        result.message = optimizeResult.message;
        return result;
    }

    return uploadModel(std::move(optimizeResult.model), loadingConfig);
}

} // namespace Play
