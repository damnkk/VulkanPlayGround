#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H
#include <nvvkgltf/scene_vk.hpp>
#include <nvvkgltf/scene_rtx.hpp>
#include "miniply.h"
#include <vector>
#include <filesystem>
namespace Play
{
class Material;
class RenderScene : public nvvkgltf::SceneVk
{
public:
    void     recordingCommandBuffer(VkCommandBuffer cmdBuffer);
    uint32_t getTextureOffset() const
    {
        return textureOffset;
    }
    void setTextureOffset(uint32_t offset)
    {
        textureOffset = offset;
    }

    void                    fillDefaultMaterials(nvvkgltf::Scene& scene);
    std::vector<Material*>& getDefaultMaterials()
    {
        return _defaultMaterials;
    }

private:
    std::vector<Material*> _defaultMaterials;
    uint32_t               textureOffset = 0;
}; // class RenderScene

class RTScene : public nvvkgltf::SceneRtx
{
}; // class RTScene

// Per-vertex data layout matching the PLY binary format exactly (56 bytes)
struct GaussianVertex
{
    float x, y, z;
    float f_dc_0, f_dc_1, f_dc_2;
    float opacity;
    float scale_0, scale_1, scale_2;
    float rot_0, rot_1, rot_2, rot_3;
};
static_assert(sizeof(GaussianVertex) == 56, "GaussianVertex size mismatch");

// Camera metadata embedded in the PLY file
struct GaussianSceneMeta
{
    float    extrinsic[16]; // 4x4 camera extrinsic matrix
    float    intrinsic[9];  // 3x3 camera intrinsic matrix
    uint32_t imageSize[2];  // width, height
    int32_t  frame[2];
    float    disparity[2]; // min, max disparity
    uint8_t  colorSpace;
    uint8_t  version[3];
};

class GaussianScene
{
public:
    bool load(const std::filesystem::path& path);

    const std::vector<GaussianVertex>& getVertices() const
    {
        return _vertices;
    }
    const GaussianSceneMeta& getMeta() const
    {
        return _meta;
    }
    uint32_t getVertexCount() const
    {
        return static_cast<uint32_t>(_vertices.size());
    }

private:
    std::vector<GaussianVertex> _vertices;
    GaussianSceneMeta           _meta{};
}; // class GaussianScene

} // namespace Play

#endif // RENDER_SCENE_H