#include "SceneManager.h"
#include "nvutils/parallel_work.hpp"
#include "Resource.h"
#include "VulkanDriver.h"
#include "DescriptorManager.h"

namespace Play
{

namespace
{
std::unique_ptr<GpuScene> createGpuScene(GpuSceneType type)
{
    switch (type)
    {
        case GpuSceneType::eGaussian:
            return std::make_unique<GaussianScene>();
        case GpuSceneType::eRayTracing:
            return std::make_unique<RayTracingGpuScene>();
        case GpuSceneType::eRaster:
        default:
            return std::make_unique<RasterGpuScene>();
    }
}

RasterGpuScene* getRasterScene(GpuScene* gpuScene)
{
    if (!gpuScene || gpuScene->getType() != GpuSceneType::eRaster)
    {
        return nullptr;
    }
    return static_cast<RasterGpuScene*>(gpuScene);
}

const RasterGpuScene* getRasterScene(const GpuScene* gpuScene)
{
    if (!gpuScene || gpuScene->getType() != GpuSceneType::eRaster)
    {
        return nullptr;
    }
    return static_cast<const RasterGpuScene*>(gpuScene);
}
} // namespace

SceneManager::SceneManager(GpuSceneType gpuSceneType) : _gpuScene(createGpuScene(gpuSceneType))
{
    if (_gpuScene)
    {
        _gpuScene->clear();
        if (_gpuScene->getType() != GpuSceneType::eGaussian)
        {
            _gpuScene->rebuild(_cpuScene, _assetRegistry);
        }
    }

    _sceneDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // g_SceneSkyTexture
    _sceneDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // s_SceneSkyBoxTexture
    _sceneDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // s_SceneVolumeFogTexture
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SceneTexturePoolCapacity, VK_SHADER_STAGE_ALL, nullptr,
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT); // s_SceneTextures[]

    vkDriver->getDescriptorSetCache()->initSceneDescriptorSets(_sceneDescriptorBindings);
}

ModelLoadResult SceneManager::loadModel(const std::filesystem::path& filename, const ModelLoadingConfig& loadingCfg)
{
    std::lock_guard<std::mutex> lock(_cpuSceneMutex);
    ModelLoadResult            result = loadModelFromFile(filename, loadingCfg, _assetRegistry, _cpuScene);
    if (!result.success)
    {
        return result;
    }

    _cpuScene.updateWorldTransforms();
    if (_gpuScene && _gpuScene->getType() != GpuSceneType::eGaussian)
    {
        RasterGpuScene* rasterScene = getRasterScene(_gpuScene.get());
        const size_t    previousBindlessTextureCount = rasterScene ? rasterScene->getBindlessTextures().size() : 0;
        _gpuScene->rebuild(_cpuScene, _assetRegistry);
        if (rasterScene && rasterScene->getBindlessTextures().size() != previousBindlessTextureCount)
        {
            updateDescriptorSet();
        }
    }
    return result;
}

template <typename T>
SceneManager& SceneManager::addScene(std::filesystem::path filename)
{
    if (typeid(T) == typeid(GaussianScene))
    {
        getGaussianScene().load(filename);
    }
    else
    {
        loadModel(filename);
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

    const RasterGpuScene* rasterScene = getRasterScene(_gpuScene.get());

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.dstBinding     = SceneTextureBinding;
    write.dstSet         = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;

    std::vector<VkDescriptorImageInfo> imageInfos;
    if (rasterScene && !rasterScene->getBindlessTextures().empty())
    {
        const std::vector<RefPtr<Texture>>& rasterBindlessTextures = rasterScene->getBindlessTextures();
        write.descriptorCount = static_cast<uint32_t>(rasterBindlessTextures.size());
        imageInfos.resize(rasterBindlessTextures.size());
        for (size_t i = 0; i < rasterBindlessTextures.size(); ++i)
        {
            imageInfos[i].imageView   = rasterBindlessTextures[i]->descriptor.imageView;
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
    if (_gpuScene && _gpuScene->getType() != GpuSceneType::eGaussian && _gpuScene->getSourceSceneRevision() != _cpuScene.getRevision())
    {
        RasterGpuScene* rasterScene = getRasterScene(_gpuScene.get());
        const size_t    previousBindlessTextureCount = rasterScene ? rasterScene->getBindlessTextures().size() : 0;
        _gpuScene->rebuild(_cpuScene, _assetRegistry);
        if (rasterScene && rasterScene->getBindlessTextures().size() != previousBindlessTextureCount)
        {
            updateDescriptorSet();
        }
    }
}

SceneManager::~SceneManager() = default;

template SceneManager& SceneManager::addScene<GaussianScene>(std::filesystem::path);
template SceneManager& SceneManager::addScenes<GaussianScene>(std::vector<std::filesystem::path>);

} // namespace Play
