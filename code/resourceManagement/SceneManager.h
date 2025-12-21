#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H
#include "filesystem"
#include "nvvkgltf/scene.hpp"
#include "nvvkgltf/scene_vk.hpp"
#include "nvvkgltf/scene_rtx.hpp"
#include "nvvk/descriptors.hpp"
namespace Play
{
class PlayElement;
class SceneManager
{
public:
    SceneManager();
    void addScene(std::filesystem::path filename);
    void addScenes(std::vector<std::filesystem::path> filenames);

    ~SceneManager();

protected:
private:
    std::vector<nvvkgltf::Scene>    _scenes; // for cpu
    std::mutex                      _sceneMutex;
    std::vector<nvvkgltf::SceneVk>  _scenesVk; // for vulkan gpu
    std::mutex                      _scenesVkMutex;
    std::vector<nvvkgltf::SceneRtx> _scenesRTX; // for ray tracing gpu
    std::mutex                      _scenesRTXMutex;
    nvvk::DescriptorBindings        _sceneDescriptorBindings;
};

} // namespace Play

#endif // SCENEMANAGER_H