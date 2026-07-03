#include "SceneManager.h"
#include "Resource.h"
#include "core/runtime/VulkanRuntime.h"
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

bool isSameModelLoadRequest(ModelLoadRequestID lhs, ModelLoadRequestID rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

void applyFailedModelLoad(CpuModelComponent& component, const std::string& message)
{
    component.model           = {};
    component.firstRenderable = 0;
    component.renderableCount = INVALID_SCENE_ID;
    component.loadState       = CpuModelComponent::LoadState::eFailed;
    component.loadMessage     = message;
}

void applyRegisteredModelLoad(CpuModelComponent& component, ModelAssetID model, uint32_t renderableCount)
{
    component.model           = model;
    component.firstRenderable = 0;
    component.renderableCount = renderableCount;
    component.loadState       = CpuModelComponent::LoadState::eLoaded;
    component.loadMessage.clear();
}

struct PendingModelRegistration
{
    ModelLoadCompletion completion;
    ModelAssetID        model;
    uint32_t            renderableCount = INVALID_SCENE_ID;
};
} // namespace

SceneManager::SceneManager(GpuSceneType gpuSceneType) : _gpuScene(createGpuScene(gpuSceneType))
{
    if (_gpuScene)
    {
        _gpuScene->clear();
    }

    _sceneDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // g_SceneSkyTexture
    _sceneDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // s_SceneSkyBoxTexture
    _sceneDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr); // s_SceneVolumeFogTexture
    _sceneDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SceneTexturePoolCapacity, VK_SHADER_STAGE_ALL, nullptr,
                                        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT); // s_SceneTextures[]

    vkDriver->getDescriptorSetCache()->initSceneDescriptorSets(_sceneDescriptorBindings);
}

void SceneManager::addSkyBoxTexture(const RefPtr<Texture>& texture)
{
    _sceneSkyTexture.push_back(texture);
}

void SceneManager::updateDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> writes;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.dstBinding     = SceneTextureBinding;
    write.dstSet         = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().set;

    std::vector<VkDescriptorImageInfo> imageInfos;
    if (_gpuScene && !_gpuScene->getSceneTextures().empty())
    {
        const std::vector<RefPtr<Texture>>& sceneTextures = _gpuScene->getSceneTextures();
        write.descriptorCount = static_cast<uint32_t>(sceneTextures.size());
        imageInfos.resize(sceneTextures.size());
        for (size_t i = 0; i < sceneTextures.size(); ++i)
        {
            imageInfos[i].imageView   = sceneTextures[i]->descriptor.imageView;
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
    editAssetLoadingServer(
        [](AssetLoadingServer& loadingServer)
        {
            loadingServer.processPendingLoads();
        });

    std::vector<ModelLoadCompletion> completedModels;

    editAssetLoadingServer(
        [&](AssetLoadingServer& loadingServer)
        {
            ModelLoadCompletion completion;
            while (loadingServer.popCompletedModel(completion))
            {
                completedModels.push_back(std::move(completion));
            }
        });

    std::vector<PendingModelRegistration> pendingRegistrations;
    pendingRegistrations.reserve(completedModels.size());

    {
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        for (ModelLoadCompletion& completion : completedModels)
        {
            CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(completion.request.requester);
            if (!component || !isSameModelLoadRequest(component->request, completion.request.id))
            {
                continue;
            }

            if (completion.result.success && _gpuScene)
            {
                PendingModelRegistration pendingRegistration;
                pendingRegistration.completion = std::move(completion);
                pendingRegistrations.push_back(std::move(pendingRegistration));
                continue;
            }

            applyFailedModelLoad(*component, completion.result.message);
            _cpuScene.notifyComponentChanged();
        }
    }

    const size_t previousSceneTextureCount = _gpuScene ? _gpuScene->getSceneTextures().size() : 0;

    for (PendingModelRegistration& pendingRegistration : pendingRegistrations)
    {
        pendingRegistration.model = _gpuScene->registerModel(std::move(pendingRegistration.completion.result.model));
        pendingRegistration.renderableCount = pendingRegistration.model.isValid()
                                                  ? static_cast<uint32_t>(_gpuScene->getModels()[pendingRegistration.model.index].renderables.size())
                                                  : INVALID_SCENE_ID;
    }

    {
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        for (PendingModelRegistration& pendingRegistration : pendingRegistrations)
        {
            const ModelLoadCompletion& completion = pendingRegistration.completion;
            CpuModelComponent*         component  = _cpuScene.getComponent<CpuModelComponent>(completion.request.requester);
            if (!component || !isSameModelLoadRequest(component->request, completion.request.id))
            {
                continue;
            }

            applyRegisteredModelLoad(*component, pendingRegistration.model, pendingRegistration.renderableCount);
            _cpuScene.notifyComponentChanged();
        }

        _cpuScene.updateWorldTransforms();

        if (_gpuScene && _gpuScene->getType() != GpuSceneType::eGaussian && _gpuScene->getSourceSceneRevision() != _cpuScene.getRevision())
        {
            _gpuScene->updateTransforms(_cpuScene);
        }
    }

    if (_gpuScene && _gpuScene->getSceneTextures().size() != previousSceneTextureCount)
    {
        updateDescriptorSet();
    }
}

SceneManager::~SceneManager() = default;

} // namespace Play
