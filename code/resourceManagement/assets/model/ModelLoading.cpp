#include "ModelLoading.h"

#include "resourceManagement/assets/image/ImageLoading.h"
#include "core/Profiling.h"
#include "core/Utils.h"
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

struct ModelImportResult
{
    bool        success = false;
    LoadedModel model;
    std::string message;
};

struct ImportedTextureSlot
{
    int         localTextureIndex = -1;
    glm::mat2x3 uvTransform       = glm::mat2x3(1.0f);
    int         texCoord          = 0;

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

bool isGltfPath(const std::filesystem::path& path)
{
    const std::string extension = lowerAscii(path.extension().string());
    return extension == ".gltf" || extension == ".glb";
}

bool isObjPath(const std::filesystem::path& path)
{
    return lowerAscii(path.extension().string()) == ".obj";
}

bool isFbxPath(const std::filesystem::path& path)
{
    return lowerAscii(path.extension().string()) == ".fbx";
}

glm::mat4 toGlm(const aiMatrix4x4& matrix)
{
    return glm::mat4(matrix.a1, matrix.b1, matrix.c1, matrix.d1, matrix.a2, matrix.b2, matrix.c2, matrix.d2, matrix.a3, matrix.b3, matrix.c3,
                     matrix.d3, matrix.a4, matrix.b4, matrix.c4, matrix.d4);
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

uint32_t appendMeshGeometry(const aiMesh* mesh, LoadedModel& model, uint32_t materialIndex)
{
    if (!mesh)
    {
        return INVALID_SCENE_ID;
    }

    ModelGeometryData& geometry = model.geometry;
    ModelMeshAsset     range;
    range.firstVertex = static_cast<uint32_t>(geometry.positions.size());
    range.firstIndex  = static_cast<uint32_t>(geometry.indices.size());
    range.materialIdx = materialIndex;

    bool hasBounds = false;
    for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
    {
        const aiVector3D position = mesh->HasPositions() ? mesh->mVertices[vertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);
        const glm::vec3  p(position.x, position.y, position.z);
        geometry.positions.push_back(p);

        if (!hasBounds)
        {
            range.bbox.min = p;
            range.bbox.max = p;
            hasBounds      = true;
        }
        else
        {
            range.bbox.min = glm::min(range.bbox.min, p);
            range.bbox.max = glm::max(range.bbox.max, p);
        }

        if (mesh->HasNormals())
        {
            const aiVector3D normal = mesh->mNormals[vertexIndex];
            geometry.normals.push_back(glm::vec3(normal.x, normal.y, normal.z));
        }
        else
        {
            geometry.normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
        }

        if (mesh->HasTangentsAndBitangents())
        {
            const aiVector3D tangent = mesh->mTangents[vertexIndex];
            geometry.tangents.push_back(glm::vec4(tangent.x, tangent.y, tangent.z, 1.0f));
        }
        else
        {
            geometry.tangents.push_back(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        }

        if (mesh->HasTextureCoords(0))
        {
            const aiVector3D texCoord = mesh->mTextureCoords[0][vertexIndex];
            geometry.texCoords0.push_back(glm::vec2(texCoord.x, texCoord.y));
        }
        else
        {
            geometry.texCoords0.push_back(glm::vec2(0.0f));
        }

        if (mesh->HasTextureCoords(1))
        {
            const aiVector3D texCoord = mesh->mTextureCoords[1][vertexIndex];
            geometry.texCoords1.push_back(glm::vec2(texCoord.x, texCoord.y));
        }
        else
        {
            geometry.texCoords1.push_back(glm::vec2(0.0f));
        }

        if (mesh->HasVertexColors(0))
        {
            geometry.colors.push_back(packColor(mesh->mColors[0][vertexIndex]));
        }
        else
        {
            geometry.colors.push_back(0xFFFFFFFFu);
        }
    }

    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
    {
        if (mesh->mFaces[faceIndex].mNumIndices == 3)
        {
            geometry.indices.push_back(mesh->mFaces[faceIndex].mIndices[0]);
            geometry.indices.push_back(mesh->mFaces[faceIndex].mIndices[1]);
            geometry.indices.push_back(mesh->mFaces[faceIndex].mIndices[2]);
        }
    }

    range.vertexCount = mesh->mNumVertices;
    range.indexCount  = static_cast<uint32_t>(geometry.indices.size()) - range.firstIndex;

    const uint32_t meshID = static_cast<uint32_t>(model.meshes.size());
    model.meshes.push_back(range);
    return meshID;
}

int findLocalTextureIndex(const LoadedModel& model, const std::filesystem::path& sourcePath, const std::string& name, bool embedded)
{
    for (uint32_t localIndex = 0; localIndex < model.textures.size(); ++localIndex)
    {
        const ModelTextureAsset& texture = model.textures[localIndex];
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

class MaterialImportSession
{
public:
    MaterialImportSession(LoadedModel& model, const std::filesystem::path& modelPath, const aiScene* assimpScene,
                          const ModelLoadingConfig& loadingCfg)
        : _model(model), _modelPath(modelPath), _assimpScene(assimpScene), _loadingCfg(loadingCfg)
    {
    }

    shaderio::GltfShadeMaterial importMaterial(const aiMaterial* material);
    void                        loadTextureImages();

private:
    int  ensureLocalTextureIndex(const aiString& texturePath, bool isSrgb);
    void readTextureSlot(const aiMaterial* material, aiTextureType textureType, ImportedTextureSlot& slot, bool isSrgb);

    LoadedModel&                   _model;
    const std::filesystem::path&   _modelPath;
    const aiScene*                 _assimpScene = nullptr;
    const ModelLoadingConfig&      _loadingCfg;
};

int MaterialImportSession::ensureLocalTextureIndex(const aiString& texturePath, bool isSrgb)
{
    if (!_loadingCfg.loadTextures)
    {
        return -1;
    }

    const bool embedded = isEmbeddedTextureName(texturePath);
    if (embedded && !_loadingCfg.registerEmbeddedTexturePlaceholders)
    {
        return -1;
    }

    const std::filesystem::path sourcePath = embedded ? std::filesystem::path() : resolveTexturePath(_modelPath, texturePath);
    std::string                 name       = embedded ? texturePath.C_Str() : sourcePath.filename().string();
    if (name.empty())
    {
        name = texturePath.C_Str();
    }

    const int existingLocalIndex = findLocalTextureIndex(_model, sourcePath, name, embedded);
    if (existingLocalIndex >= 0)
    {
        return existingLocalIndex;
    }

    if (embedded && _assimpScene)
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
        if (embeddedIndex >= static_cast<int>(_assimpScene->mNumTextures))
        {
            return -1;
        }
    }

    ModelTextureAsset texture;
    texture.name       = name;
    texture.sourcePath = sourcePath;
    texture.mipLevels  = _loadingCfg.textureMipLevels;
    texture.isSrgb     = isSrgb;

    const int localIndex = static_cast<int>(_model.textures.size());
    _model.textures.push_back(std::move(texture));
    return localIndex;
}

void MaterialImportSession::readTextureSlot(const aiMaterial* material, aiTextureType textureType, ImportedTextureSlot& slot, bool isSrgb)
{
    if (!material || material->GetTextureCount(textureType) == 0)
    {
        return;
    }

    aiString     texturePath;
    unsigned int uvIndex = 0;
    if (material->GetTexture(textureType, 0, &texturePath, nullptr, &uvIndex) != AI_SUCCESS)
    {
        return;
    }

    slot.localTextureIndex = ensureLocalTextureIndex(texturePath, isSrgb);
    slot.texCoord          = static_cast<int>(uvIndex);

    aiUVTransform transform;
    if (material->Get(AI_MATKEY_UVTRANSFORM(textureType, 0), transform) == AI_SUCCESS)
    {
        const float c          = glm::cos(transform.mRotation);
        const float s          = glm::sin(transform.mRotation);
        slot.uvTransform[0][0] = transform.mScaling.x * c;
        slot.uvTransform[0][1] = transform.mScaling.x * s;
        slot.uvTransform[1][0] = -transform.mScaling.y * s;
        slot.uvTransform[1][1] = transform.mScaling.y * c;
        slot.uvTransform[0][2] = transform.mTranslation.x;
        slot.uvTransform[1][2] = transform.mTranslation.y;
    }
}

uint16_t appendTextureInfo(LoadedModel& model, const ImportedTextureSlot& slot)
{
    if (!slot.hasTexture() || model.textureInfos.size() >= 0xFFFF)
    {
        return 0;
    }

    shaderio::GltfTextureInfo textureInfo = shaderio::defaultGltfTextureInfo();
    textureInfo.index                     = slot.localTextureIndex;
    textureInfo.texCoord                  = slot.texCoord;
    textureInfo.uvTransform = shaderio::float3x2(slot.uvTransform[0][0], slot.uvTransform[1][0], slot.uvTransform[0][1], slot.uvTransform[1][1],
                                                 slot.uvTransform[0][2], slot.uvTransform[1][2]);

    const uint16_t textureInfoIndex = static_cast<uint16_t>(model.textureInfos.size());
    model.textureInfos.push_back(textureInfo);
    return textureInfoIndex;
}

shaderio::GltfShadeMaterial MaterialImportSession::importMaterial(const aiMaterial* material)
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
        importedMaterial.pbrBaseColorFactor.w = opacity;
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
        if (asciiEqualsIgnoreCase(alphaMode.C_Str(), "MASK"))
        {
            importedMaterial.alphaMode = shaderio::eAlphaModeMask;
        }
        else if (asciiEqualsIgnoreCase(alphaMode.C_Str(), "BLEND"))
        {
            importedMaterial.alphaMode = shaderio::eAlphaModeBlend;
        }
        else
        {
            importedMaterial.alphaMode = shaderio::eAlphaModeOpaque;
        }
    }
    else if (importedMaterial.pbrBaseColorFactor.z < 1.0f)
    {
        importedMaterial.alphaMode = shaderio::eAlphaModeBlend;
    }

    if (!_loadingCfg.loadTextures)
    {
        return importedMaterial;
    }

    ImportedTextureSlot baseColorSlot;
    readTextureSlot(material, aiTextureType_BASE_COLOR, baseColorSlot, _loadingCfg.srgbBaseColorTextures);
    if (!baseColorSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE, baseColorSlot, _loadingCfg.srgbBaseColorTextures);
    }
    importedMaterial.pbrBaseColorTexture = appendTextureInfo(_model, baseColorSlot);

    ImportedTextureSlot normalSlot;
    readTextureSlot(material, aiTextureType_NORMALS, normalSlot, false);
    if (!normalSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_NORMAL_CAMERA, normalSlot, false);
    }
    importedMaterial.normalTexture = appendTextureInfo(_model, normalSlot);

