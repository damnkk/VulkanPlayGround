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

uint32_t packColor(const aiColor4D& color)
{
    auto toByte = [](float value) -> uint32_t
    {
        if (value < 0.0f)
        {
            value = 0.0f;
        }
        if (value > 1.0f)
        {
            value = 1.0f;
        }
        return static_cast<uint32_t>(value * 255.0f + 0.5f);
    };

    const uint32_t r = toByte(color.r);
    const uint32_t g = toByte(color.g);
    const uint32_t b = toByte(color.b);
    const uint32_t a = toByte(color.a);
    return r | (g << 8) | (b << 16) | (a << 24);
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

int findLocalTextureIndex(const AssetRegistry& assets, ModelAssetID modelID, const std::filesystem::path& sourcePath, const std::string& name,
                          bool embedded)
{
    const ModelAsset* model = assets.getModel(modelID);
    if (!model)
    {
        return -1;
    }

    for (uint32_t localIndex = 0; localIndex < model->localTextures.size(); ++localIndex)
    {
        const TextureAsset* texture = assets.getTexture(model->localTextures[localIndex]);
        if (!texture)
        {
            continue;
        }

        if (embedded)
        {
            if (texture->sourcePath.empty() && texture->name == name)
            {
                return static_cast<int>(localIndex);
            }
        }
        else if (texture->sourcePath == sourcePath)
        {
            return static_cast<int>(localIndex);
        }
    }

    return -1;
}

int ensureLocalTextureIndex(AssetRegistry& assets, ModelAssetID modelID, const std::filesystem::path& modelPath, const aiScene* assimpScene,
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

    const int existingLocalIndex = findLocalTextureIndex(assets, modelID, sourcePath, name, embedded);
    if (existingLocalIndex >= 0)
    {
        return existingLocalIndex;
    }

    if (embedded && assimpScene)
    {
        const char* embeddedName = texturePath.C_Str() + 1;
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

    TextureAssetID textureID = assets.registerTexture(name, sourcePath);
    TextureAsset*  texture   = assets.getTexture(textureID);
    if (texture)
    {
        texture->mipLevels = loadingCfg.textureMipLevels;
        texture->isSrgb    = isSrgb;
    }

    ModelAsset* model = assets.getModel(modelID);
    if (!model)
    {
        return -1;
    }

    const int localIndex = static_cast<int>(model->localTextures.size());
    assets.addModelLocalTexture(modelID, textureID);
    return localIndex;
}

void readTextureSlot(const aiMaterial* material, aiTextureType textureType, ImportedMaterialTextureSlot& slot, AssetRegistry& assets,
                     ModelAssetID modelID, const std::filesystem::path& modelPath, const aiScene* assimpScene,
                     const ModelLoadingConfig& loadingCfg, bool isSrgb)
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

    slot.localTextureIndex = ensureLocalTextureIndex(assets, modelID, modelPath, assimpScene, texturePath, loadingCfg, isSrgb);
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

ImportedMaterialDesc importMaterial(const aiMaterial* material, AssetRegistry& assets, ModelAssetID modelID,
                                    const std::filesystem::path& modelPath, const aiScene* assimpScene,
                                    const ModelLoadingConfig& loadingCfg, uint32_t materialIndex)
{
    ImportedMaterialDesc desc;
    desc.name = makeIndexedName("Material", materialIndex);
    if (!material)
    {
        return desc;
    }

    aiString name;
    if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length > 0)
    {
        desc.name = name.C_Str();
    }

    aiColor4D baseColor;
    if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &baseColor) == AI_SUCCESS ||
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &baseColor) == AI_SUCCESS)
    {
        desc.parameters.pbrBaseColorFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
    }

    aiColor4D emissive;
    if (aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissive) == AI_SUCCESS)
    {
        desc.parameters.emissiveFactor = glm::vec3(emissive.r, emissive.g, emissive.b);
    }

    float metallic = 0.0f;
    if (aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR, &metallic) == AI_SUCCESS)
    {
        desc.parameters.pbrMetallicFactor = metallic;
    }

    float roughness = 1.0f;
    if (aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS)
    {
        desc.parameters.pbrRoughnessFactor = roughness;
    }

    float opacity = 1.0f;
    if (aiGetMaterialFloat(material, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
    {
        desc.parameters.pbrBaseColorFactor.a = opacity;
    }

    int twoSided = 0;
    if (aiGetMaterialInteger(material, AI_MATKEY_TWOSIDED, &twoSided) == AI_SUCCESS)
    {
        desc.parameters.doubleSided = twoSided != 0 ? 1 : 0;
    }

    float alphaCutoff = 0.5f;
    if (aiGetMaterialFloat(material, AI_MATKEY_GLTF_ALPHACUTOFF, &alphaCutoff) == AI_SUCCESS)
    {
        desc.parameters.alphaCutoff = alphaCutoff;
    }

    aiString alphaMode;
    if (aiGetMaterialString(material, AI_MATKEY_GLTF_ALPHAMODE, &alphaMode) == AI_SUCCESS)
    {
        if (sameText(alphaMode.C_Str(), "MASK"))
        {
            desc.parameters.alphaMode = shaderio::eAlphaModeMask;
        }
        else if (sameText(alphaMode.C_Str(), "BLEND"))
        {
            desc.parameters.alphaMode = shaderio::eAlphaModeBlend;
        }
        else
        {
            desc.parameters.alphaMode = shaderio::eAlphaModeOpaque;
        }
    }
    else if (desc.parameters.pbrBaseColorFactor.a < 1.0f)
    {
        desc.parameters.alphaMode = shaderio::eAlphaModeBlend;
    }

    if (!loadingCfg.loadTextures)
    {
        return desc;
    }

    readTextureSlot(material, aiTextureType_BASE_COLOR, desc.textures.baseColor, assets, modelID, modelPath, assimpScene, loadingCfg,
                    loadingCfg.srgbBaseColorTextures);
    if (!desc.textures.baseColor.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE, desc.textures.baseColor, assets, modelID, modelPath, assimpScene, loadingCfg,
                        loadingCfg.srgbBaseColorTextures);
    }

    readTextureSlot(material, aiTextureType_NORMALS, desc.textures.normal, assets, modelID, modelPath, assimpScene, loadingCfg, false);
    if (!desc.textures.normal.hasTexture())
    {
        readTextureSlot(material, aiTextureType_NORMAL_CAMERA, desc.textures.normal, assets, modelID, modelPath, assimpScene, loadingCfg, false);
    }

    readTextureSlot(material, aiTextureType_GLTF_METALLIC_ROUGHNESS, desc.textures.metallicRoughness, assets, modelID, modelPath, assimpScene,
                    loadingCfg, false);
    if (!desc.textures.metallicRoughness.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE_ROUGHNESS, desc.textures.metallicRoughness, assets, modelID, modelPath, assimpScene,
                        loadingCfg, false);
    }

    readTextureSlot(material, aiTextureType_EMISSIVE, desc.textures.emissive, assets, modelID, modelPath, assimpScene, loadingCfg,
                    loadingCfg.srgbEmissiveTextures);
    readTextureSlot(material, aiTextureType_AMBIENT_OCCLUSION, desc.textures.occlusion, assets, modelID, modelPath, assimpScene, loadingCfg,
                    false);
    readTextureSlot(material, aiTextureType_SPECULAR, desc.textures.specular, assets, modelID, modelPath, assimpScene, loadingCfg, false);

    return desc;
}

