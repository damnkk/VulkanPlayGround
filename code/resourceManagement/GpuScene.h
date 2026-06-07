#ifndef GPU_SCENE_H
#define GPU_SCENE_H

#include "SceneAssets.h"

namespace Play
{

enum class GpuSceneType : uint32_t
{
    eRaster,
    eGaussian,
    eRayTracing
};

struct GpuSceneCommonData
{
    Buffer* transformBuffer = nullptr;
    Buffer* materialBuffer  = nullptr;
    Buffer* vertexBuffer    = nullptr;
    Buffer* indexBuffer     = nullptr;

    std::vector<glm::mat4>                  transforms;
    std::vector<MeshInfo>                   meshInfos;
    std::vector<shaderio::GltfShadeMaterial> materials;
    std::vector<shaderio::GltfTextureInfo>   textureInfos;
};

struct RasterGPUData
{
    bool enabled = false;
};

struct RayTracingGPUData
{
    bool enabled = false;
    std::vector<RayTracingASInfo> accelerationStructures;
};

struct GpuModelRange
{
    uint32_t firstMeshInfo    = 0;
    uint32_t meshInfoCount    = 0;
    uint32_t firstMaterial    = 0;
    uint32_t materialCount    = 0;
    uint32_t firstTextureInfo = 0;
    uint32_t textureInfoCount = 0;
};

class GpuScene
{
public:
    virtual ~GpuScene() = default;

    virtual GpuSceneType getType() const = 0;
    virtual void         clear();

    ModelAssetID registerModel(ModelAssetPackage&& package);
    void         updateTransforms(const CpuScene& scene);

    uint64_t getSourceSceneRevision() const
    {
        return _sourceSceneRevision;
    }

    const GpuSceneCommonData& getCommonData() const
    {
        return _common;
    }

    const std::vector<ModelAsset>& getModels() const
    {
        return _models;
    }

    const std::vector<GpuModelRange>& getModelRanges() const
    {
        return _modelRanges;
    }

    const std::vector<RefPtr<Texture>>& getSceneTextures() const
    {
        return _sceneTextures;
    }

protected:
    uint32_t ensureSceneTexture(ModelTextureResource&& texture);
    void     registerRasterData(const ModelAsset& model, const GpuModelRange& range);
    void     registerRayTracingData(const ModelAsset& model, const GpuModelRange& range);

    GpuSceneCommonData             _common;
    RasterGPUData                  _rasterData;
    RayTracingGPUData              _rtData;
    std::vector<ModelAsset>        _models;
    std::vector<GpuModelRange>     _modelRanges;
    std::vector<RefPtr<Texture>>   _sceneTextures;
    std::vector<std::filesystem::path> _sceneTextureSources;
    std::vector<RefPtr<Buffer>>    _ownedBuffers;
    uint64_t                       _sourceSceneRevision = 0;
};

class RasterGpuScene : public GpuScene
{
public:
    RasterGpuScene()
    {
        _rasterData.enabled = true;
    }

    GpuSceneType getType() const override
    {
        return GpuSceneType::eRaster;
    }
};

class RayTracingGpuScene : public GpuScene
{
public:
    RayTracingGpuScene()
    {
        _rtData.enabled = true;
    }

    GpuSceneType getType() const override
    {
        return GpuSceneType::eRayTracing;
    }
};

} // namespace Play

#endif // GPU_SCENE_H
