#include "SceneManager.h"
#include "resourceManagement/vulkan/resources/Resource.h"
#include "core/Profiling.h"
#include "core/runtime/VulkanRuntime.h"
#include "resourceManagement/vulkan/descriptors/DescriptorManager.h"

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

void applyCpuLoadedModel(CpuModelComponent& component)
{
    component.model           = {};
    component.firstRenderable = 0;
    component.renderableCount = INVALID_SCENE_ID;
    component.loadState       = CpuModelComponent::LoadState::eCpuLoaded;
    component.loadMessage.clear();
}

void applyUploadingModelLoad(CpuModelComponent& component)
{
    component.model           = {};
    component.firstRenderable = 0;
    component.renderableCount = INVALID_SCENE_ID;
    component.loadState       = CpuModelComponent::LoadState::eUploading;
    component.loadMessage.clear();
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
    ModelGpuUploadCompletion completion;
    ModelAssetID             model;
    uint32_t                 renderableCount = INVALID_SCENE_ID;
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
    PLAY_PROFILE_SCOPE("SceneManager::updateDescriptorSet");

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
    PLAY_PROFILE_SCOPE("SceneManager::update");

    {
        PLAY_PROFILE_SCOPE("SceneManager::process pending model loads");
        _assetLoadingServer.processPendingLoads();
    }

    {
        PLAY_PROFILE_SCOPE("SceneManager::process pending model uploads");
        _assetLoadingServer.processPendingUploads();
    }

    std::vector<ModelLoadCompletion> completedModels;
    std::vector<ModelGpuUploadCompletion> completedModelUploads;

    {
        PLAY_PROFILE_SCOPE("SceneManager::collect completed model loads");
        ModelLoadCompletion completion;
        while (_assetLoadingServer.popCompletedModel(completion))
        {
            completedModels.push_back(std::move(completion));
        }
    }

    {
        PLAY_PROFILE_SCOPE("SceneManager::collect completed model uploads");
        ModelGpuUploadCompletion completion;
        while (_assetLoadingServer.popCompletedModelUpload(completion))
        {
            completedModelUploads.push_back(std::move(completion));
        }
    }

    {
        PLAY_PROFILE_SCOPE("SceneManager::apply completed CPU model loads");
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        for (ModelLoadCompletion& completion : completedModels)
        {
            CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(completion.request.requester);
            if (!component || !isSameModelLoadRequest(component->request, completion.request.id))
            {
                continue;
            }

            if (completion.result.success && completion.result.model)
            {
                if (completion.request.uploadPolicy == AssetUploadPolicy::eUploadToGpu)
                {
                    applyUploadingModelLoad(*component);
                }
                else
                {
                    applyCpuLoadedModel(*component);
                }
                _cpuScene.notifyComponentChanged();
                continue;
            }

            applyFailedModelLoad(*component, completion.result.message);
            _cpuScene.notifyComponentChanged();
        }
    }

    std::vector<PendingModelRegistration> pendingRegistrations;
    pendingRegistrations.reserve(completedModelUploads.size());

    {
        PLAY_PROFILE_SCOPE("SceneManager::prepare completed model registrations");
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        for (ModelGpuUploadCompletion& completion : completedModelUploads)
        {
            CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(completion.request.requester);
            if (!component || !isSameModelLoadRequest(component->request, completion.request.id))
            {
                continue;
            }

            if (completion.success && _gpuScene)
            {
                PendingModelRegistration pendingRegistration;
                pendingRegistration.completion = std::move(completion);
                pendingRegistrations.push_back(std::move(pendingRegistration));
                continue;
            }

            applyFailedModelLoad(*component, completion.message.empty() ? "Model GPU upload failed." : completion.message);
            _cpuScene.notifyComponentChanged();
        }
    }

    const size_t previousSceneTextureCount = _gpuScene ? _gpuScene->getSceneTextures().size() : 0;

    {
        PLAY_PROFILE_SCOPE("SceneManager::register completed model uploads");
        for (PendingModelRegistration& pendingRegistration : pendingRegistrations)
        {
            pendingRegistration.model = _gpuScene->registerModel(std::move(pendingRegistration.completion.model));
            pendingRegistration.renderableCount = pendingRegistration.model.isValid()
                                                      ? static_cast<uint32_t>(
                                                            _gpuScene->getModels()[pendingRegistration.model.index].renderables.size())
                                                      : INVALID_SCENE_ID;
        }
    }

    {
        PLAY_PROFILE_SCOPE("SceneManager::apply completed model registrations");
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
        for (const PendingModelRegistration& pendingRegistration : pendingRegistrations)
        {
            const ModelGpuUploadCompletion& completion = pendingRegistration.completion;
            CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(completion.request.requester);
            if (!component || !isSameModelLoadRequest(component->request, completion.request.id))
            {
                continue;
            }

            if (!pendingRegistration.model.isValid())
            {
                applyFailedModelLoad(*component, "Could not register uploaded model in the GPU scene.");
            }
            else
            {
                applyRegisteredModelLoad(*component, pendingRegistration.model, pendingRegistration.renderableCount);
            }
            _cpuScene.notifyComponentChanged();
        }
    }

    {
        PLAY_PROFILE_SCOPE("SceneManager::update loaded model scene state");
        std::lock_guard<std::mutex> lock(_cpuSceneMutex);
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
