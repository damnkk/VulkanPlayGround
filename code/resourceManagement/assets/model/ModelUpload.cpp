#include "ModelUpload.h"

#include "core/Profiling.h"

namespace Play
{

namespace
{

constexpr VkBufferUsageFlags2 kUploadedModelBufferUsage =
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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

template <typename T>
bool uploadBuffer(AssetGpuUploadContext& context, const std::string& name, const std::vector<T>& values, RefPtr<Buffer>& buffer,
                  std::string& message)
{
    if (values.empty())
    {
        return true;
    }

    buffer = context.createDeviceBuffer(name, kUploadedModelBufferUsage, vectorByteSize(values));
    if (!buffer)
    {
        message = "Could not create model GPU buffer: " + name;
        return false;
    }

    if (context.uploadBuffer(*buffer, 0, vectorByteSize(values), values.data()) != VK_SUCCESS)
    {
        message = "Could not stage model GPU buffer: " + name;
        return false;
    }
    return true;
}

bool isRangeValid(uint32_t first, uint32_t count, size_t size)
{
    return first <= size && count <= size - first;
}

uint16_t convertTextureInfoIndex(uint32_t sourceIndex)
{
    return sourceIndex == vpgloader::InvalidModelIndex ? 0 : static_cast<uint16_t>(sourceIndex + 1);
}

shaderio::GltfShadeMaterial convertMaterial(const vpgloader::ModelMaterial& source)
{
    shaderio::GltfShadeMaterial material = shaderio::defaultGltfMaterial();
    material.pbrBaseColorFactor          = source.baseColorFactor;
    material.emissiveFactor              = source.emissiveFactor;
    material.normalTextureScale          = source.normalScale;
    material.pbrRoughnessFactor          = source.roughnessFactor;
    material.pbrMetallicFactor           = source.metallicFactor;
    material.alphaCutoff                 = source.alphaCutoff;
    material.occlusionStrength           = source.occlusionStrength;
    material.doubleSided                 = source.doubleSided ? 1 : 0;
    material.unlit                       = source.unlit ? 1 : 0;
    material.pbrBaseColorTexture         = convertTextureInfoIndex(source.baseColorTexture);
    material.normalTexture               = convertTextureInfoIndex(source.normalTexture);
    material.pbrMetallicRoughnessTexture = convertTextureInfoIndex(source.metallicRoughnessTexture);
    material.emissiveTexture             = convertTextureInfoIndex(source.emissiveTexture);
    material.occlusionTexture            = convertTextureInfoIndex(source.occlusionTexture);
    material.specularTexture             = convertTextureInfoIndex(source.specularTexture);

    switch (source.alphaMode)
    {
        case vpgloader::AlphaMode::Mask:
            material.alphaMode = shaderio::eAlphaModeMask;
            break;
        case vpgloader::AlphaMode::Blend:
            material.alphaMode = shaderio::eAlphaModeBlend;
            break;
        case vpgloader::AlphaMode::Opaque:
        default:
            material.alphaMode = shaderio::eAlphaModeOpaque;
            break;
    }
    return material;
}

shaderio::GltfTextureInfo convertTextureInfo(const vpgloader::ModelTextureInfo& source)
{
    shaderio::GltfTextureInfo textureInfo = shaderio::defaultGltfTextureInfo();
    const float               cosine      = glm::cos(source.rotation);
    const float               sine        = glm::sin(source.rotation);
    textureInfo.index                     = source.textureIndex == vpgloader::InvalidModelIndex ? -1 : static_cast<int>(source.textureIndex);
    textureInfo.texCoord                  = static_cast<int>(source.texCoord);
    textureInfo.uvTransform = shaderio::float3x2(source.scale.x * cosine, -source.scale.y * sine, source.scale.x * sine, source.scale.y * cosine,
                                                 source.offset.x, source.offset.y);
    return textureInfo;
}

} // namespace

ModelUploadJob::ModelUploadJob(ModelLoadRequestID requestID, vpgloader::ModelHandle source)
    : _requestID(requestID), _source(std::move(source))
{
}

bool ModelUploadJob::build(AssetGpuUploadContext& context, std::string& message)
{
    PLAY_PROFILE_SCOPE("ModelUploadJob::build");

    if (!_source)
    {
        message = "Model GPU upload has no CPU source data.";
        return false;
    }
    if (_source->textureInfos.size() >= 0xFFFF)
    {
        message = "Model has too many texture-info entries for the renderer.";
        return false;
    }

    _uploadedModel             = {};
    _uploadedModel.model       = _source;
    _uploadedModel.materials.reserve(_source->materials.size());
    for (const vpgloader::ModelMaterial& material : _source->materials)
    {
        _uploadedModel.materials.push_back(convertMaterial(material));
    }

    _uploadedModel.textureInfos.reserve(_source->textureInfos.size() + 1);
    _uploadedModel.textureInfos.push_back(shaderio::defaultGltfTextureInfo());
    for (const vpgloader::ModelTextureInfo& textureInfo : _source->textureInfos)
    {
        _uploadedModel.textureInfos.push_back(convertTextureInfo(textureInfo));
    }

    return uploadTextures(context, message) && uploadGeometry(context, message) && uploadMetadata(context, message);
}

UploadedModel ModelUploadJob::takeUploadedModel()
{
    return std::move(_uploadedModel);
}

bool ModelUploadJob::uploadTextures(AssetGpuUploadContext& context, std::string& message)
{
    PLAY_PROFILE_SCOPE("ModelUploadJob::uploadTextures");

    std::vector<uint32_t> residentTextureIndexByOriginalIndex(_source->textures.size(), INVALID_SCENE_ID);
    _uploadedModel.textures.reserve(_source->textures.size());

    for (uint32_t textureIndex = 0; textureIndex < _source->textures.size(); ++textureIndex)
    {
        const vpgloader::ModelTextureAsset& sourceTexture = _source->textures[textureIndex];
        if (!sourceTexture.texture)
        {
            continue;
        }

        const std::string textureName = sourceTexture.name.empty()
                                            ? _source->asset.name + "_Texture_" + std::to_string(textureIndex)
                                            : sourceTexture.name;
        RefPtr<Texture> texture = context.createTexture2D(textureName, *sourceTexture.texture, sourceTexture.isSrgb, 0);
        if (!texture)
        {
            message = "Could not create model texture: " + textureName;
            return false;
        }

        if (context.uploadImage(*texture, *sourceTexture.texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != VK_SUCCESS)
        {
            message = "Could not stage model texture: " + textureName;
            return false;
        }
        context.generateMipmaps(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        UploadedModelTexture uploadedTexture;
        uploadedTexture.name       = sourceTexture.name;
        uploadedTexture.sourcePath = sourceTexture.sourcePath;
        uploadedTexture.texture    = texture;
        uploadedTexture.mipLevels  = texture->MipLevel();
        uploadedTexture.isSrgb     = sourceTexture.isSrgb;

        residentTextureIndexByOriginalIndex[textureIndex] = static_cast<uint32_t>(_uploadedModel.textures.size());
        _uploadedModel.textures.push_back(std::move(uploadedTexture));
    }

    for (uint32_t textureInfoIndex = 1; textureInfoIndex < _uploadedModel.textureInfos.size(); ++textureInfoIndex)
    {
        shaderio::GltfTextureInfo& textureInfo = _uploadedModel.textureInfos[textureInfoIndex];
        if (textureInfo.index < 0 || static_cast<uint32_t>(textureInfo.index) >= residentTextureIndexByOriginalIndex.size())
        {
            textureInfo.index = -1;
            continue;
        }

        const uint32_t compactLocalTextureIndex = residentTextureIndexByOriginalIndex[textureInfo.index];
        textureInfo.index = compactLocalTextureIndex == INVALID_SCENE_ID ? -1 : static_cast<int>(compactLocalTextureIndex);
    }

    return true;
}

bool ModelUploadJob::uploadGeometry(AssetGpuUploadContext& context, std::string& message)
{
    PLAY_PROFILE_SCOPE("ModelUploadJob::uploadGeometry");

    const vpgloader::ModelGeometryData& geometry = _source->geometry;
    if (_source->meshes.empty())
    {
        return true;
    }

    if (geometry.positions.empty() || geometry.indices.empty())
    {
        message = "Model GPU upload received meshes without geometry data.";
        return false;
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
    placeGeometrySection(geometry.positions, cursor, positionsOffset, positionsSize);
    placeGeometrySection(geometry.normals, cursor, normalsOffset, normalsSize);
    placeGeometrySection(geometry.tangents, cursor, tangentsOffset, tangentsSize);
    placeGeometrySection(geometry.texCoords0, cursor, texCoords0Offset, texCoords0Size);
    placeGeometrySection(geometry.texCoords1, cursor, texCoords1Offset, texCoords1Size);
    placeGeometrySection(geometry.colors, cursor, colorsOffset, colorsSize);
    placeGeometrySection(geometry.indices, cursor, indicesOffset, indicesSize);

    RefPtr<Buffer> geometryBuffer = context.createDeviceBuffer(_source->asset.name + "_GeometryBuffer", kUploadedModelBufferUsage, cursor);
    if (!geometryBuffer)
    {
        message = "Could not create model geometry buffer.";
        return false;
    }

    if ((positionsSize > 0 && context.uploadBuffer(*geometryBuffer, positionsOffset, positionsSize, geometry.positions.data()) != VK_SUCCESS) ||
        (normalsSize > 0 && context.uploadBuffer(*geometryBuffer, normalsOffset, normalsSize, geometry.normals.data()) != VK_SUCCESS) ||
        (tangentsSize > 0 && context.uploadBuffer(*geometryBuffer, tangentsOffset, tangentsSize, geometry.tangents.data()) != VK_SUCCESS) ||
        (texCoords0Size > 0 && context.uploadBuffer(*geometryBuffer, texCoords0Offset, texCoords0Size, geometry.texCoords0.data()) != VK_SUCCESS) ||
        (texCoords1Size > 0 && context.uploadBuffer(*geometryBuffer, texCoords1Offset, texCoords1Size, geometry.texCoords1.data()) != VK_SUCCESS) ||
        (colorsSize > 0 && context.uploadBuffer(*geometryBuffer, colorsOffset, colorsSize, geometry.colors.data()) != VK_SUCCESS) ||
        (indicesSize > 0 && context.uploadBuffer(*geometryBuffer, indicesOffset, indicesSize, geometry.indices.data()) != VK_SUCCESS))
    {
        message = "Could not stage model geometry buffer.";
        return false;
    }

    _uploadedModel.meshInfos.resize(_source->meshes.size());
    std::vector<VertexStreamInfo> vertexStreams(_source->meshes.size());
    for (uint32_t meshIndex = 0; meshIndex < _source->meshes.size(); ++meshIndex)
    {
        const vpgloader::ModelMeshAsset& mesh = _source->meshes[meshIndex];
        if (!isRangeValid(mesh.firstVertex, mesh.vertexCount, geometry.positions.size()) ||
            !isRangeValid(mesh.firstIndex, mesh.indexCount, geometry.indices.size()))
        {
            message = "Model mesh range is outside its CPU geometry data.";
            return false;
        }

        VertexStreamInfo& stream = vertexStreams[meshIndex];
        stream.positionBufferAddress  = geometryBuffer->address + positionsOffset + mesh.firstVertex * sizeof(glm::vec3);
        stream.normalBufferAddress    = geometryBuffer->address + normalsOffset + mesh.firstVertex * sizeof(glm::vec3);
        stream.tangentBufferAddress   = geometryBuffer->address + tangentsOffset + mesh.firstVertex * sizeof(glm::vec4);
        stream.texCoord0BufferAddress = geometryBuffer->address + texCoords0Offset + mesh.firstVertex * sizeof(glm::vec2);
        stream.texCoord1BufferAddress = geometryBuffer->address + texCoords1Offset + mesh.firstVertex * sizeof(glm::vec2);
        stream.colorBufferAddress     = geometryBuffer->address + colorsOffset + mesh.firstVertex * sizeof(uint32_t);

        MeshInfo& meshInfo             = _uploadedModel.meshInfos[meshIndex];
        meshInfo.IndexBufferAddress    = geometryBuffer->address + indicesOffset + mesh.firstIndex * sizeof(uint32_t);
        meshInfo.indexCount            = mesh.indexCount;
        meshInfo.materialIdx           = mesh.materialIndex;
    }

    RefPtr<Buffer> vertexStreamBuffer;
    if (!uploadBuffer(context, _source->asset.name + "_VertexStreamBuffer", vertexStreams, vertexStreamBuffer, message))
    {
        return false;
    }

    for (uint32_t meshIndex = 0; meshIndex < _uploadedModel.meshInfos.size(); ++meshIndex)
    {
        _uploadedModel.meshInfos[meshIndex].vertexBufferAddress = vertexStreamBuffer->address + meshIndex * sizeof(VertexStreamInfo);
    }

    _uploadedModel.resources.ownedBuffers.push_back(geometryBuffer);
    _uploadedModel.resources.ownedBuffers.push_back(vertexStreamBuffer);
    return true;
}

bool ModelUploadJob::uploadMetadata(AssetGpuUploadContext& context, std::string& message)
{
    PLAY_PROFILE_SCOPE("ModelUploadJob::uploadMetadata");

    return uploadBuffer(context, _source->asset.name + "_TransformBuffer", _source->asset.transforms,
                      _uploadedModel.resources.transformBuffer, message) &&
           uploadBuffer(context, _source->asset.name + "_MaterialBuffer", _uploadedModel.materials,
                      _uploadedModel.resources.materialBuffer, message) &&
           uploadBuffer(context, _source->asset.name + "_TextureInfoBuffer", _uploadedModel.textureInfos,
                      _uploadedModel.resources.textureInfoBuffer, message) &&
           uploadBuffer(context, _source->asset.name + "_MeshInfoBuffer", _uploadedModel.meshInfos,
                      _uploadedModel.resources.meshInfoBuffer, message);
}

} // namespace Play