void appendMeshGeometry(const aiMesh* mesh, ModelAsset& model, ModelSubmeshAsset& submesh)
{
    const uint32_t vertexBase = static_cast<uint32_t>(model.positions.size());
    submesh.vertexBase       = vertexBase;
    submesh.firstIndex       = static_cast<uint32_t>(model.indices.size());

    bool hasBounds = false;
    for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
    {
        const aiVector3D position = mesh->HasPositions() ? mesh->mVertices[vertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);
        const glm::vec3  p(position.x, position.y, position.z);
        model.positions.push_back(p);

        if (!hasBounds)
        {
            submesh.boundsMin = p;
            submesh.boundsMax = p;
            hasBounds         = true;
        }
        else
        {
            submesh.boundsMin = glm::min(submesh.boundsMin, p);
            submesh.boundsMax = glm::max(submesh.boundsMax, p);
        }

        if (mesh->HasNormals())
        {
            const aiVector3D normal = mesh->mNormals[vertexIndex];
            model.normals.push_back(glm::vec3(normal.x, normal.y, normal.z));
        }
        else
        {
            model.normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
        }

        if (mesh->HasTangentsAndBitangents())
        {
            const aiVector3D tangent = mesh->mTangents[vertexIndex];
            model.tangents.push_back(glm::vec4(tangent.x, tangent.y, tangent.z, 1.0f));
        }
        else
        {
            model.tangents.push_back(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        }

        if (mesh->HasTextureCoords(0))
        {
            const aiVector3D texCoord = mesh->mTextureCoords[0][vertexIndex];
            model.texCoords0.push_back(glm::vec2(texCoord.x, texCoord.y));
        }
        else
        {
            model.texCoords0.push_back(glm::vec2(0.0f));
        }

        if (mesh->HasTextureCoords(1))
        {
            const aiVector3D texCoord = mesh->mTextureCoords[1][vertexIndex];
            model.texCoords1.push_back(glm::vec2(texCoord.x, texCoord.y));
        }
        else
        {
            model.texCoords1.push_back(glm::vec2(0.0f));
        }

        if (mesh->HasVertexColors(0))
        {
            model.colors.push_back(packColor(mesh->mColors[0][vertexIndex]));
        }
        else
        {
            model.colors.push_back(0xFFFFFFFFu);
        }
    }

    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh->mFaces[faceIndex];
        if (face.mNumIndices != 3)
        {
            continue;
        }
        model.indices.push_back(face.mIndices[0]);
        model.indices.push_back(face.mIndices[1]);
        model.indices.push_back(face.mIndices[2]);
    }

    submesh.indexCount = static_cast<uint32_t>(model.indices.size()) - submesh.firstIndex;
}

