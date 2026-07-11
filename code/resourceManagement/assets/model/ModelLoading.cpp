#include "ModelLoading.h"

#include "resourceManagement/assets/image/ImageLoading.h"
#include "resourceManagement/vulkan/resources/PlayAllocator.h"
#include "resourceManagement/vulkan/resources/VulkanResourceUtils.h"
#include "core/Profiling.h"
#include "core/Utils.h"
#include "nvutils/file_operations.hpp"
#include "nvvk/mipmaps.hpp"
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

constexpr VkBufferUsageFlags2 kUploadedModelBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
constexpr VkDeviceSize kGeometrySectionAlignment = 16;

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
VkDeviceSize vectorByteSize(const std::vector<T>& values)
{
    return values.size() * sizeof(T);
}

template <typename T>
RefPtr<Buffer> createAndAppendBuffer(const std::string& name, const std::vector<T>& values, bool& hasPendingUpload)
{
    if (values.empty())
    {
        return nullptr;
    }

    RefPtr<Buffer> buffer =
        RefPtr<Buffer>(new Buffer(name, kUploadedModelBufferUsage, vectorByteSize(values), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    PlayResourceManager::Instance().appendBuffer(*buffer, 0, std::span(values.data(), values.size()));
    hasPendingUpload = true;
    return buffer;
}

template <typename T>
void placeGeometrySection(const std::vector<T>& values, VkDeviceSize& cursor, VkDeviceSize& offset, VkDeviceSize& size)
{
    if (values.empty())
    {
        offset = 0;
        size   = 0;
        return;
    }

    cursor = alignUp(cursor, kGeometrySectionAlignment);
    offset = cursor;
    size   = vectorByteSize(values);
    cursor += size;
}

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
    std::vector<glm::vec3>      positions;
    std::vector<glm::vec3>      normals;
    std::vector<glm::vec4>      tangents;
    std::vector<glm::vec2>      texCoords0;
    std::vector<glm::vec2>      texCoords1;
    std::vector<uint32_t>       colors;
    std::vector<uint32_t>       indices;
    std::vector<ModelMeshRange> ranges;

    bool empty() const
    {
        return positions.empty() || indices.empty() || ranges.empty();
    }
};

struct ImportedModel
{
    ModelAssetPackage                      package;
    ModelGeometryPayload                    geometry;
    std::vector<ImageLoading::LoadedImage> textureImages;
};

struct OptimizedModel
{
    ModelAssetPackage                      package;
    ModelGeometryPayload                    geometry;
    std::vector<ImageLoading::LoadedImage> textureImages;
};

struct ModelImportResult
{
    bool          success = false;
    ImportedModel model;
    std::string   message;
};

struct ModelOptimizeResult
{
    bool           success = false;
    OptimizedModel model;
    std::string    message;
};

bool isValidLoadedImage(const ImageLoading::LoadedImage& loadedImage)
{
    return loadedImage.format != VK_FORMAT_UNDEFINED && loadedImage.extent.width > 0 && loadedImage.extent.height > 0 &&
           !loadedImage.pixels.empty();
}

uint32_t resolveTextureMipLevels(uint32_t requestedMipLevels, VkExtent2D extent)
{
    const uint32_t maxMipLevels = nvvk::mipLevels(extent);
    if (requestedMipLevels == 0 || requestedMipLevels > maxMipLevels)
    {
        return maxMipLevels;
    }
    return requestedMipLevels;
}

struct PendingTextureMipGeneration
{
    Texture*      texture     = nullptr;
    VkExtent2D    extent      = {};
    uint32_t      mipLevels   = 1;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};

class ModelUploadSession
{
public:
    explicit ModelUploadSession(OptimizedModel&& sourceModel) : _model(std::move(sourceModel)), _package(std::move(_model.package)) {}

    ModelAssetPackage upload();

private:
    RefPtr<Texture> createTextureFromLoadedImage(const ModelTextureResource& textureResource, const ImageLoading::LoadedImage& loadedImage);
    void            uploadTextures();
    void            uploadGeometry();
    void            submitPendingUploads();

    OptimizedModel                            _model;
    ModelAssetPackage                        _package;
    bool                                     _hasPendingUpload = false;
    std::vector<PendingTextureMipGeneration> _pendingMipGenerations;
};

RefPtr<Texture> ModelUploadSession::createTextureFromLoadedImage(const ModelTextureResource& textureResource,
                                                                 const ImageLoading::LoadedImage& loadedImage)
{
    if (!isValidLoadedImage(loadedImage))
    {
        return nullptr;
    }

    const uint32_t      mipLevels   = resolveTextureMipLevels(textureResource.mipLevels, loadedImage.extent);
    const VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = loadedImage.format,
        .extent        = {loadedImage.extent.width, loadedImage.extent.height, 1},
        .mipLevels     = mipLevels,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = loadedImage.format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {inferImageAspectFlags(loadedImage.format, true), 0, mipLevels, 0, 1},
    };

    nvvk::Image uploadedImage;
    PlayResourceManager& uploadManager = PlayResourceManager::Instance();
    if (uploadManager.createImage(uploadedImage, imageInfo, viewInfo) != VK_SUCCESS)
    {
        if (uploadedImage.image != VK_NULL_HANDLE)
        {
            uploadManager.destroyImage(uploadedImage);
        }
        return nullptr;
    }

    if (uploadManager.appendImage(uploadedImage, loadedImage.pixels.size(), loadedImage.pixels.data(), finalLayout) != VK_SUCCESS)
    {
        uploadManager.destroyImage(uploadedImage);
        return nullptr;
    }

    RefPtr<Texture> texture = RefPtr<Texture>(new Texture(textureResource.name,
                                                          uploadedImage.image,
                                                          uploadedImage.descriptor.imageView,
                                                          loadedImage.format,
                                                          uploadedImage.extent,
                                                          imageInfo.usage,
                                                          finalLayout,
                                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                                          mipLevels,
                                                          1,
                                                          VK_SAMPLE_COUNT_1_BIT,
                                                          true));
    texture->allocation = uploadedImage.allocation;
    _hasPendingUpload   = true;

    if (mipLevels > 1)
    {
        PendingTextureMipGeneration pendingMipGeneration;
        pendingMipGeneration.texture     = texture.get();
        pendingMipGeneration.extent      = loadedImage.extent;
        pendingMipGeneration.mipLevels   = mipLevels;
        pendingMipGeneration.finalLayout = finalLayout;
        _pendingMipGenerations.push_back(pendingMipGeneration);
    }

    uploadManager.acquireSampler(texture->descriptor.sampler);
    return texture;
}

