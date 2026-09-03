#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include <mutex>
#include "core/RefCounted.h"
#include "nvvk/descriptors.hpp"
#include "resourceManagement/scene/cpu/Scene.h"
#include "resourceManagement/scene/gpu/GaussianScene.h"

namespace Play
{
class Texture;
class GPUScene;

class SceneManager
{
public:
    static constexpr uint32_t SceneTextureBinding      = 3;
    static constexpr uint32_t SceneTexturePoolCapacity = 1024;

    SceneManager();
    ~SceneManager();

    Scene& getScene()
    {
        return *_scene;
    }
    const Scene& getScene() const
    {
        return *_scene;
    }

    template <typename Fn>
    decltype(auto) readScene(Fn fn) const
    {
        std::lock_guard<std::mutex> lock(_sceneMutex);
        return fn(*_scene);
    }

    template <typename Fn>
    decltype(auto) editScene(Fn fn)
    {
        std::lock_guard<std::mutex> lock(_sceneMutex);
        return fn(*_scene);
    }

    void addSkyBoxTexture(const RefPtr<Texture>& texture);
    void updateDescriptorSet();
    void update();

    bool createProject(const std::string& projectPath, std::string* errorMessage = nullptr);
    bool saveProject(std::string* errorMessage = nullptr);
    bool loadProject(const std::string& projectPath, std::string* errorMessage = nullptr);

private:
    nvvk::DescriptorBindings     _sceneDescriptorBindings;
    std::vector<RefPtr<Texture>> _sceneSkyTexture;
    std::shared_ptr<Scene>       _scene;
    std::shared_ptr<GPUScene>    _gpuScene = nullptr;
    mutable std::mutex           _sceneMutex;
};

} // namespace Play

#endif // SCENEMANAGER_H
