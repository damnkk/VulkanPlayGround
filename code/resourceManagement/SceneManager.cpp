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
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SceneTexturePoolCapacity, VK_SHADER_STAGE_ALL, nullptr,
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT); // s_SceneTextures[]

    vkDriver->getDescriptorSetCache()->initSceneDescriptorSets(_sceneDescriptorBindings);
}

template <typename T>
SceneManager& SceneManager::addScene(std::filesystem::path filename)
{
    if (typeid(T) == typeid(GaussianScene))
    {
        _gaussianScene.load(filename);
    }
    else if (typeid(T) == typeid(nvvkgltf::Scene))
    {
        nvvkgltf::Scene* cpuScene = nullptr;
        {
            std::lock_guard<std::mutex> lock(_sceneMutex);
            cpuScene = &_scenes.emplace_back();
        }
        cpuScene->load(filename);
        RenderScene* vkScene = nullptr;
        {
            std::lock_guard<std::mutex> lock(_scenesVkMutex);
            vkScene = &_scenesVk.emplace_back();
        }
        vkScene->init(PlayResourceManager::GetAsAllocator(), PlayResourceManager::GetAsSamplerPool());
        auto cmd = PlayResourceManager::Instance().getTempCommandBuffer();
        vkScene->create(cmd, *PlayResourceManager::Instance().GetAsStagingUploader(), *cpuScene, true, false);
        PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
        {
            std::lock_guard<std::mutex> lock(_scenesVkMutex);
            vkScene->setTextureOffset(static_cast<uint32_t>(_sceneImages.size()));
            _sceneImages.insert(_sceneImages.end(), vkScene->textures().begin(), vkScene->textures().end());
            vkScene->fillDefaultMaterials(*cpuScene);
        }
        {
            std::lock_guard<std::mutex> lock(_cpuSceneMutex);
            const std::string           modelName = filename.stem().string();
            ModelAssetID                modelID   = _assetRegistry.registerModel(modelName, filename);
            CpuSceneNodeID              nodeID    = _cpuScene.create3DNode(modelName, _cpuScene.rootNode());
            CpuModelComponent*          modelComponent = _cpuScene.addComponent<CpuModelComponent>(nodeID);
            if (modelComponent)
            {
                modelComponent->model = modelID;
            }
            _cpuScene.updateWorldTransforms();
            _rasterGpuScene.rebuild(_cpuScene, _assetRegistry);
        }
    }
    return *this;
}

template <typename T>
SceneManager& SceneManager::addScenes(std::vector<std::filesystem::path> filenames)
{
    nvutils::parallel_batches<8>(filenames.size(), [&](size_t i) { addScene<T>(filenames[i]); }, 4);
    return *this;
}

void SceneManager::addSkyBoxTexture(const RefPtr<Texture>& texture)
{
    _sceneSkyTexture.push_back(texture);
}

void SceneManager::updateDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> writes;

    const std::vector<RefPtr<Texture>>& rasterBindlessTextures = _rasterGpuScene.getBindlessTextures();

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.dstBinding     = SceneTextureBinding;
    write.dstSet         = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;

    std::vector<VkDescriptorImageInfo> imageInfos;
    if (!rasterBindlessTextures.empty())
    {
        write.descriptorCount = static_cast<uint32_t>(rasterBindlessTextures.size());
        imageInfos.resize(rasterBindlessTextures.size());
        for (size_t i = 0; i < rasterBindlessTextures.size(); ++i)
        {
            imageInfos[i].imageView   = rasterBindlessTextures[i]->descriptor.imageView;
            imageInfos[i].sampler     = VK_NULL_HANDLE;
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }
    else
    {
        write.descriptorCount = static_cast<uint32_t>(_sceneImages.size());
        imageInfos.resize(_sceneImages.size());
        for (size_t i = 0; i < _sceneImages.size(); ++i)
        {
            imageInfos[i].imageView   = _sceneImages[i].descriptor.imageView;
            imageInfos[i].sampler     = VK_NULL_HANDLE;
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }
    write.pImageInfo = imageInfos.data();
    if (!imageInfos.empty()) writes.push_back(write);

    VkWriteDescriptorSet skyWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    skyWrite.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    skyWrite.descriptorCount = static_cast<uint32_t>(_sceneSkyTexture.size());
    skyWrite.dstBinding      = 0;
    skyWrite.dstSet          = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;
    std::vector<VkDescriptorImageInfo> skyImageInfos(_sceneSkyTexture.size());
    for (size_t i = 0; i < _sceneSkyTexture.size(); ++i)
    {
        skyImageInfos[i].imageView   = _sceneSkyTexture[i]->descriptor.imageView;
        skyImageInfos[i].sampler     = VK_NULL_HANDLE;
        skyImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    skyWrite.pImageInfo = skyImageInfos.data();
    if (!_sceneSkyTexture.empty()) writes.push_back(skyWrite);

    vkUpdateDescriptorSets(vkDriver->getDevice(), writes.size(), writes.data(), 0, nullptr);
}

void SceneManager::update()
{
    std::lock_guard<std::mutex> lock(_cpuSceneMutex);
    _cpuScene.updateWorldTransforms();
    if (_rasterGpuScene.getSourceSceneRevision() != _cpuScene.getRevision())
    {
        const size_t previousBindlessTextureCount = _rasterGpuScene.getBindlessTextures().size();
        _rasterGpuScene.rebuild(_cpuScene, _assetRegistry);
        if (_rasterGpuScene.getBindlessTextures().size() != previousBindlessTextureCount)
        {
            updateDescriptorSet();
        }
    }
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

template SceneManager& SceneManager::addScene<nvvkgltf::Scene>(std::filesystem::path);
template SceneManager& SceneManager::addScenes<nvvkgltf::Scene>(std::vector<std::filesystem::path>);
template SceneManager& SceneManager::addScene<GaussianScene>(std::filesystem::path);
template SceneManager& SceneManager::addScenes<GaussianScene>(std::vector<std::filesystem::path>);

} // namespace Play
