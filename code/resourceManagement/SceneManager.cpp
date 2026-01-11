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
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024, VK_SHADER_STAGE_ALL, nullptr,
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT); // s_SceneVolumeFogTexture

    vkDriver->getDescriptorSetCache()->initSceneDescriptorSets(_sceneDescriptorBindings);
}

SceneManager& SceneManager::addScene(std::filesystem::path filename)
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
    vkScene->create(cmd, *PlayResourceManager::Instance().GetAsStagingUploader(), *cpuScene, true);
    PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
    {
        std::lock_guard<std::mutex> lock(_scenesVkMutex);
        vkScene->setTextureOffset(static_cast<uint32_t>(_sceneImages.size()));
        _sceneImages.insert(_sceneImages.end(), vkScene->textures().begin(), vkScene->textures().end());
    }
    return *this;
}

SceneManager& SceneManager::addScenes(std::vector<std::filesystem::path> filenames)
{
    nvutils::parallel_batches<8>(filenames.size(), [&](size_t i) { addScene(filenames[i]); }, 4);
    return *this;
}

void SceneManager::addSkyBoxTexture(Texture* texture)
{
    _sceneSkyTexture.push_back(texture);
}

void SceneManager::updateDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> writes;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.descriptorCount = static_cast<uint32_t>(_sceneImages.size());
    write.dstBinding      = 3;
    write.dstSet          = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;
    std::vector<VkDescriptorImageInfo> imageInfos(_sceneImages.size());
    for (size_t i = 0; i < _sceneImages.size(); ++i)
    {
        imageInfos[i].imageView   = _sceneImages[i].descriptor.imageView;
        imageInfos[i].sampler     = VK_NULL_HANDLE;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    write.pImageInfo = imageInfos.data();
    if (!_sceneImages.empty()) writes.push_back(write);

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

    vkUpdateDescriptorSets(vkDriver->_device, writes.size(), writes.data(), 0, nullptr);
}

void SceneManager::update() {}

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