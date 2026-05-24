#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include "filesystem"
#include "nvvk/descriptors.hpp"
#include "PlayScene.h"
#include "CpuScene.h"
#include "SceneAssets.h"
#include "RasterGpuScene.h"
#include "ModelLoading.h"
#include "core/RefCounted.h"
namespace Play
{
class RenderSession;
class Texture;
class SceneManager
{
public:
    static constexpr uint32_t SceneTextureBinding      = 3;
    static constexpr uint32_t SceneTexturePoolCapacity = 1024;

    SceneManager(GpuSceneType gpuSceneType = GpuSceneType::eRaster);
    GpuScene* getGpuScene()
    {
        return _gpuScene.get();
    }
    const GpuScene* getGpuScene() const
    {
        return _gpuScene.get();
    }
    GpuSceneType getGpuSceneType() const
    {
        return _gpuScene ? _gpuScene->getType() : GpuSceneType::eRaster;
    }
    ModelLoadResult loadModel(const std::filesystem::path& filename, const ModelLoadingConfig& loadingCfg = ModelLoadingConfig{});
    template <typename T>
    SceneManager& addScene(std::filesystem::path filename);
    template <typename T>
    SceneManager& addScenes(std::vector<std::filesystem::path> filenames);
    GaussianScene& getGaussianScene()
    {
        return *static_cast<GaussianScene*>(_gpuScene.get());
    }
    CpuScene& getSceneGraph()
    {
        return _cpuScene;
    }
    const CpuScene& getSceneGraph() const
    {
        return _cpuScene;
    }
    AssetRegistry& getAssetRegistry()
    {
        return _assetRegistry;
    }
    const AssetRegistry& getAssetRegistry() const
    {
        return _assetRegistry;
    }
    RasterGpuScene& getRasterGpuScene()
    {
        return *static_cast<RasterGpuScene*>(_gpuScene.get());
    }
    const RasterGpuScene& getRasterGpuScene() const
    {
        return *static_cast<const RasterGpuScene*>(_gpuScene.get());
    }
    void addSkyBoxTexture(const RefPtr<Texture>& texture);
    void updateDescriptorSet();
    void update();

    ~SceneManager();

protected:
private:
    nvvk::DescriptorBindings     _sceneDescriptorBindings;
    std::vector<RefPtr<Texture>> _sceneSkyTexture;

    CpuScene       _cpuScene;
    std::mutex     _cpuSceneMutex;
    AssetRegistry  _assetRegistry;
    std::unique_ptr<GpuScene> _gpuScene;
};

} // namespace Play

#endif // SCENEMANAGER_H
