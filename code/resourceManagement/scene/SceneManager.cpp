#include "SceneManager.h"

#include "core/Profiling.h"
#include "core/ProjectPaths.h"
#include "core/runtime/VulkanRuntime.h"
#include "resourceManagement/vulkan/descriptors/DescriptorManager.h"
#include "resourceManagement/vulkan/resources/Resource.h"

namespace Play
{

namespace
{

bool initializeProject(const std::string& projectPath, std::string* errorMessage)
{
    if (!ProjectInfo::setProjectPath(projectPath))
    {
        if (errorMessage)
        {
            *errorMessage = "Could not initialize the project information.";
        }
        return false;
    }

    if (vkDriver && vkDriver->getAssetManager() && !vkDriver->getAssetManager()->Init())
    {
        if (errorMessage)
        {
            *errorMessage = "Could not initialize the project asset map.";
        }
        return false;
    }

    return true;
}
} // namespace

SceneManager::SceneManager() : _scene(std::make_shared<Scene>("Scene"))
{
    _sceneDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _sceneDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _sceneDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SceneTexturePoolCapacity, VK_SHADER_STAGE_ALL, nullptr,
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);

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
}

void SceneManager::update()
{
    PLAY_PROFILE_SCOPE("SceneManager::update");
    std::lock_guard<std::mutex> lock(_sceneMutex);
    _scene->tick(static_cast<float>(vkDriver->getDeltaTime()));
}

bool SceneManager::createProject(const std::string& projectPath, std::string* errorMessage)
{
    std::string normalizedPath = projectPath;
    if (std::filesystem::path(normalizedPath).extension().empty())
    {
        normalizedPath += ".project";
    }

    if (!initializeProject(normalizedPath, errorMessage))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(_sceneMutex);
    _scene = std::make_shared<Scene>(std::filesystem::path(normalizedPath).stem().string());
    return true;
}

bool SceneManager::saveProject(std::string* errorMessage)
{
    if (!ProjectInfo::isOpen() || !vkDriver || !vkDriver->getAssetManager())
    {
        if (errorMessage)
        {
            *errorMessage = "No project is currently open.";
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(_sceneMutex);
    vkDriver->getAssetManager()->saveAsset(_scene);
    return true;
}

bool SceneManager::loadProject(const std::string& projectPath, std::string* errorMessage)
{
    if (!initializeProject(projectPath, errorMessage))
    {
        return false;
    }

    std::shared_ptr<Scene> loadedScene;
    if (vkDriver && vkDriver->getAssetManager())
    {
        for (const auto& [guid, asset] : vkDriver->getAssetManager()->getAssets())
        {
            if (asset && asset->getAssetType() == ASSET_TYPE_SCENE)
            {
                loadedScene = std::dynamic_pointer_cast<Scene>(asset);
                break;
            }
        }
    }

    std::lock_guard<std::mutex> lock(_sceneMutex);
    _scene = loadedScene ? loadedScene : std::make_shared<Scene>(std::filesystem::path(projectPath).stem().string());
    return true;
}

} // namespace Play