    ImportedTextureSlot metallicRoughnessSlot;
    readTextureSlot(material, aiTextureType_GLTF_METALLIC_ROUGHNESS, metallicRoughnessSlot, false);
    if (!metallicRoughnessSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE_ROUGHNESS, metallicRoughnessSlot, false);
    }
    importedMaterial.pbrMetallicRoughnessTexture = appendTextureInfo(_model, metallicRoughnessSlot);

    ImportedTextureSlot emissiveSlot;
    readTextureSlot(material, aiTextureType_EMISSIVE, emissiveSlot, _loadingCfg.srgbEmissiveTextures);
    importedMaterial.emissiveTexture = appendTextureInfo(_model, emissiveSlot);

    ImportedTextureSlot occlusionSlot;
    readTextureSlot(material, aiTextureType_AMBIENT_OCCLUSION, occlusionSlot, false);
    importedMaterial.occlusionTexture = appendTextureInfo(_model, occlusionSlot);

    ImportedTextureSlot specularSlot;
    readTextureSlot(material, aiTextureType_SPECULAR, specularSlot, false);
    importedMaterial.specularTexture = appendTextureInfo(_model, specularSlot);

    return importedMaterial;
}

void MaterialImportSession::loadTextureImages()
{
    PLAY_PROFILE_SCOPE("ModelLoading::loadImportedTextureImages");

    for (ModelTextureAsset& texture : _model.textures)
    {
        if (texture.sourcePath.empty())
        {
            continue;
        }

        ImageLoading::LoadTextureImage(texture.sourcePath, texture.isSrgb, texture.image);
    }
}