void ModelUploadSession::uploadTextures()
{
    PLAY_PROFILE_SCOPE("ModelLoading::uploadModelTextures");

    for (uint32_t textureIndex = 0; textureIndex < _package.textures.size(); ++textureIndex)
    {
        ModelTextureResource& texture = _package.textures[textureIndex];
        if (texture.texture)
        {
            continue;
        }

        if (textureIndex < _model.textureImages.size())
        {
            texture.texture = createTextureFromLoadedImage(texture, _model.textureImages[textureIndex]);
        }
    }
}

void compactResidentTextures(ModelAssetPackage& package)
{
    PLAY_PROFILE_SCOPE("ModelLoading::compactResidentTextures");

    std::vector<uint32_t> residentTextureIndexByOriginalIndex(package.textures.size(), INVALID_SCENE_ID);
    std::vector<ModelTextureResource> residentTextures;
    residentTextures.reserve(package.textures.size());

    for (uint32_t textureIndex = 0; textureIndex < package.textures.size(); ++textureIndex)
    {
        ModelTextureResource& texture = package.textures[textureIndex];
        if (!texture.isResident())
        {
            continue;
        }

        residentTextureIndexByOriginalIndex[textureIndex] = static_cast<uint32_t>(residentTextures.size());
        residentTextures.push_back(std::move(texture));
    }

    for (uint32_t textureInfoIndex = 1; textureInfoIndex < package.textureInfos.size(); ++textureInfoIndex)
    {
        shaderio::GltfTextureInfo& textureInfo = package.textureInfos[textureInfoIndex];
        if (textureInfo.index < 0 || static_cast<uint32_t>(textureInfo.index) >= residentTextureIndexByOriginalIndex.size())
        {
            textureInfo.index = -1;
            continue;
        }

        // Keep texture indices local to this package. GBuffer adds GpuModelRange::firstTexture when sampling the scene texture array.
        const uint32_t compactLocalTextureIndex = residentTextureIndexByOriginalIndex[textureInfo.index];
        textureInfo.index = compactLocalTextureIndex == INVALID_SCENE_ID ? -1 : static_cast<int>(compactLocalTextureIndex);
    }

    package.textures = std::move(residentTextures);
}

