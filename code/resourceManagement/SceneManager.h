#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include "AssetLoadingServer.h"
#include "filesystem"
#include "nvvk/descriptors.hpp"
#include "PlayScene.h"
#include "CpuScene.h"
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
    AssetLoadingServer& getAssetLoadingServer()
    {
        return _assetLoadingServer;
    }
    const AssetLoadingServer& getAssetLoadingServer() const
    {
        return _assetLoadingServer;
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
    AssetLoadingServer _assetLoadingServer;
    std::unique_ptr<GpuScene> _gpuScene;
};

} // namespace Play

#endif // SCENEMANAGER_H
