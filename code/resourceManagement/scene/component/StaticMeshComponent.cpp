#include "StaticMeshComponent.h"
#include "core/runtime/VulkanRuntime.h"
#include "core/assets/AssetLoadingServer.h"

CEREAL_REGISTER_TYPE(Play::StaticMeshComponent)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Play::Component, Play::StaticMeshComponent)

namespace Play
{
StaticMeshComponent::~StaticMeshComponent() = default;

void StaticMeshComponent::onLoad()
{
    const auto iter = _assetMap.find("_model");
    setModel(iter != _assetMap.end() && !iter->second.is_nil()
                 ? vkDriver->getAssetManager()->getOrLoadAsset<Model>(iter->second)
                 : nullptr);
}

void StaticMeshComponent::onSave()
{
    _assetMap["_model"] = _model ? _model->getUID() : VUID{};
}

void StaticMeshComponent::onInit() {}
void StaticMeshComponent::onUpdate(float deltaTime) {}

void StaticMeshComponent::collectDrawBatch(std::vector<DrawBatch>& batches)
{
    // DrawBatch has no mesh fields yet; resource binding and draw submission are added with the renderer path.
}
} // namespace Play
