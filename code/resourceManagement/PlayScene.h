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

    const std::vector<float4>& getColors() const
    {
        return _colors;
    }

    const std::vector<float>& getCovariances() const
    {
        return _covariances;
    }

    const std::vector<float>& getShRestCoefficients() const
    {
        return _shRestCoefficients;
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

    Buffer* getCovarianceGPUBuffer()
    {
        return _covarianceBuffer;
    }

    Buffer* getShRestGPUBuffer()
    {
        return _shRestBuffer;
    }

    Buffer* getSceneUniformBuffer()
    {
        return _sceneUniformBuffer;
    }

private:
    std::vector<float3> _positions;
    std::vector<float4> _colors;
    std::vector<float>  _covariances;
    std::vector<float>  _shRestCoefficients;

    Buffer* _positionBuffer   = nullptr;
    Buffer* _colorBuffer      = nullptr;
    Buffer* _covarianceBuffer = nullptr;
    Buffer* _shRestBuffer     = nullptr;
    Buffer* _splatMetaBuffer  = nullptr;

    Buffer*           _sceneUniformBuffer = nullptr;
    GaussianSceneMeta _meta{};
}; // class GaussianScene

} // namespace Play

#endif // RENDER_SCENE_H