struct AssimpImportContext
{
    AssetRegistry&         assets;
    CpuScene&              scene;
    ModelAssetID           modelID;
    std::vector<uint32_t>  meshSubmeshIndices;
};

void appendAssimpNode(const aiNode* assimpNode, CpuSceneNodeID parentNode, AssimpImportContext& context)
{
    if (!assimpNode)
    {
        return;
    }

    const std::string nodeName = makeAssimpName(assimpNode->mName, "Node", static_cast<uint32_t>(context.scene.getNodes().size()));
    CpuSceneNodeID    nodeID   = context.scene.create3DNode(nodeName, parentNode);
    context.scene.setLocalTransform(nodeID, toGlm(assimpNode->mTransformation));

    ModelAsset* model = context.assets.getModel(context.modelID);
    if (model && assimpNode->mNumMeshes > 0)
    {
        const uint32_t firstRenderable = static_cast<uint32_t>(model->renderables.size());
        uint32_t       renderableCount = 0;
        for (uint32_t meshSlot = 0; meshSlot < assimpNode->mNumMeshes; ++meshSlot)
        {
            const uint32_t meshIndex = assimpNode->mMeshes[meshSlot];
            if (meshIndex >= context.meshSubmeshIndices.size())
            {
                continue;
            }

            ModelRenderableTemplate renderable;
            renderable.submeshIndex = context.meshSubmeshIndices[meshIndex];
            renderable.localToModel = glm::mat4(1.0f);
            context.assets.addModelRenderable(context.modelID, renderable);
            ++renderableCount;
        }

        if (renderableCount > 0)
        {
            CpuModelComponent* modelComponent = context.scene.addComponent<CpuModelComponent>(nodeID);
            if (modelComponent)
            {
                modelComponent->model           = context.modelID;
                modelComponent->firstRenderable = firstRenderable;
                modelComponent->renderableCount = renderableCount;
            }
        }
    }

    for (uint32_t childIndex = 0; childIndex < assimpNode->mNumChildren; ++childIndex)
    {
        appendAssimpNode(assimpNode->mChildren[childIndex], nodeID, context);
    }
}

