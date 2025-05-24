#include "ModelLoader.h"
#include "nvh/nvprint.hpp"
#include "nvh/gltfscene.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "iostream"
#include "PlayApp.h"
#include "nvvk/images_vk.hpp"
namespace Play
{
void ModelLoader::init(PlayApp* app)
{
    _app = app;
    m_debug.setup(app->m_device);
}

void ModelLoader::loadModel(std::string path)
{
    // gltf load implementation
    auto     scene          = this->_app->getScene();
    uint32_t textureOffset  = _sceneTextures.size();
    uint32_t vboOffset      = _sceneVBuffers.size();
    uint32_t iboOffset      = _sceneIBuffers.size();
    uint32_t meshOffset     = _sceneMeshes.size();
    uint32_t materialOffset = _sceneMaterials.size();

    nvh::GltfScene     gltfScene;
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;
    if (path.ends_with(".gltf"))
    {
        loader.LoadASCIIFromFile(&model, &err, &warn, path);
        if (!err.empty())
        {
            LOGE_FILELINE("Failed to load model: %s\n", path.c_str());
            return;
        }
        else
        {
            LOGI("Model loaded successfully: %s\n", path.c_str());
        }
    }
    else if (path.ends_with(".glb"))
    {
        loader.LoadBinaryFromFile(&model, &err, &warn, path);
        if (!err.empty())
        {
            LOGE_FILELINE("Failed to load model: %s\n", path.c_str());
            return;
        }
        else
        {
            LOGI("Model loaded successfully: %s\n", path.c_str());
        }
    }
    else
    {
        LOGE_FILELINE("Unsupported model format: %s\n", path.c_str());
        return;
    }
    gltfScene.importDrawableNodes(model,
                                  nvh::GltfAttributes::Normal | nvh::GltfAttributes::Texcoord_0 |
                                      nvh::GltfAttributes::Tangent | nvh::GltfAttributes::Color_0,
                                  nvh::GltfAttributes::Position);
    gltfScene.importMaterials(model);

    std::vector<uint32_t> lightMateiralIdx;
    int                   materialCnt = 0;
    for (const auto& material : gltfScene.m_materials)
    {
        Material mat;
        mat.pbrBaseColorFactor = material.baseColorFactor;
        mat.pbrBaseColorTexture =
            material.baseColorTexture == -1 ? -1 : textureOffset + material.baseColorTexture;
        mat.pbrMetallicFactor           = material.metallicFactor;
        mat.pbrRoughnessFactor          = material.roughnessFactor;
        mat.pbrMetallicRoughnessTexture = material.metallicRoughnessTexture == -1
                                              ? -1
                                              : textureOffset + material.metallicRoughnessTexture;
        mat.emissiveTexture =
            material.emissiveTexture == -1 ? -1 : textureOffset + material.emissiveTexture;
        mat.emissiveFactor = material.emissiveFactor;
        mat.alphaMode      = material.alphaMode;
        mat.alphaCutoff    = material.alphaCutoff;
        mat.doubleSided    = material.doubleSided;
        mat.normalTexture =
            material.normalTexture == -1 ? -1 : textureOffset + material.normalTexture;
        mat.normalTextureScale  = material.normalTextureScale;
        mat.uvTransform         = glm::mat4(material.textureTransform.uvTransform);
        mat.unlit               = material.unlit.active;
        mat.transmissionFactor  = material.transmission.factor;
        mat.transmissionTexture = material.transmission.texture.index == -1
                                      ? -1
                                      : textureOffset + material.transmission.texture.index;
        mat.anisotropy          = material.anisotropy.anisotropyStrength;
        mat.anisotropyDirection = glm::vec3(sin(material.anisotropy.anisotropyRotation),
                                            cos(material.anisotropy.anisotropyRotation), 0.f);
        mat.ior                 = material.ior.ior;
        mat.attenuationColor    = material.volume.attenuationColor;
        mat.thicknessFactor     = material.volume.thicknessFactor;
        mat.thicknessTexture    = material.volume.thicknessTexture.index == -1
                                      ? -1
                                      : textureOffset + material.volume.thicknessTexture.index;
        mat.attenuationDistance = material.volume.attenuationDistance;
        mat.clearcoatFactor     = material.clearcoat.factor;
        mat.clearcoatRoughness  = material.clearcoat.roughnessFactor;
        mat.clearcoatTexture    = material.clearcoat.texture.index == -1
                                      ? -1
                                      : textureOffset + material.clearcoat.texture.index;
        mat.clearcoatRoughnessTexture =
            material.clearcoat.roughnessTexture.index == -1
                ? -1
                : textureOffset + material.clearcoat.roughnessTexture.index;
        mat.sheen = glm::packUnorm4x8(
            vec4(material.sheen.sheenColorFactor, material.sheen.sheenRoughnessFactor));

        _sceneMaterials.push_back(mat);
        if (material.emissiveFactor != glm::vec3(0.0))
        {
            lightMateiralIdx.push_back(materialOffset + materialCnt);
        }
        ++materialCnt;
    }
    auto modelRootNode =
        this->_app->getScene()._root->addChild(path.substr(path.find_last_of('/') + 1));
    for (auto& node : gltfScene.m_nodes)
    {
        auto newNode = modelRootNode->addChild(node);
        newNode->_meshIdx.push_back(node.primMesh + meshOffset);
    }
    std::unordered_map<std::string, Buffer*> m_cacheBuffers;
    for (const auto& primMesh : gltfScene.m_primMeshes)
    {
        std::stringstream o;
        o << primMesh.vertexOffset << "::" << primMesh.vertexCount;
        std::string key  = o.str();
        auto        find = m_cacheBuffers.find(key);
        Buffer*     vBufferref;
        Buffer*     iBufferref;
        if (find == m_cacheBuffers.end())
        {
            std::vector<Vertex> vertices;
            vertices.reserve(primMesh.vertexCount);
            for (uint32_t v = 0; v < primMesh.vertexCount; ++v)
            {
                Vertex vert;
                vert._position = glm::vec3(gltfScene.m_positions[primMesh.vertexOffset + v]);
                vert._normal   = glm::vec3(gltfScene.m_normals[primMesh.vertexOffset + v]);
                vert._texCoord = glm::vec2(gltfScene.m_texcoords0[primMesh.vertexOffset + v]);
                //TODO:nvpro_core gltfScene loader got some issue with tangent, so tangent is set to 0 tmeporarily
                // if (gltfScene.m_tangents.size() > 0)
                //     vert._tangent = glm::vec3(gltfScene.m_tangents[primMesh.vertexOffset + v]);
                // else
                vert._tangent = glm::vec3(0.0);
                vertices.push_back(vert);
            }
            Buffer*         vBuffer      = _app->_bufferPool.alloc<Buffer>();
            VkCommandBuffer tmpCmdBuffer = _app->createTempCmdBuffer();
            VkDeviceSize    vBufferSize  = vertices.size() * sizeof(Vertex);
            nvvk::Buffer    gpuVBuffer   = _app->_alloc.createBuffer(
                tmpCmdBuffer, vBufferSize, vertices.data(),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT |
                    VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
            _app->submitTempCmdBuffer(tmpCmdBuffer);
            NAME_VK(gpuVBuffer.buffer);

            vBuffer->buffer     = gpuVBuffer.buffer;
            vBuffer->address    = gpuVBuffer.address;
            vBuffer->memHandle  = gpuVBuffer.memHandle;
            m_cacheBuffers[key] = vBuffer;
            vBufferref          = vBuffer;
        }
        else
        {
            vBufferref = (find->second);
        }

        std::vector<uint32_t> indices;
        indices.reserve(primMesh.indexCount);
        for (uint32_t i = 0; i < primMesh.indexCount; ++i)
        {
            indices.push_back(gltfScene.m_indices[primMesh.firstIndex + i]);
        }
        Buffer*         iBuffer      = _app->_bufferPool.alloc<Buffer>();
        VkDeviceSize    iBufferSize  = indices.size() * sizeof(uint32_t);
        VkCommandBuffer tmpCmdBuffer = _app->createTempCmdBuffer();
        nvvk::Buffer    gpuIBuffer   = _app->_alloc.createBuffer(
            tmpCmdBuffer, iBufferSize, indices.data(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT |
                VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
        _app->submitTempCmdBuffer(tmpCmdBuffer);
        iBuffer->buffer    = gpuIBuffer.buffer;
        iBuffer->address   = gpuIBuffer.address;
        iBuffer->memHandle = gpuIBuffer.memHandle;
        iBufferref         = iBuffer;

        _sceneVBuffers.push_back(*vBufferref);
        _sceneIBuffers.push_back(*iBufferref);
        _sceneMeshes.emplace_back(Mesh{vBufferref->address, iBufferref->address,
                                       primMesh.materialIndex + (int) materialOffset,
                                       primMesh.indexCount / 3, primMesh.vertexCount,
                                       static_cast<uint32_t>(_sceneVBuffers.size() - 1),
                                       static_cast<uint32_t>(_sceneIBuffers.size() - 1)});
        _dynamicUniformData.push_back({.model = glm::mat4(1.0f),
                                 .matIdx = static_cast<uint32_t>(primMesh.materialIndex + (int) materialOffset)});
        if (std::find(lightMateiralIdx.begin(), lightMateiralIdx.end(),
                      primMesh.materialIndex + (int) materialOffset) != lightMateiralIdx.end())
        {
            _emissiveMeshIdx.push_back(_sceneMeshes.size() - 1);
        }
    }

    auto         cmd            = _app->createTempCmdBuffer();
    _materialBuffer             = _app->_bufferPool.alloc<Buffer>();
    nvvk::Buffer materialBuf    = _app->_alloc.createBuffer(
        cmd, sizeof(Material) * _sceneMaterials.size(), _sceneMaterials.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
    _app->submitTempCmdBuffer(cmd);
    _materialBuffer->buffer            = materialBuf.buffer;
    _materialBuffer->address           = materialBuf.address;
    _materialBuffer->memHandle         = materialBuf.memHandle;
    _materialBuffer->descriptor.buffer = materialBuf.buffer;
    _materialBuffer->descriptor.offset = 0;
    _materialBuffer->descriptor.range  = VK_WHOLE_SIZE;
    NAME_VK(_materialBuffer->buffer);

    for (const auto& texture : model.images)
    {
        auto              cmd       = _app->createTempCmdBuffer();
        VkImageCreateInfo imageInfo = nvvk::makeImage2DCreateInfo(
            VkExtent2D{(uint32_t) texture.width, (uint32_t) texture.height},
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            true);
        Texture*    tex = _app->_texturePool.alloc<Texture>();
        nvvk::Image image =
            _app->_alloc.createImage(cmd, texture.image.size(), texture.image.data(), imageInfo,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        nvvk::cmdGenerateMipmaps(cmd, image.image, imageInfo.format,
                                 {imageInfo.extent.width, imageInfo.extent.height},
                                 nvvk::mipLevels(imageInfo.extent));
        _app->submitTempCmdBuffer(cmd);

        VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.minFilter           = VK_FILTER_LINEAR;
        samplerInfo.magFilter           = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod              = 0;
        samplerInfo.maxLod              = (float) imageInfo.mipLevels;
        samplerInfo.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkImageViewCreateInfo viewInfo  = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
        auto nvvkTexture                = _app->_alloc.createTexture(image, viewInfo, samplerInfo);
        tex->image        = nvvkTexture.image;
        tex->memHandle    = image.memHandle;
        tex->descriptor   = nvvkTexture.descriptor;
        tex->_debugName   = texture.uri;
        tex->_format      = VK_FORMAT_R8G8B8A8_UNORM;
        tex->_mipmapLevel = imageInfo.mipLevels;
        _sceneTextures.push_back(tex);
    }

    _instanceBuffer                    = _app->_bufferPool.alloc<Buffer>();
    VkCommandBuffer tmpCmdBuffer       = _app->createTempCmdBuffer();
    VkDeviceSize    instanceBufferSize = sizeof(Mesh) * gltfScene.m_nodes.size();
    nvvk::Buffer    gpuInstanceBuffer  = _app->_alloc.createBuffer(
        tmpCmdBuffer, instanceBufferSize, _sceneMeshes.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
    _app->submitTempCmdBuffer(tmpCmdBuffer);
    _instanceBuffer->buffer            = gpuInstanceBuffer.buffer;
    _instanceBuffer->address           = gpuInstanceBuffer.address;
    _instanceBuffer->memHandle         = gpuInstanceBuffer.memHandle;
    _instanceBuffer->descriptor.buffer = gpuInstanceBuffer.buffer;
    _instanceBuffer->descriptor.offset = 0;
    _instanceBuffer->descriptor.range  = instanceBufferSize;
    NAME_VK(_instanceBuffer->buffer);
    if (_emissiveMeshIdx.empty()) _emissiveMeshIdx.push_back(0);
    _lightMeshIdxBuffer                = _app->_bufferPool.alloc<Buffer>();
    tmpCmdBuffer                       = _app->createTempCmdBuffer();
    nvvk::Buffer gpuLightMeshIdxBuffer = _app->_alloc.createBuffer(
        tmpCmdBuffer, sizeof(uint32_t) * _emissiveMeshIdx.size(), _emissiveMeshIdx.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
    _app->submitTempCmdBuffer(tmpCmdBuffer);
    _lightMeshIdxBuffer->buffer            = gpuLightMeshIdxBuffer.buffer;
    _lightMeshIdxBuffer->address           = gpuLightMeshIdxBuffer.address;
    _lightMeshIdxBuffer->memHandle         = gpuLightMeshIdxBuffer.memHandle;
    _lightMeshIdxBuffer->descriptor.buffer = gpuLightMeshIdxBuffer.buffer;
    _lightMeshIdxBuffer->descriptor.offset = 0;
    _lightMeshIdxBuffer->descriptor.range  = sizeof(uint32_t) * _emissiveMeshIdx.size();
    NAME_VK(_lightMeshIdxBuffer->buffer);

    _dynamicUniformBuffer = _app->_bufferPool.alloc<Buffer>();
    VkDeviceSize dynamicUniformBufferSize = sizeof(DynamicStruct) * _dynamicUniformData.size();
    VkBufferCreateInfo bufferInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = dynamicUniformBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    nvvk::Buffer nvvkBuffer = _app->_alloc.createBuffer(bufferInfo,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _dynamicUniformBuffer->buffer            = nvvkBuffer.buffer;
    _dynamicUniformBuffer->address           = nvvkBuffer.address;
    _dynamicUniformBuffer->memHandle         = nvvkBuffer.memHandle;
    _dynamicUniformBuffer->descriptor.buffer = nvvkBuffer.buffer;
    _dynamicUniformBuffer->descriptor.offset = 0;
    _dynamicUniformBuffer->descriptor.range  = sizeof(DynamicStruct);
    void * data = PlayApp::MapBuffer(*_dynamicUniformBuffer);
    memcpy(data, _dynamicUniformData.data(), dynamicUniformBufferSize);
    PlayApp::UnmapBuffer(*_dynamicUniformBuffer);
    NAME_VK(_dynamicUniformBuffer->buffer);
}

} // namespace Play