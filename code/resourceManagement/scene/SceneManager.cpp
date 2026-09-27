#include "SceneManager.h"

#include "core/Profiling.h"
#include "core/ProjectPaths.h"
#include "core/runtime/VulkanRuntime.h"
#include "resourceManagement/vulkan/descriptors/DescriptorManager.h"
#include "resourceManagement/vulkan/resources/Resource.h"

namespace Play
{

SceneManager::SceneManager() : _gpuScene(std::make_shared<GpuScene>())
{
    _sceneDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _sceneDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _sceneDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SceneTexturePoolCapacity, VK_SHADER_STAGE_ALL, nullptr,
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);

    vkDriver->getDescriptorSetCache()->initSceneDescriptorSets(_sceneDescriptorBindings);
}

SceneManager::~SceneManager() = default;

void SceneManager::addSkyBoxTexture(const RefPtr<Texture>& texture)
{
    _sceneSkyTexture.push_back(texture);
}

void SceneManager::updateDescriptorSet()
{
    PLAY_PROFILE_SCOPE("SceneManager::updateDescriptorSet");
    auto* cache = vkDriver->getDescriptorSetCache();
    if (cache->getSceneDescriptorSetCount() != vkDriver->getFrameCycleSize()) cache->initSceneDescriptorSets(_sceneDescriptorBindings);
    const VkDescriptorSet set = cache->getSceneDescriptorSet().set;
    for (uint32_t binding = 0; binding < _sceneSkyTexture.size() && binding < SceneTextureBinding; ++binding)
    {
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = set;
        write.dstBinding      = binding;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo      = &_sceneSkyTexture[binding]->descriptor;
        vkUpdateDescriptorSets(vkDriver->getDevice(), 1, &write, 0, nullptr);
    }
    _gpuScene->updateTextureDescriptors(set);
}

void SceneManager::update()
{
    PLAY_PROFILE_SCOPE("SceneManager::update");
    std::lock_guard<std::mutex> lock(_sceneMutex);
    _scene->tick(static_cast<float>(vkDriver->getDeltaTime()));
    _gpuScene->prepareFrame();
    updateDescriptorSet();
}

bool SceneManager::createProject(const std::string& projectPath, std::string* errorMessage)
{
    if (!ProjectInfo::setProjectPath(std::filesystem::u8path(projectPath)) || !vkDriver->getAssetManager()->Init())
    {
        if (errorMessage) *errorMessage = "Could not initialize the new project.";
        return false;
    }

    _scene = vkDriver->getAssetManager()->createAsset<Scene>(ProjectInfo::getProjectName());
    if (!_scene)
    {
        if (errorMessage) *errorMessage = "Could not create the startup scene asset.";
        return false;
    }
    return saveProject(errorMessage);
}

bool SceneManager::saveProject(std::string* errorMessage)
{
    if (!ProjectInfo::isOpen() || !_scene || !vkDriver || !vkDriver->getAssetManager())
    {
        if (errorMessage) *errorMessage = "No project is currently open.";
        return false;
    }

    std::lock_guard<std::mutex> lock(_sceneMutex);
    if (!vkDriver->getAssetManager()->saveAsset(_scene))
    {
        if (errorMessage) *errorMessage = "Could not save the scene or asset manifest.";
        return false;
    }

    std::ofstream stream(ProjectInfo::getProjectPath() / "project.json", std::ios::trunc);
    if (!stream)
    {
        if (errorMessage) *errorMessage = "Could not write project.json.";
        return false;
    }
    try
    {
        cereal::JSONOutputArchive archive(stream);
        const VUID                startupScene = _scene->getUID();
        archive(cereal::make_nvp("startupScene", startupScene));
    }
    catch (const std::exception& error)
    {
        if (errorMessage) *errorMessage = error.what();
        return false;
    }
    stream.close();
    if (stream.fail())
    {
        if (errorMessage) *errorMessage = "Could not finish writing project.json.";
        return false;
    }
    return true;
}

bool SceneManager::loadProject(const std::string& projectPath, std::string* errorMessage)
{
    if (!ProjectInfo::setProjectPath(std::filesystem::u8path(projectPath)))
    {
        if (errorMessage) *errorMessage = "Invalid project directory.";
        return false;
    }

    VUID startupScene;
    try
    {
        std::ifstream stream(ProjectInfo::getProjectPath() / "project.json");
        if (!stream)
        {
            if (errorMessage) *errorMessage = "Could not read project.json. Open a project saved in the current format.";
            return false;
        }
        cereal::JSONInputArchive archive(stream);
        archive(cereal::make_nvp("startupScene", startupScene));

        auto* assets = vkDriver->getAssetManager();
        if (startupScene.is_nil() || !assets->Init())
        {
            if (errorMessage) *errorMessage = "Could not restore the project assets or startup scene ID.";
            return false;
        }
        auto scene = assets->getOrLoadAsset<Scene>(startupScene);
        if (!scene)
        {
            if (errorMessage) *errorMessage = "The project's startup scene could not be loaded.";
            return false;
        }
        std::lock_guard<std::mutex> lock(_sceneMutex);
        _scene = std::move(scene);
    }
    catch (const std::exception& error)
    {
        if (errorMessage) *errorMessage = error.what();
        return false;
    }
    return true;
}

} // namespace Play
