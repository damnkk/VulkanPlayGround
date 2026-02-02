#include "MeshCollector.h"
#include "renderer/Renderer.h"
#include "SceneManager.h"
#include "core/PlayCamera.h"
#include <array>
#include <filesystem>
#include <assert.h>
#include <glm/glm.hpp>
#include <nvvkgltf/scene.hpp>
#include <nvutils/parallel_work.hpp>
namespace Play
{
namespace
{
struct FrustumPlane
{
    glm::vec3 normal;
    float     d;
};

std::array<FrustumPlane, 6> extractFrustumPlanes(const glm::mat4& viewProj)
{
    // glm is column-major: build rows manually
    const glm::vec4 row0 = {viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]};
    const glm::vec4 row1 = {viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]};
    const glm::vec4 row2 = {viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]};
    const glm::vec4 row3 = {viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]};

    std::array<FrustumPlane, 6> planes = {
        FrustumPlane{glm::vec3(row3 + row0), (row3 + row0).w}, // left
        FrustumPlane{glm::vec3(row3 - row0), (row3 - row0).w}, // right
        FrustumPlane{glm::vec3(row3 + row1), (row3 + row1).w}, // bottom
        FrustumPlane{glm::vec3(row3 - row1), (row3 - row1).w}, // top
        FrustumPlane{glm::vec3(row2), row2.w},                 // near (ZO)
        FrustumPlane{glm::vec3(row3 - row2), (row3 - row2).w}  // far
    };

    for (auto& plane : planes)
    {
        const float len = glm::length(plane.normal);
        if (len > 0.0f)
        {
            plane.normal /= len;
            plane.d /= len;
        }
    }
    return planes;
}

bool isAabbInsideFrustum(const nvutils::Bbox& bbox, const std::array<FrustumPlane, 6>& planes)
{
    const glm::vec3 bmin = bbox.min();
    const glm::vec3 bmax = bbox.max();

    for (const auto& plane : planes)
    {
        const glm::vec3& n = plane.normal;
        glm::vec3        p;
        p.x = (n.x >= 0.0f) ? bmax.x : bmin.x;
        p.y = (n.y >= 0.0f) ? bmax.y : bmin.y;
        p.z = (n.z >= 0.0f) ? bmax.z : bmin.z;

        if (glm::dot(n, p) + plane.d < 0.0f)
        {
            return false;
        }
    }
    return true;
}
} // namespace

std::vector<MeshBatch>& MeshCollector::collectMeshBatches()
{
    assert(_renderer);
    assert(_renderer->getSceneManager());
    PlayCamera*                   activeCamera  = _renderer->getActiveCamera();
    SceneManager*                 sceneMgr      = _renderer->getSceneManager();
    const glm::mat4               view          = activeCamera->getCameraManipulator()->getViewMatrix();
    const glm::mat4               proj          = activeCamera->getCameraManipulator()->getPerspectiveMatrix();
    const glm::mat4               viewProj      = proj * view;
    const auto                    frustumPlanes = extractFrustumPlanes(viewProj);
    std::vector<nvvkgltf::Scene>& scenes        = sceneMgr->getCpuScene();

    _meshBatches.clear();
    // 使用 Map 加速查找: key = (sceneID << 32) | materialID, value = index in _meshBatches
    // 预估一个大小避免 rehash，例如场景里可能有 100 种材质
    std::unordered_map<uint64_t, size_t> batchMap;
    batchMap.reserve(128);

    for (int i = 0; i < scenes.size(); ++i)
    {
        nvvkgltf::Scene& scene = scenes[i];
        for (int j = 0; j < scene.getRenderNodes().size(); ++j)
        {
            const nvvkgltf::RenderNode& renderNode  = scene.getRenderNodes()[j];
            uint32_t                    materialID  = renderNode.materialID;
            Material*                   materialRef = sceneMgr->getVkScene()[i].getDefaultMaterials()[materialID];

            // check visibility FIRST
            glm::vec3 minValues = {0.f, 0.f, 0.f};
            glm::vec3 maxValues = {0.f, 0.f, 0.f};

            const nvvkgltf::RenderPrimitive& rprim    = scene.getRenderPrimitive(renderNode.renderPrimID);
            const tinygltf::Accessor&        accessor = scene.getModel().accessors[rprim.pPrimitive->attributes.at("POSITION")];
            if (!accessor.minValues.empty()) minValues = glm::vec3(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]);
            if (!accessor.maxValues.empty()) maxValues = glm::vec3(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]);
            nvutils::Bbox bbox(minValues, maxValues);
            bbox = bbox.transform(renderNode.worldMatrix);

            if (!isAabbInsideFrustum(bbox, frustumPlanes))
            {
                continue;
            }

            // Only proceed to batch collection if visible
            // Generate unique key for scene + material combination
            uint64_t   key   = (static_cast<uint64_t>(i) << 32) | (uint64_t) materialRef;
            MeshBatch* batch = nullptr;

            auto it = batchMap.find(key);
            if (it != batchMap.end())
            {
                // Found existing batch
                batch = &_meshBatches[it->second];
            }
            else
            {
                // Create new batch
                MeshBatch newBatch{};
                newBatch.sceneID    = i;
                newBatch.materialID = materialID;

                // Record index
                batchMap[key] = _meshBatches.size();
                _meshBatches.push_back(newBatch);

                batch = &_meshBatches.back();
            }

            batch->renderNodeIDs.push_back(j);
        }
    }

    // No need to remove empty batches anymore, as we only create them for visible nodes.
    // However, if strict correctness is required for edge cases (should be none here):
    // _meshBatches.erase(...)

    return _meshBatches;
}

} // namespace Play