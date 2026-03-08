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

class GaussianScene
{
public:
    bool load(const std::filesystem::path& path);

    const std::vector<float3>& getPositions() const
    {
        return _positions;
    }

    const std::vector<float3>& getColors() const
    {
        return _colors;
    }

    const std::vector<float>& getOpacities() const
    {
        return _opacities;
    }

    const std::vector<float3>& getScales() const
    {
        return _scales;
    }

    const std::vector<float4>& getRotations() const
    {
        return _rotations;
    }

    const GaussianSceneMeta& getMeta() const
    {
        return _meta;
    }
    uint32_t getVertexCount() const
    {
        return static_cast<uint32_t>(_positions.size());
    }

    Buffer* getSplatMetaGPUBuffer()
    {
        return _splatMetaBuffer;
    }

    Buffer* getPositionGPUBuffer()
    {
        return _positionBuffer;
    }

    Buffer* getColorGPUBuffer()
    {
        return _colorBuffer;
    }

    Buffer* getOpacityGPUBuffer()
    {
        return _opacityBuffer;
    }

    Buffer* getScaleGPUBuffer()
    {
        return _scaleBuffer;
    }

    Buffer* getRotationGPUBuffer()
    {
        return _rotationBuffer;
    }

private:
    std::vector<float3> _positions;
    std::vector<float3> _colors;
    std::vector<float>  _opacities;
    std::vector<float3> _scales;
    std::vector<float4> _rotations;

    Buffer*           _positionBuffer   = nullptr;
    Buffer*           _colorBuffer      = nullptr;
    Buffer*           _opacityBuffer    = nullptr;
    Buffer*           _scaleBuffer      = nullptr;
    Buffer*           _rotationBuffer   = nullptr;
    Buffer*           _splatMetaBuffer  = nullptr;
    GaussianSceneMeta _meta{};
}; // class GaussianScene

} // namespace Play

#endif // RENDER_SCENE_H