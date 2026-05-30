#ifndef SCENE_ASSETS_H
#define SCENE_ASSETS_H

#include "CpuScene.h"
#include <filesystem>
#include "nvshaders/gltf_scene_io.h.slang"
#include "Resource.h"

namespace Play
{

struct AABB
{
    glm::vec3 min = {0.0f, 0.0f, 0.0f};
    glm::vec3 max = {0.0f, 0.0f, 0.0f};
};

struct ModelSubmeshAsset
{
    uint32_t meshID = INVALID_SCENE_ID; // idx to meshInfos
    AABB     bbox;
};

struct ModelNodeAsset
{
    std::string           name;
    uint32_t              parent       = INVALID_SCENE_ID;
    uint32_t              firstChild   = INVALID_SCENE_ID;
    uint32_t              nextSibling  = INVALID_SCENE_ID;
    uint32_t              transformIdx = INVALID_SCENE_ID;
    glm::vec3             translation  = {0.0f, 0.0f, 0.0f};
    glm::vec3             rotation     = {0.0f, 0.0f, 0.0f};
    glm::vec3             scale        = {1.0f, 1.0f, 1.0f};
    std::vector<uint32_t> submeshIdx;
};

struct ModelRenderableTemplate
{
    uint32_t  submeshIndex = INVALID_SCENE_ID;
    uint32_t  nodeIndex    = INVALID_SCENE_ID;
    glm::mat4 localToModel = glm::mat4(1.0f);
};

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

struct LightInfo
{
    glm::vec3 lightPosition;
};

struct ModelAsset
{
    std::string                    name;
    std::filesystem::path          sourcePath;
    std::vector<ModelSubmeshAsset> submeshes;
    std::vector<ModelNodeAsset>    nodes;
    std::vector<glm::mat4>         transforms;
    uint32_t                       rootNode = INVALID_SCENE_ID;

    Buffer*                       materialBuffer  = nullptr;
    Buffer*                       meshInfoBuffer  = nullptr;
    Buffer*                       lightInfoBuffer = nullptr;
    std::vector<RayTracingASInfo> accelerationStructures;

    uint32_t generation = 1;
};

} // namespace Play

#endif // SCENE_ASSETS_H
