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
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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
    uint32_t textureOffset  = static_cast<uint32_t>(_sceneTextures.size());
    uint32_t vboOffset      = static_cast<uint32_t>(_sceneVBuffers.size());
    uint32_t iboOffset      = static_cast<uint32_t>(_sceneIBuffers.size());
    uint32_t meshOffset     = static_cast<uint32_t>(_sceneMeshes.size());
    uint32_t materialOffset = static_cast<uint32_t>(_sceneMaterials.size());

    // Assimp load implementation
    Assimp::Importer importer;
    const aiScene* Assimpscene = importer.ReadFile(path, 
        aiProcess_Triangulate | 
        aiProcess_FlipUVs | 
        aiProcess_CalcTangentSpace | 
        aiProcess_GenNormals);
    
    if (!Assimpscene || Assimpscene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Assimpscene->mRootNode) {
        LOGE_FILELINE("Failed to load model with Assimp: %s\n", importer.GetErrorString());
        return;
    } else {
        LOGI("Model loaded successfully with Assimp: %s\n", path.c_str());
    }



    std::vector<uint32_t> lightMateiralIdx;
    int                   materialCnt = 0;
    for (unsigned int m = 0; m < Assimpscene->mNumMaterials; ++m)
    {
        const aiMaterial* assimpMat = Assimpscene->mMaterials[m];
        Material mat = {};
        
        // PBR Base Color
        aiColor3D color;
        if (assimpMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            mat.pbrBaseColorFactor = vec4(color.r, color.g, color.b, 1.0f);
        } else {
            mat.pbrBaseColorFactor = vec4(1.0f);
        }
        
        // Metallic factor
        float metallic;
        if (assimpMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
            mat.pbrMetallicFactor = metallic;
        } else {
            mat.pbrMetallicFactor = 0.0f;
        }
        
        // Roughness factor
        float roughness;
        if (assimpMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
            mat.pbrRoughnessFactor = roughness;
        } else {
            mat.pbrRoughnessFactor = 1.0f;
        }
        
        // Emissive factor
        aiColor3D emissive;
        if (assimpMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            mat.emissiveFactor = vec3(emissive.r, emissive.g, emissive.b);
        } else {
            mat.emissiveFactor = vec3(0.0f);
        }
        
        // Alpha mode and cutoff
        float opacity;
        if (assimpMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
            mat.pbrBaseColorFactor.z = opacity;
            if (opacity < 1.0f) {
                mat.alphaMode = 1; // BLEND mode
            } else {
                mat.alphaMode = 0; // OPAQUE mode
            }
        } else {
            mat.alphaMode = 0;
        }
        mat.alphaCutoff = 0.5f;
        
        // Double sided
        int twoSided;
        if (assimpMat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) {
            mat.doubleSided = twoSided;
        } else {
            mat.doubleSided = 0;
        }
        
        // Initialize texture indices to -1 (no texture)
        mat.pbrBaseColorTexture = 0;
        mat.pbrMetallicRoughnessTexture = -1;
        mat.emissiveTexture = -1;
        mat.normalTexture = -1;
        mat.transmissionTexture = -1;
        mat.thicknessTexture = -1;
        mat.clearcoatTexture = -1;
        mat.clearcoatRoughnessTexture = -1;
        
        // Normal texture scale
        mat.normalTextureScale = 1.0f;
        
        // UV transform (identity)
        mat.uvTransform = mat4(1.0f);
        
        // Default values for advanced PBR properties
        mat.unlit = 0;
        mat.transmissionFactor = 0.0f;
        mat.ior = 1.5f;
        mat.anisotropyDirection = vec3(1.0f, 0.0f, 0.0f);
        mat.anisotropy = 0.0f;
        mat.attenuationColor = vec3(1.0f);
        mat.thicknessFactor = 0.0f;
        mat.attenuationDistance = 1e10f;
        mat.clearcoatFactor = 0.0f;
        mat.clearcoatRoughness = 0.0f;
        mat.sheen = 0;

        _sceneMaterials.push_back(mat);
        if (mat.emissiveFactor != vec3(0.0f))
        {
            lightMateiralIdx.push_back(materialOffset + materialCnt);
        }
        ++materialCnt;
    }
    
    // Process Assimp scene nodes
    auto modelRootNode = this->_app->getScene()._root->addChild(path.substr(path.find_last_of('/') + 1));
    processAssimpNode(Assimpscene->mRootNode, Assimpscene, modelRootNode.get(), meshOffset, materialOffset);
    
    // Create buffers for all processed meshes
    auto cmd = _app->createTempCmdBuffer();
    
    // Create material buffer
    _materialBuffer = _app->_bufferPool.alloc();
    nvvk::Buffer materialBuf = _app->_alloc.createBuffer(
        cmd, sizeof(Material) * _sceneMaterials.size(), _sceneMaterials.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
    _app->submitTempCmdBuffer(cmd);
    _materialBuffer->buffer = materialBuf.buffer;
    _materialBuffer->address = materialBuf.address;
    _materialBuffer->memHandle = materialBuf.memHandle;
    _materialBuffer->descriptor.buffer = materialBuf.buffer;
    _materialBuffer->descriptor.offset = 0;
    _materialBuffer->descriptor.range = VK_WHOLE_SIZE;
    NAME_VK(_materialBuffer->buffer);

    // Load textures from Assimp materials
    for (unsigned int m = 0; m < Assimpscene->mNumMaterials; ++m)
    {
        const aiMaterial* assimpMat = Assimpscene->mMaterials[m];
        
        // Load diffuse/base color texture
        if (assimpMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            aiString texPath;
            if (assimpMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                loadAssimpTexture(path.substr(0, path.find_last_of('/') + 1) + texPath.C_Str());
                _sceneMaterials[m + materialOffset].pbrBaseColorTexture = static_cast<int>(_sceneTextures.size() - 1);
            }
        }
        
        // Load normal texture
        if (assimpMat->GetTextureCount(aiTextureType_NORMALS) > 0)
        {
            aiString texPath;
            if (assimpMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS)
            {
                loadAssimpTexture(path.substr(0, path.find_last_of('/') + 1) + texPath.C_Str());
                _sceneMaterials[m + materialOffset].normalTexture = static_cast<int>(_sceneTextures.size() - 1);
            }
        }
        
        // Load metallic-roughness texture
        if (assimpMat->GetTextureCount(aiTextureType_METALNESS) > 0)
        {
            aiString texPath;
            if (assimpMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS)
            {
                loadAssimpTexture(path.substr(0, path.find_last_of('/') + 1) + texPath.C_Str());
                _sceneMaterials[m + materialOffset].pbrMetallicRoughnessTexture = static_cast<int>(_sceneTextures.size() - 1);
            }
        }
        
        // Load emissive texture
        if (assimpMat->GetTextureCount(aiTextureType_EMISSIVE) > 0)
        {
            aiString texPath;
            if (assimpMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS)
            {
                loadAssimpTexture(path.substr(0, path.find_last_of('/') + 1) + texPath.C_Str());
                _sceneMaterials[m + materialOffset].emissiveTexture = static_cast<int>(_sceneTextures.size() - 1);
            }
        }
    }

    // Create instance buffer
    _instanceBuffer = _app->_bufferPool.alloc();
    VkCommandBuffer tmpCmdBuffer = _app->createTempCmdBuffer();
    VkDeviceSize instanceBufferSize = sizeof(Mesh) * _sceneMeshes.size();
    nvvk::Buffer gpuInstanceBuffer = _app->_alloc.createBuffer(
        tmpCmdBuffer, instanceBufferSize, _sceneMeshes.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
    _app->submitTempCmdBuffer(tmpCmdBuffer);
    _instanceBuffer->buffer = gpuInstanceBuffer.buffer;
    _instanceBuffer->address = gpuInstanceBuffer.address;
    _instanceBuffer->memHandle = gpuInstanceBuffer.memHandle;
    _instanceBuffer->descriptor.buffer = gpuInstanceBuffer.buffer;
    _instanceBuffer->descriptor.offset = 0;
    _instanceBuffer->descriptor.range = instanceBufferSize;
    NAME_VK(_instanceBuffer->buffer);
    
    // Create light mesh index buffer
    if (_emissiveMeshIdx.empty()) _emissiveMeshIdx.push_back(0);
    _lightMeshIdxBuffer = _app->_bufferPool.alloc();
    tmpCmdBuffer = _app->createTempCmdBuffer();
    nvvk::Buffer gpuLightMeshIdxBuffer = _app->_alloc.createBuffer(
        tmpCmdBuffer, sizeof(uint32_t) * _emissiveMeshIdx.size(), _emissiveMeshIdx.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
    _app->submitTempCmdBuffer(tmpCmdBuffer);
    _lightMeshIdxBuffer->buffer = gpuLightMeshIdxBuffer.buffer;
    _lightMeshIdxBuffer->address = gpuLightMeshIdxBuffer.address;
    _lightMeshIdxBuffer->memHandle = gpuLightMeshIdxBuffer.memHandle;
    _lightMeshIdxBuffer->descriptor.buffer = gpuLightMeshIdxBuffer.buffer;
    _lightMeshIdxBuffer->descriptor.offset = 0;
    _lightMeshIdxBuffer->descriptor.range = sizeof(uint32_t) * _emissiveMeshIdx.size();
    NAME_VK(_lightMeshIdxBuffer->buffer);

    // Create dynamic uniform buffer
    _dynamicUniformBuffer = _app->_bufferPool.alloc();
    VkDeviceSize dynamicUniformBufferSize = sizeof(DynamicStruct) * _dynamicUniformData.size();
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = dynamicUniformBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    nvvk::Buffer nvvkBuffer = _app->_alloc.createBuffer(bufferInfo,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _dynamicUniformBuffer->buffer = nvvkBuffer.buffer;
    _dynamicUniformBuffer->address = nvvkBuffer.address;
    _dynamicUniformBuffer->memHandle = nvvkBuffer.memHandle;
    _dynamicUniformBuffer->descriptor.buffer = nvvkBuffer.buffer;
    _dynamicUniformBuffer->descriptor.offset = 0;
    _dynamicUniformBuffer->descriptor.range = sizeof(DynamicStruct);
    void* data = PlayApp::MapBuffer(*_dynamicUniformBuffer);
    memcpy(data, _dynamicUniformData.data(), dynamicUniformBufferSize);
    PlayApp::UnmapBuffer(*_dynamicUniformBuffer);
    NAME_VK(_dynamicUniformBuffer->buffer);
}

void ModelLoader::processAssimpNode(aiNode* node, const aiScene* scene, SceneNode* parentNode, uint32_t meshOffset, uint32_t materialOffset)
{
    // Create a scene node for this Assimp node
    auto currentNode = parentNode;
    if (node != scene->mRootNode) {
        currentNode = parentNode->addChild(std::string(node->mName.C_Str())).get();
    }
    
    // Process meshes in this node and add their indices to the scene node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        uint32_t meshIdx = processAssimpMesh(mesh, scene, meshOffset, materialOffset);
        currentNode->_meshIdx.push_back(meshIdx);
    }
    
    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processAssimpNode(node->mChildren[i], scene, currentNode, meshOffset, materialOffset);
    }
}

uint32_t ModelLoader::processAssimpMesh(aiMesh* mesh, const aiScene* scene, uint32_t meshOffset, uint32_t materialOffset)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex._position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        
        if (mesh->HasNormals()) {
            vertex._normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex._normal = vec3(0.0f, 1.0f, 0.0f);
        }
        
        if (mesh->HasTangentsAndBitangents()) {
            vertex._tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
        } else {
            vertex._tangent = vec3(1.0f, 0.0f, 0.0f);
        }
        
        if (mesh->mTextureCoords[0]) {
            vertex._texCoord = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex._texCoord = vec2(0.0f);
        }
        
        vertices.push_back(vertex);
    }
    
    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Create vertex buffer
    Buffer* vBuffer = _app->_bufferPool.alloc();
    VkCommandBuffer tmpCmdBuffer = _app->createTempCmdBuffer();
    VkDeviceSize vBufferSize = vertices.size() * sizeof(Vertex);
    nvvk::Buffer gpuVBuffer = _app->_alloc.createBuffer(
        tmpCmdBuffer, vBufferSize, vertices.data(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT |
        VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    _app->submitTempCmdBuffer(tmpCmdBuffer);
    
    vBuffer->buffer = gpuVBuffer.buffer;
    vBuffer->address = gpuVBuffer.address;
    vBuffer->memHandle = gpuVBuffer.memHandle;
    
    // Create index buffer
    Buffer* iBuffer = _app->_bufferPool.alloc();
    VkDeviceSize iBufferSize = indices.size() * sizeof(uint32_t);
    tmpCmdBuffer = _app->createTempCmdBuffer();
    nvvk::Buffer gpuIBuffer = _app->_alloc.createBuffer(
        tmpCmdBuffer, iBufferSize, indices.data(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT |
        VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    _app->submitTempCmdBuffer(tmpCmdBuffer);
    
    iBuffer->buffer = gpuIBuffer.buffer;
    iBuffer->address = gpuIBuffer.address;
    iBuffer->memHandle = gpuIBuffer.memHandle;
    
    _sceneVBuffers.push_back(*vBuffer);
    _sceneIBuffers.push_back(*iBuffer);
    
    // Create mesh and get its index
    uint32_t meshIndex = static_cast<uint32_t>(_sceneMeshes.size());
    _sceneMeshes.emplace_back(Mesh{
        vBuffer->address, iBuffer->address,
        static_cast<int>(mesh->mMaterialIndex + materialOffset),
        static_cast<uint32_t>(indices.size() / 3), 
        static_cast<uint32_t>(vertices.size()),
        static_cast<uint32_t>(_sceneVBuffers.size() - 1),
        static_cast<uint32_t>(_sceneIBuffers.size() - 1)
    });
    
    _dynamicUniformData.push_back({
        .model = glm::mat4(1.0f),
        .matIdx = static_cast<uint32_t>(mesh->mMaterialIndex + materialOffset)
    });
    
    // Check if this mesh uses an emissive material
    if (mesh->mMaterialIndex + materialOffset < _sceneMaterials.size()) {
        auto& material = _sceneMaterials[mesh->mMaterialIndex + materialOffset];
        if (material.emissiveFactor != vec3(0.0f)) {
            _emissiveMeshIdx.push_back(static_cast<int32_t>(meshIndex));
        }
    }
    
    return meshIndex;
}

void ModelLoader::loadAssimpTexture(const std::string& texturePath)
{
    // Check if texture already loaded
    for (const auto& tex : _sceneTextures) {
        if (tex->_metadata._debugName == texturePath) {
            return; // Already loaded
        }
    }
    
    // Load image using STB
    int width, height, channels;
    stbi_uc* pixels = stbi_load(texturePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        LOGE("Failed to load texture: %s", texturePath.c_str());
        return;
    }
    
    // Create Vulkan texture
    auto cmd = _app->createTempCmdBuffer();
    VkImageCreateInfo imageInfo = nvvk::makeImage2DCreateInfo(
        VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        VK_FORMAT_R8G8B8A8_UNORM, 
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        true);
    
    Texture* tex = _app->_texturePool.alloc();
    size_t imageSize = width * height * 4; // RGBA
    nvvk::Image image = _app->_alloc.createImage(cmd, imageSize, pixels, imageInfo,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    nvvk::cmdGenerateMipmaps(cmd, image.image, imageInfo.format,
                            {imageInfo.extent.width, imageInfo.extent.height},
                            nvvk::mipLevels(imageInfo.extent));
    _app->submitTempCmdBuffer(cmd);
    
    // Create sampler and image view
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0;
    samplerInfo.maxLod = static_cast<float>(imageInfo.mipLevels);
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    VkImageViewCreateInfo viewInfo = nvvk::makeImageViewCreateInfo(image.image, imageInfo);
    auto nvvkTexture = _app->_alloc.createTexture(image, viewInfo, samplerInfo);
    
    tex->image = nvvkTexture.image;
    tex->memHandle = image.memHandle;
    tex->descriptor = nvvkTexture.descriptor;
    tex->_metadata._debugName = texturePath;
    tex->_metadata._format = VK_FORMAT_R8G8B8A8_UNORM;
    tex->_metadata._mipmapLevel = imageInfo.mipLevels;
    tex->_metadata._extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    _sceneTextures.push_back(tex);
    
    // Free CPU image data
    stbi_image_free(pixels);
}

} // namespace Play