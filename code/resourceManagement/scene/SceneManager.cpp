#include "SceneManager.h"
#include "core/ProjectPaths.h"
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

std::string normalizeAssetPath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return {};
    }

    std::error_code       errorCode;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, errorCode);
    if (errorCode)
    {
        return path.lexically_normal().generic_string();
    }

    std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
    return errorCode ? absolutePath.lexically_normal().generic_string() : normalizedPath.generic_string();
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

bool SceneManager::createProject(const std::string& projectPath, std::string* errorMessage)
{
    std::string archivePath = projectPath;
    if (std::filesystem::path(archivePath).extension().empty())
    {
        archivePath += ".project";
    }

    if (!createProjectArchive(archivePath, errorMessage))
    {
        return false;
    }

    if (!ProjectInfo::setProjectPath(archivePath))
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

    std::lock_guard<std::mutex> lock(_cpuSceneMutex);
    _cpuScene.clear();
    _projectAssets.clear();
    _projectPath = archivePath;
    _assetLoadingServer.clear();
    return true;
}

bool SceneManager::saveProject(std::string* errorMessage)
{
    std::lock_guard<std::mutex> lock(_cpuSceneMutex);
    if (_projectPath.empty())
    {
        if (errorMessage)
        {
            *errorMessage = "No project is currently open.";
        }
        return false;
    }

    if (!registerUntrackedModelAssetsLocked(errorMessage))
    {
        return false;
    }

    return saveProjectArchive(_cpuScene, _projectAssets, _projectPath, errorMessage);
}

bool SceneManager::loadProject(const std::string& projectPath, std::string* errorMessage)
{
    std::lock_guard<std::mutex> lock(_cpuSceneMutex);
    if (!loadProjectArchive(_cpuScene, _projectAssets, projectPath, errorMessage))
    {
        return false;
    }

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

    // A loaded JSON scene creates new component IDs. Invalidate outstanding source-file
    // load completions so they cannot be matched to the new CPU scene by stale IDs.
    _projectPath = projectPath;
    _assetLoadingServer.clear();
    queueProjectModelLoadsLocked();
    return true;
}

bool SceneManager::loadModelIntoNode(CpuSceneNodeID nodeID, const std::string& sourcePath)
{
    if (sourcePath.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(_cpuSceneMutex);
    CpuSceneNode* node = _cpuScene.getNode(nodeID);
    if (!node || node->type != CpuSceneNodeType::eNode3D)
    {
        return false;
    }

    CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(nodeID);
    if (!component)
    {
        component = _cpuScene.addComponent<CpuModelComponent>(nodeID);
    }
    if (!component)
    {
        return false;
    }

    std::string assetGuid;
    if (!ensureProjectAssetLocked(ProjectAssetType::eModel, sourcePath, assetGuid))
    {
        return false;
    }

    component->assetGuid = assetGuid;
    const vpgloader::ModelLoadOptions loadingOptions = component->loadingOptions;
    return component->requestLoadFromFile(_cpuScene, _assetLoadingServer, sourcePath, loadingOptions).isValid();
}

bool SceneManager::ensureProjectAssetLocked(ProjectAssetType type, const std::string& sourcePath, std::string& assetGuid)
{
    const std::string normalizedPath = normalizeAssetPath(std::filesystem::path(sourcePath));
    if (normalizedPath.empty())
    {
        return false;
    }

    for (const ProjectAssetRecord& asset : _projectAssets.getRecords())
    {
        if (asset.type == type && normalizeAssetPath(std::filesystem::path(resolveProjectAssetPathLocked(asset))) == normalizedPath)
        {
            assetGuid = asset.guid;
            return true;
        }
    }

    ProjectAssetRecord asset;
    do
    {
        asset.guid = generateProjectAssetGuid();
    } while (_projectAssets.find(asset.guid));
    asset.sourcePath = normalizedPath;
    asset.type       = type;

    if (!_projectAssets.set(asset))
    {
        return false;
    }

    assetGuid = asset.guid;
    return true;
}

bool SceneManager::registerUntrackedModelAssetsLocked(std::string* errorMessage)
{
    const std::vector<CpuSceneNode>& nodes = _cpuScene.getNodes();
    for (uint32_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const CpuSceneNode& node = nodes[nodeIndex];
        if (!node.alive)
        {
            continue;
        }

        CpuSceneNodeID nodeID;
        nodeID.index      = nodeIndex;
        nodeID.generation = node.generation;
        CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(nodeID);
        if (!component)
        {
            continue;
        }

        const ProjectAssetRecord* asset = component->assetGuid.empty() ? nullptr : _projectAssets.find(component->assetGuid);
        if (asset && asset->type == ProjectAssetType::eModel)
        {
            continue;
        }

        if (component->sourcePath.empty())
        {
            // An empty model component has no persistent model reference and is not
            // emitted by the scene serializer.
            continue;
        }

        if (!ensureProjectAssetLocked(ProjectAssetType::eModel, component->sourcePath, component->assetGuid))
        {
            if (errorMessage)
            {
                *errorMessage = "Could not register model asset for scene node '" + node.name + "'.";
            }
            return false;
        }
    }

    return true;
}

std::string SceneManager::resolveProjectAssetPathLocked(const ProjectAssetRecord& asset) const
{
    std::filesystem::path path(asset.sourcePath);
    if (path.is_relative() && !_projectPath.empty())
    {
        path = std::filesystem::path(_projectPath) / path;
    }

    return normalizeAssetPath(path);
}

void SceneManager::queueProjectModelLoadsLocked()
{
    const std::vector<CpuSceneNode>& nodes = _cpuScene.getNodes();
    for (uint32_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const CpuSceneNode& node = nodes[nodeIndex];
        if (!node.alive)
        {
            continue;
        }

        CpuSceneNodeID nodeID;
        nodeID.index      = nodeIndex;
        nodeID.generation = node.generation;
        CpuModelComponent* component = _cpuScene.getComponent<CpuModelComponent>(nodeID);
        if (!component || component->assetGuid.empty())
        {
            continue;
        }

        const ProjectAssetRecord* asset = _projectAssets.find(component->assetGuid);
        if (!asset || asset->type != ProjectAssetType::eModel)
        {
            applyFailedModelLoad(*component, "Model component references an unknown project asset.");
            _cpuScene.notifyComponentChanged();
            continue;
        }

        const std::string sourcePath = resolveProjectAssetPathLocked(*asset);
        if (!component->requestLoadFromFile(_cpuScene, _assetLoadingServer, sourcePath, component->loadingOptions).isValid())
        {
            applyFailedModelLoad(*component, "Could not queue model asset load.");
        }
    }
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

            if (completion.model)
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

            applyFailedModelLoad(*component, completion.message);
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
                                                            _gpuScene->getModelRenderables()[pendingRegistration.model.index].size())
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