void ModelUploadSession::uploadGeometry()
{
    PLAY_PROFILE_SCOPE("ModelLoading::uploadModelGeometry");

    ModelAssetPackage&    package  = _package;
    ModelGeometryPayload& geometry = _model.geometry;
    if (geometry.empty() || package.meshInfos.empty())
    {
        return;
    }

    VkDeviceSize positionsOffset  = 0;
    VkDeviceSize normalsOffset    = 0;
    VkDeviceSize tangentsOffset   = 0;
    VkDeviceSize texCoords0Offset = 0;
    VkDeviceSize texCoords1Offset = 0;
    VkDeviceSize colorsOffset     = 0;
    VkDeviceSize indicesOffset    = 0;

    VkDeviceSize positionsSize  = 0;
    VkDeviceSize normalsSize    = 0;
    VkDeviceSize tangentsSize   = 0;
    VkDeviceSize texCoords0Size = 0;
    VkDeviceSize texCoords1Size = 0;
    VkDeviceSize colorsSize     = 0;
    VkDeviceSize indicesSize    = 0;

    VkDeviceSize cursor = 0;
    {
        PLAY_PROFILE_SCOPE("ModelLoading::layout geometry sections");
        placeGeometrySection(geometry.positions, cursor, positionsOffset, positionsSize);
        placeGeometrySection(geometry.normals, cursor, normalsOffset, normalsSize);
        placeGeometrySection(geometry.tangents, cursor, tangentsOffset, tangentsSize);
        placeGeometrySection(geometry.texCoords0, cursor, texCoords0Offset, texCoords0Size);
        placeGeometrySection(geometry.texCoords1, cursor, texCoords1Offset, texCoords1Size);
        placeGeometrySection(geometry.colors, cursor, colorsOffset, colorsSize);
        placeGeometrySection(geometry.indices, cursor, indicesOffset, indicesSize);
    }

    if (cursor == 0)
    {
        return;
    }

    RefPtr<Buffer> geometryBuffer;
    {
        PLAY_PROFILE_SCOPE("ModelLoading::create geometry buffer");
        geometryBuffer = RefPtr<Buffer>(
            new Buffer(package.asset.name + "_GeometryBuffer", kUploadedModelBufferUsage, cursor, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    }

    PlayResourceManager& uploadManager = PlayResourceManager::Instance();
    {
        PLAY_PROFILE_SCOPE("ModelLoading::append geometry buffer sections");
        if (positionsSize > 0)
            uploadManager.appendBuffer(*geometryBuffer, positionsOffset, std::span(geometry.positions.data(), geometry.positions.size()));
        if (normalsSize > 0) uploadManager.appendBuffer(*geometryBuffer, normalsOffset, std::span(geometry.normals.data(), geometry.normals.size()));
        if (tangentsSize > 0)
            uploadManager.appendBuffer(*geometryBuffer, tangentsOffset, std::span(geometry.tangents.data(), geometry.tangents.size()));
        if (texCoords0Size > 0)
            uploadManager.appendBuffer(*geometryBuffer, texCoords0Offset, std::span(geometry.texCoords0.data(), geometry.texCoords0.size()));
        if (texCoords1Size > 0)
            uploadManager.appendBuffer(*geometryBuffer, texCoords1Offset, std::span(geometry.texCoords1.data(), geometry.texCoords1.size()));
        if (colorsSize > 0) uploadManager.appendBuffer(*geometryBuffer, colorsOffset, std::span(geometry.colors.data(), geometry.colors.size()));
        if (indicesSize > 0) uploadManager.appendBuffer(*geometryBuffer, indicesOffset, std::span(geometry.indices.data(), geometry.indices.size()));
    }

    std::vector<VertexStreamInfo> vertexStreams;
    vertexStreams.resize(geometry.ranges.size());
    {
        PLAY_PROFILE_SCOPE("ModelLoading::build vertex stream infos");
        for (uint32_t meshIndex = 0; meshIndex < geometry.ranges.size() && meshIndex < package.meshInfos.size(); ++meshIndex)
        {
            const ModelMeshRange& range = geometry.ranges[meshIndex];

            VertexStreamInfo stream;
            stream.positionBufferAddress  = geometryBuffer->address + positionsOffset + range.firstVertex * sizeof(glm::vec3);
            stream.normalBufferAddress    = geometryBuffer->address + normalsOffset + range.firstVertex * sizeof(glm::vec3);
            stream.tangentBufferAddress   = geometryBuffer->address + tangentsOffset + range.firstVertex * sizeof(glm::vec4);
            stream.texCoord0BufferAddress = geometryBuffer->address + texCoords0Offset + range.firstVertex * sizeof(glm::vec2);
            stream.texCoord1BufferAddress = geometryBuffer->address + texCoords1Offset + range.firstVertex * sizeof(glm::vec2);
            stream.colorBufferAddress     = geometryBuffer->address + colorsOffset + range.firstVertex * sizeof(uint32_t);
            vertexStreams[meshIndex]      = stream;

            package.meshInfos[meshIndex].IndexBufferAddress = geometryBuffer->address + indicesOffset + range.firstIndex * sizeof(uint32_t);
            package.meshInfos[meshIndex].indexCount         = range.indexCount;
        }
    }

    RefPtr<Buffer> vertexStreamBuffer =
        createAndAppendBuffer(package.asset.name + "_VertexStreamBuffer", vertexStreams, _hasPendingUpload);
    if (vertexStreamBuffer)
    {
        for (uint32_t meshIndex = 0; meshIndex < vertexStreams.size() && meshIndex < package.meshInfos.size(); ++meshIndex)
        {
            package.meshInfos[meshIndex].vertexBufferAddress = vertexStreamBuffer->address + meshIndex * sizeof(VertexStreamInfo);
        }
    }

    package.ownedBuffers.push_back(geometryBuffer);
    if (vertexStreamBuffer)
    {
        package.ownedBuffers.push_back(vertexStreamBuffer);
    }
    _hasPendingUpload = true;
}

void ModelUploadSession::submitPendingUploads()
{
    PLAY_PROFILE_SCOPE("ModelLoading::submitPendingUploads");

    if (!_hasPendingUpload && _pendingMipGenerations.empty())
    {
        return;
    }

    PlayResourceManager& uploadManager = PlayResourceManager::Instance();
    VkCommandBuffer      cmd           = VK_NULL_HANDLE;
    {
        PLAY_PROFILE_SCOPE("ModelLoading::get temp upload command buffer");
        cmd = uploadManager.getTempCommandBuffer();
    }
    {
        PLAY_PROFILE_SCOPE("ModelLoading::record pending uploads");
        PLAY_PROFILE_COMMAND_LABEL(cmd, "Model Pending Uploads");
        if (_hasPendingUpload)
        {
            uploadManager.cmdUploadAppended(cmd);
        }
    }
    {
        PLAY_PROFILE_SCOPE("ModelLoading::record texture mipmaps");
        PLAY_PROFILE_COMMAND_LABEL(cmd, "Model Texture Generate Mipmaps");
        for (const PendingTextureMipGeneration& pendingMipGeneration : _pendingMipGenerations)
        {
            if (!pendingMipGeneration.texture)
            {
                continue;
            }

            nvvk::cmdGenerateMipmaps(cmd,
                                     pendingMipGeneration.texture->image,
                                     pendingMipGeneration.extent,
                                     pendingMipGeneration.mipLevels,
                                     1,
                                     pendingMipGeneration.finalLayout);
        }
    }
    {
        PLAY_PROFILE_SCOPE("ModelLoading::submit and wait pending uploads");
        uploadManager.submitAndWaitTempCmdBuffer(cmd);
    }
}

ModelAssetPackage ModelUploadSession::upload()
{
    PLAY_PROFILE_SCOPE("ModelLoading::uploadModelPackage");

    uploadTextures();
    compactResidentTextures(_package);

    uploadGeometry();
    {
        PLAY_PROFILE_SCOPE("ModelLoading::append model metadata buffers");
        _package.asset.transformBuffer =
            createAndAppendBuffer(_package.asset.name + "_TransformBuffer", _package.asset.transforms, _hasPendingUpload);
        _package.asset.materialBuffer = createAndAppendBuffer(_package.asset.name + "_MaterialBuffer", _package.materials, _hasPendingUpload);
        _package.asset.textureInfoBuffer =
            createAndAppendBuffer(_package.asset.name + "_TextureInfoBuffer", _package.textureInfos, _hasPendingUpload);
        _package.asset.meshInfoBuffer = createAndAppendBuffer(_package.asset.name + "_MeshInfoBuffer", _package.meshInfos, _hasPendingUpload);
    }
    submitPendingUploads();

    return std::move(_package);
}

ModelAssetPackage uploadModelPackage(OptimizedModel&& model)
{
    ModelUploadSession uploadSession(std::move(model));
    return uploadSession.upload();
}


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

uint32_t appendMeshGeometry(const aiMesh* mesh, ModelAssetPackage& package, ModelGeometryPayload& geometry, uint32_t materialIndex)
{
    if (!mesh)
    {
        return INVALID_SCENE_ID;
    }

    ModelMeshRange range;
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

    MeshInfo meshInfo;
    meshInfo.vertexBufferAddress = 0;
    meshInfo.IndexBufferAddress  = 0;
    meshInfo.indexCount          = range.indexCount;
    meshInfo.materialIdx         = range.materialIdx;

    const uint32_t meshID = static_cast<uint32_t>(package.meshInfos.size());
    geometry.ranges.push_back(range);
    package.meshInfos.push_back(meshInfo);
    return meshID;
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

class MaterialImportSession
{
public:
    MaterialImportSession(ImportedModel& model, const std::filesystem::path& modelPath, const aiScene* assimpScene,
                          const ModelLoadingConfig& loadingCfg)
        : _model(model), _modelPath(modelPath), _assimpScene(assimpScene), _loadingCfg(loadingCfg)
    {
    }

    shaderio::GltfShadeMaterial importMaterial(const aiMaterial* material);
    void                        loadTextureImages();

private:
    int  ensureLocalTextureIndex(const aiString& texturePath, bool isSrgb);
    void readTextureSlot(const aiMaterial* material, aiTextureType textureType, ImportedTextureSlot& slot, bool isSrgb);

    ImportedModel&                 _model;
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

    ModelAssetPackage& package = _model.package;
    const int existingLocalIndex = findLocalTextureIndex(package, sourcePath, name, embedded);
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

    ModelTextureResource texture;
    texture.name       = name;
    texture.sourcePath = sourcePath;
    texture.mipLevels  = _loadingCfg.textureMipLevels;
    texture.isSrgb     = isSrgb;

    const int localIndex = static_cast<int>(package.textures.size());
    package.textures.push_back(std::move(texture));
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

uint16_t appendTextureInfo(ModelAssetPackage& package, const ImportedTextureSlot& slot)
{
    if (!slot.hasTexture() || package.textureInfos.size() >= 0xFFFF)
    {
        return 0;
    }

    shaderio::GltfTextureInfo textureInfo = shaderio::defaultGltfTextureInfo();
    textureInfo.index                     = slot.localTextureIndex;
    textureInfo.texCoord                  = slot.texCoord;
    textureInfo.uvTransform = shaderio::float3x2(slot.uvTransform[0][0], slot.uvTransform[1][0], slot.uvTransform[0][1], slot.uvTransform[1][1],
                                                 slot.uvTransform[0][2], slot.uvTransform[1][2]);

    const uint16_t textureInfoIndex = static_cast<uint16_t>(package.textureInfos.size());
    package.textureInfos.push_back(textureInfo);
    return textureInfoIndex;
}

shaderio::GltfShadeMaterial MaterialImportSession::importMaterial(const aiMaterial* material)
{
    ModelAssetPackage& package = _model.package;
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
    importedMaterial.pbrBaseColorTexture = appendTextureInfo(package, baseColorSlot);

    ImportedTextureSlot normalSlot;
    readTextureSlot(material, aiTextureType_NORMALS, normalSlot, false);
    if (!normalSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_NORMAL_CAMERA, normalSlot, false);
    }
    importedMaterial.normalTexture = appendTextureInfo(package, normalSlot);

    ImportedTextureSlot metallicRoughnessSlot;
    readTextureSlot(material, aiTextureType_GLTF_METALLIC_ROUGHNESS, metallicRoughnessSlot, false);
    if (!metallicRoughnessSlot.hasTexture())
    {
        readTextureSlot(material, aiTextureType_DIFFUSE_ROUGHNESS, metallicRoughnessSlot, false);
    }
    importedMaterial.pbrMetallicRoughnessTexture = appendTextureInfo(package, metallicRoughnessSlot);

    ImportedTextureSlot emissiveSlot;
    readTextureSlot(material, aiTextureType_EMISSIVE, emissiveSlot, _loadingCfg.srgbEmissiveTextures);
    importedMaterial.emissiveTexture = appendTextureInfo(package, emissiveSlot);

    ImportedTextureSlot occlusionSlot;
    readTextureSlot(material, aiTextureType_AMBIENT_OCCLUSION, occlusionSlot, false);
    importedMaterial.occlusionTexture = appendTextureInfo(package, occlusionSlot);

    ImportedTextureSlot specularSlot;
    readTextureSlot(material, aiTextureType_SPECULAR, specularSlot, false);
    importedMaterial.specularTexture = appendTextureInfo(package, specularSlot);

    return importedMaterial;
}

void MaterialImportSession::loadTextureImages()
{
    PLAY_PROFILE_SCOPE("ModelLoading::loadImportedTextureImages");

    ModelAssetPackage& package = _model.package;
    _model.textureImages.clear();
    _model.textureImages.resize(package.textures.size());

    for (uint32_t textureIndex = 0; textureIndex < package.textures.size(); ++textureIndex)
    {
        const ModelTextureResource& texture = package.textures[textureIndex];
        if (texture.sourcePath.empty())
        {
            continue;
        }

        ImageLoading::LoadTextureImage(texture.sourcePath, texture.isSrgb, _model.textureImages[textureIndex]);
    }
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

        ModelAssetPackage&   package  = result.model.package;
        ModelGeometryPayload& geometry = result.model.geometry;
        {
            PLAY_PROFILE_SCOPE("ModelLoading::initialize model package");
            package.asset.name       = path.stem().string();
            package.asset.sourcePath = path;
            package.textureInfos.push_back(shaderio::defaultGltfTextureInfo());
        }

        MaterialImportSession materialImporter(result.model, path, assimpScene, loadingCfg);
        {
            PLAY_PROFILE_SCOPE("ModelLoading::import materials");
            if (loadingCfg.loadMaterials && assimpScene->mNumMaterials > 0)
            {
                package.materials.reserve(assimpScene->mNumMaterials);
                for (uint32_t materialIndex = 0; materialIndex < assimpScene->mNumMaterials; ++materialIndex)
                {
                    package.materials.push_back(materialImporter.importMaterial(assimpScene->mMaterials[materialIndex]));
                }
            }
        }

        if (package.materials.empty())
        {
            package.materials.push_back(shaderio::defaultGltfMaterial());
        }

        materialImporter.loadTextureImages();

        AssimpImportContext context;
        context.package = &package;
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
                if (materialIndex >= package.materials.size())
                {
                    materialIndex = 0;
                }

                const uint32_t meshID = appendMeshGeometry(mesh, package, geometry, materialIndex);
                if (meshID == INVALID_SCENE_ID || meshID >= geometry.ranges.size())
                {
                    context.meshSubmeshIndices.push_back(INVALID_SCENE_ID);
                    continue;
                }

                ModelSubmeshAsset submesh;
                submesh.meshID = meshID;
                submesh.bbox   = geometry.ranges[meshID].bbox;

                const uint32_t submeshIndex = static_cast<uint32_t>(package.asset.submeshes.size());
                package.asset.submeshes.push_back(submesh);
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

ModelOptimizeResult optimizeModel(ImportedModel&& importedModel, const ModelLoadingConfig& loadingConfig)
{
    PLAY_PROFILE_SCOPE("ModelLoading::optimizeModel");

    (void) loadingConfig;

    ModelOptimizeResult result;
    result.success             = true;
    result.model.package       = std::move(importedModel.package);
    result.model.geometry      = std::move(importedModel.geometry);
    result.model.textureImages = std::move(importedModel.textureImages);
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

    ModelOptimizeResult optimizeResult = [&]()
    {
        PLAY_PROFILE_SCOPE("model_loading::optimize");
        return optimizeModel(std::move(importResult.model), loadingConfig);
    }();
    if (!optimizeResult.success)
    {
        ModelLoadResult result;
        result.message = optimizeResult.message;
        return result;
    }

    ModelLoadResult result;
    result.success = true;
    {
        PLAY_PROFILE_SCOPE("model_loading::upload");
        result.model = uploadModelPackage(std::move(optimizeResult.model));
    }
    return result;
}

} // namespace Play
