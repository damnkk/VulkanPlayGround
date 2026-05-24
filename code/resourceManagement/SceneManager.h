#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include "filesystem"
#include "nvvkgltf/scene.hpp"
#include "nvvkgltf/scene_vk.hpp"
#include "nvvkgltf/scene_rtx.hpp"
#include "nvvk/descriptors.hpp"
#include "PlayScene.h"
#include "CpuScene.h"
#include "SceneAssets.h"
#include "RasterGpuScene.h"
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

    SceneManager();
    template <typename T>
    SceneManager& addScene(std::filesystem::path filename);
    template <typename T>
    SceneManager&                 addScenes(std::vector<std::filesystem::path> filenames);
    std::vector<nvvkgltf::Scene>& getCpuScene()
    {
        return _scenes;
    }
    std::vector<RenderScene>& getVkScene()
    {
        return _scenesVk;
    }
    std::vector<RTScene>& getRtxScene()
    {
        return _scenesRTX;
    }
    GaussianScene& getGaussianScene()
    {
        return _gaussianScene;
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
        return _rasterGpuScene;
    }
    const RasterGpuScene& getRasterGpuScene() const
    {
        return _rasterGpuScene;
    }
    void addSkyBoxTexture(const RefPtr<Texture>& texture);
    void updateDescriptorSet();
    void update();

    ~SceneManager();

protected:
private:
    std::vector<nvvkgltf::Scene> _scenes; // for cpu
    std::mutex                   _sceneMutex;
    std::vector<RenderScene>     _scenesVk; // for vulkan gpu
    std::mutex                   _scenesVkMutex;
    std::vector<RTScene>         _scenesRTX; // for ray tracing gpu
    std::mutex                   _scenesRTXMutex;
    std::vector<nvvk::Image>     _sceneImages; // all scene images
    nvvk::DescriptorBindings     _sceneDescriptorBindings;
    std::vector<RefPtr<Texture>> _sceneSkyTexture;

    CpuScene       _cpuScene;
    std::mutex     _cpuSceneMutex;
    AssetRegistry  _assetRegistry;
    RasterGpuScene _rasterGpuScene;

    // for gaussian scene
    GaussianScene _gaussianScene;
};

} // namespace Play

#endif // SCENEMANAGER_H
