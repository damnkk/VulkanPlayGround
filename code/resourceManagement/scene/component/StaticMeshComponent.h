#ifndef STATICMESHCOMPONENT_H
#define STATICMESHCOMPONENT_H
#include <functional>
#include "renderer/Drawable.h"
#include "Component.h"
#include "resourceManagement/Model.h"
#include "resourceManagement/scene/gpu/GpuScene.h"
#include "core/assets/Asset.h"

namespace Play
{
class MaterialInstance;
struct InstanceData
{
    glm::mat4 transform  = glm::mat4(1.0f); // Component-local model instance transform.
    glm::vec4 customData = glm::vec4(0.0f);
};
class StaticMeshComponent : public Play::Component, public AssetBinder, public Drawable
{
public:
    StaticMeshComponent() = default;
    ~StaticMeshComponent() override;

    // Model, materials and rendering properties.
    void              setModel(ModelRef model);
    ModelRef          getModel() const;
    void              setMaterialInstance(uint32_t drawableID, MaterialInstance* material);
    MaterialInstance* getMaterialInstance(uint32_t drawableID) const;
    void              setCastShadow(bool value);
    bool              getCastShadow() const;

    // Instance data.
    void     setInstanceCount(int32_t count);
    uint32_t getInstanceCount() const;
    // Reserves CPU storage without changing the live count or GPU allocation.
    void                reserveInstances(uint32_t capacity);
    const InstanceData& getInstanceData(uint32_t index) const;
    void                setInstanceData(uint32_t index, const InstanceData& data);
    void                addInstance(InstanceData&& instanceData);
    // Edits all instances and marks their data for upload.
    void updateInstanceState(const std::function<void(size_t, InstanceData&)>& func);
    void updateInstanceRange(uint32_t first, uint32_t count, const std::function<void(size_t, InstanceData&)>& func);

    // Lifecycle.
    void onLoad() override;
    void onSave() override;
    void onEnterScene() override;
    void onExitScene() override;
    void onUpdate(float deltaTime) override;

    // Type information and renderer integration.
    std::string                     getTypeName() override;
    ComponentType                   getType() override;
    void                            collectDrawBatch(std::vector<DrawBatch>& batches) override;
    uint32_t                        getModelID() const;
    const std::vector<DrawCommand>& getDrawCommands() const;

    RTTR_ENABLE(Component)

private:
    // GPU registration and synchronization.
    void registerModel();
    void releaseGpuRanges();
    void markInstanceRangeDirty(uint32_t first, uint32_t count);

    // Component configuration and instance data.
    ModelRef                       _model;
    std::vector<MaterialInstance*> _materialArray;
    std::vector<InstanceData>      _instanceDatas{InstanceData{}};
    bool                           _castShadow = true;

    // GPU registration and update state.
    std::weak_ptr<GpuScene>  _gpuScene;
    std::vector<DrawCommand> _drawCommands;
    GpuSceneRange            _instanceRange;
    GpuSceneRange            _drawRange;
    uint32_t                 _modelID            = GpuScene::InvalidID;
    uint32_t                 _dirtyInstanceFirst = 0;
    uint32_t                 _dirtyInstanceEnd   = 1;
    bool                     _drawsDirty         = true;
    glm::mat4                _worldFromComponent = glm::mat4(1.0f);

    // clang-format off
    BeginSerailize() 
    SerailizeBaseClass(Component) 
    SerailizeBaseClass(AssetBinder) 
    SerailizeEntry(_castShadow) 
    EndSerailize
    // clang-format on
};

} // namespace Play

#endif // STATICMESHCOMPONENT_H