struct AssimpImportContext
{
    LoadedModel*          model = nullptr;
    std::vector<uint32_t> meshSubmeshIndices;
};

uint32_t appendAssimpNode(const aiNode* assimpNode, uint32_t parentNodeIndex, AssimpImportContext& context)
{
    if (!assimpNode || !context.model)
    {
        return INVALID_SCENE_ID;
    }

    ModelAsset& asset = context.model->asset;

    const glm::mat4 localTransform = toGlm(assimpNode->mTransformation);
    aiVector3D      scaling;
    aiVector3D      position;
    aiQuaternion    rotation;
    assimpNode->mTransformation.Decompose(scaling, rotation, position);

    ModelNodeAsset modelNode;
    modelNode.name         = makeAssimpName(assimpNode->mName, "Node", static_cast<uint32_t>(asset.nodes.size()));
    modelNode.parent       = parentNodeIndex;
    modelNode.transformIdx = static_cast<uint32_t>(asset.transforms.size());
    modelNode.translation  = glm::vec3(position.x, position.y, position.z);
    modelNode.rotation     = glm::vec3(rotation.x, rotation.y, rotation.z);
    modelNode.scale        = glm::vec3(scaling.x, scaling.y, scaling.z);

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

void configureFbxImport(Assimp::Importer& importer, const ModelLoadingConfig& loadingCfg)
{
    PLAY_PROFILE_SCOPE("ModelLoading::configureFbxImport");

    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_MATERIALS, loadingCfg.loadMaterials ? 1 : 0);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_TEXTURES, loadingCfg.loadTextures ? 1 : 0);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_CAMERAS, 0);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_LIGHTS, 0);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_ANIMATIONS, 0);
    importer.SetPropertyInteger(AI_CONFIG_IMPORT_FBX_READ_WEIGHTS, 0);
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
        if (_format == ModelFileFormat::eFbx)
        {
            return isFbxPath(path);
        }
        return false;
    }

    ModelImportResult import(const std::filesystem::path& path, const ModelLoadingConfig& loadingCfg) const override
    {
        PLAY_PROFILE_SCOPE("ModelLoading::Assimp import");

        ModelImportResult result;

        Assimp::Importer importer;
        {
            PLAY_PROFILE_SCOPE("ModelLoading::configure Assimp importer");
            importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, loadingCfg.globalScale);
            importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
            if (_format == ModelFileFormat::eFbx)
            {
                configureFbxImport(importer, loadingCfg);
            }
        }

        uint32_t assimpFlags = loadingCfg.assimpPostProcessFlags | loadingCfg.extraAssimpProcessFlags;
        if (loadingCfg.globalScale != 1.0f)
        {
            assimpFlags |= aiProcess_GlobalScale;
        }

        const std::string pathUtf8 = nvutils::utf8FromPath(path);
        const aiScene*    assimpScene = nullptr;
        {
            PLAY_PROFILE_SCOPE("ModelLoading::Assimp ReadFile");
            PLAY_PROFILE_MARK("Assimp ReadFile begin");
            assimpScene = importer.ReadFile(pathUtf8, assimpFlags);
            PLAY_PROFILE_MARK("Assimp ReadFile end");
        }
        if (!assimpScene || !assimpScene->mRootNode)
        {
            result.message = importer.GetErrorString();
            return result;
        }

        LoadedModel& model = result.model;
        {
            PLAY_PROFILE_SCOPE("ModelLoading::initialize loaded model");
            model.asset.name       = path.stem().string();
            model.asset.sourcePath = path;
            model.textureInfos.push_back(shaderio::defaultGltfTextureInfo());
        }

        MaterialImportSession materialImporter(model, path, assimpScene, loadingCfg);
        {
            PLAY_PROFILE_SCOPE("ModelLoading::import materials");
            if (loadingCfg.loadMaterials && assimpScene->mNumMaterials > 0)
            {
                model.materials.reserve(assimpScene->mNumMaterials);
                for (uint32_t materialIndex = 0; materialIndex < assimpScene->mNumMaterials; ++materialIndex)
                {
                    model.materials.push_back(materialImporter.importMaterial(assimpScene->mMaterials[materialIndex]));
                }
            }
        }

        if (model.materials.empty())
        {
            model.materials.push_back(shaderio::defaultGltfMaterial());
        }

        materialImporter.loadTextureImages();

        AssimpImportContext context;
        context.model = &model;
        context.meshSubmeshIndices.reserve(assimpScene->mNumMeshes);

        {
            PLAY_PROFILE_SCOPE("ModelLoading::import meshes");
            for (uint32_t meshIndex = 0; meshIndex < assimpScene->mNumMeshes; ++meshIndex)
            {
                const aiMesh* mesh = assimpScene->mMeshes[meshIndex];
                if (!mesh)
                {
                    context.meshSubmeshIndices.push_back(INVALID_SCENE_ID);
                    continue;
                }

                uint32_t materialIndex = mesh->mMaterialIndex;
                if (materialIndex >= model.materials.size())
                {
                    materialIndex = 0;
                }

                const uint32_t meshID = appendMeshGeometry(mesh, model, materialIndex);
                if (meshID == INVALID_SCENE_ID || meshID >= model.meshes.size())
                {
                    context.meshSubmeshIndices.push_back(INVALID_SCENE_ID);
                    continue;
                }

                ModelSubmeshAsset submesh;
                submesh.meshID = meshID;
                submesh.bbox   = model.meshes[meshID].bbox;

                const uint32_t submeshIndex = static_cast<uint32_t>(model.asset.submeshes.size());
                model.asset.submeshes.push_back(submesh);
                context.meshSubmeshIndices.push_back(submeshIndex);
            }
        }

        {
            PLAY_PROFILE_SCOPE("ModelLoading::import node hierarchy");
            if (appendAssimpNode(assimpScene->mRootNode, INVALID_SCENE_ID, context) == INVALID_SCENE_ID)
            {
                result.message = "Model node hierarchy import failed.";
                return result;
            }
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
           aiProcess_ImproveCacheLocality | aiProcess_SortByPType | aiProcess_FindInvalidData | aiProcess_GenBoundingBoxes | aiProcess_FlipUVs;
}

