#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include "resourceManagement/assets/AssetLoadingServer.h"
#include "filesystem"
#include "nvvk/descriptors.hpp"
#include "resourceManagement/scene/gpu/GaussianScene.h"
#include "resourceManagement/scene/cpu/CpuScene.h"
#include "resourceManagement/scene/cpu/CpuSceneSerialization.h"
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
    template <typename Fn>
    decltype(auto) readSceneGraph(Fn fn) const
    {
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        return fn(_cpuScene);
    }
    template <typename Fn>
    decltype(auto) editSceneGraph(Fn fn)
    {
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        return fn(_cpuScene);
    }
    template <typename Fn>
    decltype(auto) readProjectAssets(Fn fn) const
    {
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        return fn(_projectAssets);
    }
    template <typename Fn>
    decltype(auto) editProjectAssets(Fn fn)
    {
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        return fn(_projectAssets);
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
    bool createProject(const std::string& projectPath, std::string* errorMessage = nullptr);
    bool saveProject(std::string* errorMessage = nullptr) const;
    bool loadProject(const std::string& projectPath, std::string* errorMessage = nullptr);

    ~SceneManager();

protected:
private:
    nvvk::DescriptorBindings     _sceneDescriptorBindings;
    std::vector<RefPtr<Texture>> _sceneSkyTexture;

    CpuScene                  _cpuScene;
    ProjectAssetTable         _projectAssets;
    std::string               _projectPath;
    mutable std::mutex        _cpuSceneMutex;
    AssetLoadingServer        _assetLoadingServer;
    std::unique_ptr<GpuScene> _gpuScene;
};

} // namespace Play

#endif // SCENEMANAGER_H
