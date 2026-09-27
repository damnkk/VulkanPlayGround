#include "StaticMeshComponent.h"
#include "core/runtime/VulkanRuntime.h"
#include "core/assets/AssetLoadingServer.h"
#include "resourceManagement/scene/SceneManager.h"
#include "TransformComponent.h"

CEREAL_REGISTER_TYPE(Play::StaticMeshComponent)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Play::Component, Play::StaticMeshComponent)

namespace Play
{
StaticMeshComponent::~StaticMeshComponent()
{
    releaseGpuRanges();
}

void StaticMeshComponent::setModel(ModelRef model)
{
    if (_model == model) return;
    _model = std::move(model);
    _materialArray.clear();
    registerModel();
}

ModelRef StaticMeshComponent::getModel() const
{
    return _model;
}

void StaticMeshComponent::setMaterialInstance(uint32_t drawableID, MaterialInstance* material)
{
    if (_materialArray.at(drawableID) == material) return;
    _materialArray.at(drawableID) = material;
    if (_drawCommands.empty()) return;
    _drawCommands.at(drawableID).materialInstanceRef = material;
    if (auto scene = _gpuScene.lock()) scene->updateDrawCommands(_drawCommands);
}

void StaticMeshComponent::setCastShadow(bool value)
{
    _castShadow = value;
}

bool StaticMeshComponent::getCastShadow() const
{
    return _castShadow;
}

void StaticMeshComponent::setInstanceCount(int32_t count)
{
    assert(count >= 0);
    if (count < 0 || size_t(count) == _instanceDatas.size()) return;
    _instanceDatas.resize(static_cast<size_t>(count));
    _dirtyInstanceFirst = 0;
    _dirtyInstanceEnd   = static_cast<uint32_t>(count);
    _drawsDirty         = true;
}

uint32_t StaticMeshComponent::getInstanceCount() const
{
    return static_cast<uint32_t>(_instanceDatas.size());
}

void StaticMeshComponent::reserveInstances(uint32_t capacity)
{
    _instanceDatas.reserve(capacity);
}

const InstanceData& StaticMeshComponent::getInstanceData(uint32_t index) const
{
    return _instanceDatas.at(index);
}

void StaticMeshComponent::setInstanceData(uint32_t index, const InstanceData& data)
{
    _instanceDatas.at(index) = data;
    markInstanceRangeDirty(index, 1);
}

void StaticMeshComponent::addInstance(InstanceData&& instanceData)
{
    const uint32_t index = getInstanceCount();
    _instanceDatas.push_back(std::move(instanceData));
    markInstanceRangeDirty(index, 1);
    _drawsDirty = true;
}

void StaticMeshComponent::updateInstanceState(const std::function<void(size_t, InstanceData&)>& func)
{
    updateInstanceRange(0, getInstanceCount(), func);
}

void StaticMeshComponent::updateInstanceRange(uint32_t first, uint32_t count, const std::function<void(size_t, InstanceData&)>& func)
{
    assert(first <= getInstanceCount() && count <= getInstanceCount() - first);
    if (!func) return;
    for (uint32_t index = first; index < first + count; ++index) func(index, _instanceDatas[index]);
    markInstanceRangeDirty(first, count);
}

void StaticMeshComponent::onLoad()
{
    // clang-format off
    BeginLoadAssetBind() 
    LoadAssetBind(Model, _model)
    EndLoadAssetBind
    // clang-format on
    registerModel();
}

void StaticMeshComponent::onSave()
{
    // clang-format off
    BeginSaveAssetBind()
    SaveAssetBind(_model)
    EndSaveAssetBind
    // clang-format on
}

void StaticMeshComponent::onEnterScene()
{
    registerModel();
}

void StaticMeshComponent::onExitScene()
{
    releaseGpuRanges();
}

void StaticMeshComponent::onUpdate(float deltaTime)
{
    auto scene = _gpuScene.lock();
    if (!scene || _modelID == GpuScene::InvalidID || _drawCommands.empty()) return;

    glm::mat4 worldFromComponent(1.0f);
    for (auto entity = getEntity(); entity; entity = entity->getFather().lock())
    {
        if (auto transform = entity->TryGetComponent<TransformComponent>()) worldFromComponent = transform->getTransform() * worldFromComponent;
    }
    if (worldFromComponent != _worldFromComponent)
    {
        _worldFromComponent = worldFromComponent;
        markInstanceRangeDirty(0, getInstanceCount());
    }

    if (getInstanceCount() > _instanceRange.capacity)
    {
        scene->releaseInstances(_instanceRange);
        _instanceRange = scene->allocateInstances(getInstanceCount());
        _drawsDirty    = true;
        markInstanceRangeDirty(0, getInstanceCount());
    }
    if (_dirtyInstanceFirst < _dirtyInstanceEnd)
    {
        std::vector<GpuSceneInstanceData> data(_dirtyInstanceEnd - _dirtyInstanceFirst);
        for (uint32_t i = _dirtyInstanceFirst; i < _dirtyInstanceEnd; ++i)
        {
            auto& gpu          = data[i - _dirtyInstanceFirst];
            gpu.worldFromModel = _worldFromComponent * _instanceDatas[i].transform;
            gpu.modelFromWorld = glm::inverse(gpu.worldFromModel);
            gpu.customData     = _instanceDatas[i].customData;
        }
        scene->updateInstances(_instanceRange, _dirtyInstanceFirst, static_cast<uint32_t>(data.size()), data.data());
    }
    _dirtyInstanceFirst = GpuScene::InvalidID;
    _dirtyInstanceEnd   = 0;
    if (_drawsDirty)
    {
        std::vector<GpuSceneDrawData> data(_drawCommands.size());
        for (uint32_t i = 0; i < data.size(); ++i) data[i] = {_modelID, i, _instanceRange.offset, getInstanceCount()};
        scene->updateDraws(_drawRange, 0, static_cast<uint32_t>(data.size()), data.data());
        _drawsDirty = false;
    }
}

std::string StaticMeshComponent::getTypeName()
{
    return "Static Mesh";
}

ComponentType StaticMeshComponent::getType()
{
    return STATIC_MESH_COMPONENT;
}

void StaticMeshComponent::collectDrawBatch(std::vector<DrawBatch>& batches)
{
    for (const auto& command : _drawCommands)
    {
        auto batch = std::find_if(batches.begin(), batches.end(),
                                  [&](const DrawBatch& candidate) { return candidate.materialInstanceRef == command.materialInstanceRef; });
        if (batch == batches.end())
            batches.push_back({command.materialInstanceRef, {command}});
        else
            batch->commands.push_back(command);
    }
}

uint32_t StaticMeshComponent::getModelID() const
{
    return _modelID;
}

const std::vector<DrawCommand>& StaticMeshComponent::getDrawCommands() const
{
    return _drawCommands;
}

void StaticMeshComponent::registerModel()
{
    releaseGpuRanges();
    if (!_model) return;
    auto scene = vkDriver->getSceneManager()->getGpuScene();
    _gpuScene  = scene;
    _modelID   = scene->registerModel(_model);
    if (_modelID == GpuScene::InvalidID) return;

    const uint32_t count = static_cast<uint32_t>(_model->getDrawableInfos().size());
    _materialArray.resize(count, nullptr);
    auto entity = getEntity();
    if (!entity || !entity->getScene()) return;
    _drawRange = scene->allocateDraws(count);
    _drawCommands.resize(count);
    for (uint32_t drawableID = 0; drawableID < count; ++drawableID)
    {
        _drawCommands[drawableID] = {_drawRange.offset + drawableID, _modelID, drawableID, _materialArray[drawableID]};
    }
    scene->updateDrawCommands(_drawCommands);
    _drawsDirty = true;
    markInstanceRangeDirty(0, getInstanceCount());
}

void StaticMeshComponent::releaseGpuRanges()
{
    if (auto scene = _gpuScene.lock())
    {
        scene->releaseDraws(_drawRange);
        scene->releaseInstances(_instanceRange);
    }
    _drawRange     = {};
    _instanceRange = {};
    _drawCommands.clear();
    _modelID = GpuScene::InvalidID;
}

void StaticMeshComponent::markInstanceRangeDirty(uint32_t first, uint32_t count)
{
    if (count == 0) return;
    _dirtyInstanceFirst = std::min(_dirtyInstanceFirst, first);
    _dirtyInstanceEnd   = std::max(_dirtyInstanceEnd, first + count);
}

} // namespace Play
