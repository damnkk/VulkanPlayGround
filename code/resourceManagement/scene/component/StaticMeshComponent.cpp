#include "StaticMeshComponent.h"
#include "core/runtime/VulkanRuntime.h"
#include "core/assets/AssetLoadingServer.h"

CEREAL_REGISTER_TYPE(Play::StaticMeshComponent)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Play::Component, Play::StaticMeshComponent)

namespace Play
{
StaticMeshComponent::~StaticMeshComponent() {}

void StaticMeshComponent::onLoad()
{
    // clang-format off
    BeginLoadAssetBind() 
    LoadAssetBind(Model, _model)
    EndLoadAssetBind
    // clang-format on
}

void StaticMeshComponent::onSave()
{
    // clang-format off
    BeginSaveAssetBind()
    SaveAssetBind(_model)
    EndSaveAssetBind
    // clang-format on
}

void StaticMeshComponent::onInit()
{
    Component::onInit();
}
void StaticMeshComponent::onUpdate(float deltaTime) {}

void StaticMeshComponent::collectDrawBatch(std::vector<DrawBatch>& batches)
{
    // DrawBatch has no mesh fields yet; resource binding and draw submission are added with the renderer path.
}
} // namespace Play
