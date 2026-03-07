#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H
#include <nvvkgltf/scene_vk.hpp>
#include <nvvkgltf/scene_rtx.hpp>
#include "miniply.h"
#include <vector>
#include <filesystem>
#include "newshaders/gaussian/gaussianLib.h.slang"

namespace Play
{
class Material;
class Buffer;
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

static_assert(sizeof(GaussianVertex) == 56, "GaussianVertex size mismatch");

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

    Buffer* getSplatGPUBuffer()
    {
        return _splatBuffer;
    }

    Buffer* getSplatMetaGPUBuffer()
    {
        return _splatMetaBuffer;
    }

private:
    std::vector<GaussianVertex> _vertices;
    Buffer*                     _splatBuffer;
    Buffer*                     _splatMetaBuffer;
    GaussianSceneMeta           _meta{};
}; // class GaussianScene

} // namespace Play

#endif // RENDER_SCENE_H