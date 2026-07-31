#ifndef MODEL_GPU_ASSETS_H
#define MODEL_GPU_ASSETS_H

#include "resourceManagement/vulkan/resources/Resource.h"
#include "nvshaders/gltf_scene_io.h.slang"
#include <VPGLoader/Model.hpp>
#include <glm/glm.hpp>

namespace Play
{

// GPU-facing data derived from vpgloader::LoadedModel at the upload boundary.
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

struct UploadedModelTexture
{
    std::string           name;
    std::filesystem::path sourcePath;
    RefPtr<Texture>       texture;
    uint32_t              mipLevels = 0;
    bool                  isSrgb    = true;

    bool isResident() const
    {
        return texture && texture->isValid();
    }
};

struct ModelGpuResources
{
    RefPtr<Buffer>                transformBuffer   = nullptr;
    RefPtr<Buffer>                materialBuffer    = nullptr;
    RefPtr<Buffer>                textureInfoBuffer = nullptr;
    RefPtr<Buffer>                meshInfoBuffer    = nullptr;
    RefPtr<Buffer>                lightInfoBuffer   = nullptr;
    std::vector<RayTracingASInfo> accelerationStructures;
    std::vector<RefPtr<Buffer>>   ownedBuffers;
};

struct GpuModelRenderable
{
    uint32_t           meshIndex = vpgloader::InvalidModelIndex;
    glm::mat4          localToModel = glm::mat4(1.0f);
    vpgloader::AABB    modelBounds;
};

struct UploadedModel
{
    vpgloader::ModelHandle                   model;
    ModelGpuResources                        resources;
    std::vector<MeshInfo>                    meshInfos;
    std::vector<shaderio::GltfShadeMaterial> materials;
    std::vector<shaderio::GltfTextureInfo>   textureInfos;
    std::vector<UploadedModelTexture>         textures;
};

} // namespace Play

#endif // MODEL_GPU_ASSETS_H