namespace
{

ModelImportResult importModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig)
{
    PLAY_PROFILE_SCOPE("ModelLoading::importModelFromFile");

    const AssimpFormatImporter gltfImporter(ModelFileFormat::eGltf);
    const AssimpFormatImporter objImporter(ModelFileFormat::eObj);
    const AssimpFormatImporter fbxImporter(ModelFileFormat::eFbx);

    const ModelFormatImporter* importers[] = {&gltfImporter, &objImporter, &fbxImporter};
    for (const ModelFormatImporter* importer : importers)
    {
        PLAY_PROFILE_SCOPE("ModelLoading::try importer");
        if (importer->canImport(path, loadingConfig))
        {
            return importer->import(path, loadingConfig);
        }
    }

    ModelImportResult result;
    result.message = "Unsupported model format: " + path.extension().string();
    return result;
}

} // namespace

ModelLoadResult model_loading::loadModelFromFile(const std::filesystem::path& path, const ModelLoadingConfig& loadingConfig)
{
    PLAY_PROFILE_SCOPE("model_loading::loadModelFromFile");

    ModelImportResult importResult = [&]()
    {
        PLAY_PROFILE_SCOPE("model_loading::import");
        return importModelFromFile(path, loadingConfig);
    }();
    if (!importResult.success)
    {
        ModelLoadResult result;
        result.message = importResult.message;
        return result;
    }

    ModelLoadResult result;
    result.success = true;
    result.model   = std::make_shared<LoadedModel>(std::move(importResult.model));
    return result;
}

} // namespace Play