class AssimpFormatLoader : public ModelFormatLoader
{
public:
    explicit AssimpFormatLoader(ModelFileFormat format) : _format(format) {}

    bool canLoad(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg) const override
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

    ModelLoadResult load(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg, AssetRegistry& assets,
                         CpuScene& scene) const override
    {
        ModelLoadResult result;

        Assimp::Importer importer;
        importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, loadingCfg.globalScale);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

        uint32_t assimpFlags = loadingCfg.assimpPostProcessFlags | loadingCfg.extraAssimpProcessFlags;
        if (loadingCfg.globalScale != 1.0f)
        {
            assimpFlags |= aiProcess_GlobalScale;
        }

        const std::string pathUtf8     = nvutils::utf8FromPath(path);
        const aiScene*    assimpScene  = importer.ReadFile(pathUtf8, assimpFlags);
        if (!assimpScene || !assimpScene->mRootNode)
        {
            result.message = importer.GetErrorString();
            return result;
        }

        const std::string modelName = path.stem().string();
        result.model               = assets.registerModel(modelName, path);

        std::vector<MaterialAssetID> materialIDs;
        if (loadingCfg.loadMaterials && assimpScene->mNumMaterials > 0)
        {
            materialIDs.reserve(assimpScene->mNumMaterials);
            for (uint32_t materialIndex = 0; materialIndex < assimpScene->mNumMaterials; ++materialIndex)
            {
                ImportedMaterialDesc materialDesc =
                    importMaterial(assimpScene->mMaterials[materialIndex], assets, result.model, path, assimpScene, loadingCfg, materialIndex);
                materialIDs.push_back(assets.registerMaterialFromModelTextureTable(result.model, materialDesc));
            }
        }

        if (materialIDs.empty())
        {
            MaterialAsset defaultMaterial;
            defaultMaterial.name = modelName + "_DefaultMaterial";
            MaterialAssetID defaultMaterialID = assets.registerMaterial(defaultMaterial);
            ModelAsset*     model             = assets.getModel(result.model);
            if (model)
            {
                model->materials.push_back(defaultMaterialID);
            }
            materialIDs.push_back(defaultMaterialID);
        }

        ModelAsset* model = assets.getModel(result.model);
        if (!model)
        {
            result.message = "Model registration failed.";
            return result;
        }

        AssimpImportContext context{assets, scene, result.model, {}};
        context.meshSubmeshIndices.reserve(assimpScene->mNumMeshes);
        for (uint32_t meshIndex = 0; meshIndex < assimpScene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* mesh = assimpScene->mMeshes[meshIndex];
            if (!mesh)
            {
                context.meshSubmeshIndices.push_back(INVALID_SCENE_ID);
                continue;
            }

            ModelSubmeshAsset submesh;
            submesh.name = makeAssimpName(mesh->mName, "Submesh", meshIndex);
            appendMeshGeometry(mesh, *model, submesh);

            uint32_t materialIndex = mesh->mMaterialIndex;
            if (materialIndex >= materialIDs.size())
            {
                materialIndex = 0;
            }
            submesh.material = materialIDs[materialIndex];

            context.meshSubmeshIndices.push_back(assets.addModelSubmesh(result.model, submesh));
        }

        result.rootNode = scene.create3DNode(modelName, scene.rootNode());
        appendAssimpNode(assimpScene->mRootNode, result.rootNode, context);
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

ModelLoadResult loadModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg, AssetRegistry& assets,
                                  CpuScene& scene)
{
    const AssimpFormatLoader gltfLoader(ModelFileFormat::eGltf);
    const AssimpFormatLoader objLoader(ModelFileFormat::eObj);

    const ModelFormatLoader* loaders[] = {&gltfLoader, &objLoader};
    for (const ModelFormatLoader* loader : loaders)
    {
        if (loader->canLoad(path, loadingCfg))
        {
            return loader->load(path, loadingCfg, assets, scene);
        }
    }

    ModelLoadResult result;
    result.message = "Unsupported model format: " + path.extension().string();
    return result;
}

} // namespace Play
