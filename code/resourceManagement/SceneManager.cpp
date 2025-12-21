#include "SceneManager.h"
#include "nvutils/parallel_work.hpp"
#include "Resource.h"
#include "VulkanDriver.h"
#include "DescriptorManager.h"
namespace Play
{

SceneManager::SceneManager()
{
    _sceneDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // g_SceneSkyTexture
    _sceneDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // s_SceneSkyBoxTexture
    _sceneDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // s_SceneVolumeFogTexture
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr);         // s_SceneVolumeFogTexture
    vkDriver->getDescriptorSetCache()->initSceneDescriptorSets(_sceneDescriptorBindings);
}

void SceneManager::addScene(std::filesystem::path filename)
{
    nvvkgltf::Scene* cpuScene = nullptr;
    {
        std::lock_guard<std::mutex> lock(_sceneMutex);
        cpuScene = &_scenes.emplace_back();
    }
    cpuScene->load(filename);
    nvvkgltf::SceneVk* vkScene = nullptr;
    {
        std::lock_guard<std::mutex> lock(_scenesVkMutex);
        vkScene = &_scenesVk.emplace_back();
    }
    vkScene->init(PlayResourceManager::GetAsAllocator(), PlayResourceManager::GetAsSamplerPool());
    auto cmd = PlayResourceManager::Instance().getTempCommandBuffer();
    vkScene->create(cmd, *PlayResourceManager::Instance().GetAsStagingUploader(), *cpuScene, true);
    PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
}

void SceneManager::addScenes(std::vector<std::filesystem::path> filenames)
{
    nvutils::parallel_batches<8>(filenames.size(), [&](size_t i) { addScene(filenames[i]); }, 4);
}

SceneManager::~SceneManager()
{
    for (auto& scene : _scenes)
    {
        scene.destroy();
    }
    for (auto& vkScene : _scenesVk)
    {
        vkScene.deinit();
    }
    for (auto& rtScene : _scenesRTX)
    {
        rtScene.deinit();
    }
}

} // namespace Play